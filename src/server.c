#include "server.h"
#include "helper.h"
#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/epoll.h>
#include <sys/socket.h>
#include <unistd.h>
#define BUF_SIZE 1024
#define LISTEN_BACKLOG 32
#define MAX_EVENTS 10

pthread_mutex_t clientLock;

typedef struct {
  int sfd;
  int32_t ip;
  int16_t port;
  bool active;
  uint8_t buf[BUF_SIZE];
  size_t buf_len;
} client_t;

int init_server_socket(int16_t port, int backlog) {
  struct sockaddr_in addr;

  int sfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sfd == -1)
    handle_error("socket");

  memset(&addr, 0, sizeof(struct sockaddr_in));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  addr.sin_addr.s_addr = htonl(INADDR_ANY);

  if (bind(sfd, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)) == -1)
    handle_error("bind");
  if (listen(sfd, backlog) == -1)
    handle_error("listen");
  return sfd;
}

size_t s2c_msging_protocol(char *client_buf, char *ip_str, uint16_t port, char *out_buf) {

  size_t offset = 0;
  out_buf[offset] = client_buf[0];
  offset += 1;

  memcpy(out_buf + offset, &ip_str, 4);
  offset += 4;

  memcpy(out_buf + offset, &port, 2);
  offset += 2;

  size_t msg_len = strnlen(client_buf, BUF_SIZE);
  memcpy(out_buf + offset, client_buf, msg_len);
  offset += msg_len;

  out_buf[offset] = '\n';
  offset += 1;
  return offset;
}

void send_type0_msg() { return; }

void send_type1_msg() { return; }

static int num_clients_active = 0;
static int num_clients_done = 0;

int main(int argc, char *argv[]) {
  if (argc != 3) {
    handle_error("incorrect arguments");
  }
  uint16_t port = (uint16_t)atoi(argv[1]);
  const uint8_t NUM_CLIENTS = atoi(argv[2]);
  if (NUM_CLIENTS == 0) {
    handle_error("# clients is 0");
  }

  client_t clients[NUM_CLIENTS];

  struct sockaddr_in client_addr;
  int sfd, cfd, epollfd;
  int nfds;
  ssize_t num_read;
  socklen_t addrlen = sizeof(struct sockaddr_in);
  char buf[BUF_SIZE];
  struct epoll_event ev, events[NUM_CLIENTS];

  sfd = init_server_socket(port, NUM_CLIENTS);

  // 1. create epoll
  epollfd = epoll_create1(0);
  if (epollfd == -1)
    handle_error("epoll_create1");

  // 2.
  ev.events = EPOLLIN | EPOLLOUT;
  ev.data.fd = sfd;
  if (epoll_ctl(epollfd, EPOLL_CTL_ADD, sfd, &ev) == -1)
    handle_error("epoll_ctl");

  int num_type1_msgs = 0;

  for (;;) {
    nfds = epoll_wait(epollfd, events, NUM_CLIENTS, -1);
    if (nfds == -1)
      handle_error("epoll_wait");

    for (int i = 0; i < nfds; ++i) {
      // case 1: the incoming signal is a new client trying to connect
      if (events[i].data.fd == sfd) {
        memset(&client_addr, 0, sizeof(struct sockaddr_in));
        cfd = accept(sfd, (struct sockaddr *)&client_addr, &addrlen);
        if (cfd == -1)
          handle_error("accept");

        // set up non-blocking
        int flags = fcntl(cfd, F_GETFL, 0);
        if (flags == -1)
          handle_error("fcntl");
        flags |= O_NONBLOCK;
        if (fcntl(cfd, F_SETFL, flags) == -1)
          handle_error("fcntl");

        ev.events = EPOLLIN;
        ev.data.fd = cfd;

        // add cfd into epoll watchlist
        if (epoll_ctl(epollfd, EPOLL_CTL_ADD, cfd, &ev) == -1)
          handle_error("epoll_ctl: conn_sock");

      } else {
        // case 2: the incoming signal is an existing client sending data

        ssize_t n = read(events[i].data.fd, buf, BUF_SIZE);
        while (n > 0) {

          // get sender ip and port
          char ip_str[INET_ADDRSTRLEN];
          if (inet_ntop(AF_INET, &client_addr.sin_addr, ip_str, sizeof(ip_str)) == NULL) {
            handle_error("inet_ntop");
          }

          uint16_t port_c = ntohs(client_addr.sin_port);

          printf("client connected from ip: %s, port: %u\n", ip_str, port_c);

          // message protocol
          char out_buf[BUF_SIZE + 4 + 2];
          ssize_t out_buf_len = s2c_msging_protocol(buf, ip_str, port_c, out_buf);

          // send the incoming msg to ALL clients (including sender)
          pthread_mutex_lock(&clientLock);
          for (int j = 0; j < NUM_CLIENTS; j++) {
            write(events[j].data.fd, out_buf, out_buf_len);
          }
          pthread_mutex_unlock(&clientLock);

          // check if all clients have sent a type 1 msg, and
          // determine if server should terminate itself
          if (buf[0] == '1') {
            num_type1_msgs++;
          }
          if (num_type1_msgs >= NUM_CLIENTS) {
            printf("server terminates successfully");
            return 0;
          }

          if (n == -1)
            handle_error("read"); // client disconnects
        }
      }
    }
  }
  return 0;
}
