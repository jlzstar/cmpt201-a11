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

typedef struct {
  struct MsgList_t *next;
  void *data;
} MsgList_t;

typedef struct {
  struct MsgList_t *last;
  uint32_t count;
} MsgHandle_t;

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

  struct sockaddr_in remote_addr;
  int sfd, cfd, epollfd;
  int nfds;
  ssize_t num_read;
  socklen_t addrlen = sizeof(struct sockaddr_in);
  char buf[BUF_SIZE];
  struct epoll_event ev, events[NUM_CLIENTS];

  sfd = init_server_socket(port, NUM_CLIENTS);

  // init epoll
  epollfd = epoll_create1(0);
  if (epollfd == -1)
    handle_error("epoll_create1");

  ev.events = EPOLLIN | EPOLLOUT;
  ev.data.fd = sfd;
  if (epoll_ctl(epollfd, EPOLL_CTL_ADD, sfd, &ev) == -1)
    handle_error("epoll_ctl");

  for (;;) {
    nfds = epoll_wait(epollfd, events, NUM_CLIENTS, -1);
    if (nfds == -1)
      handle_error("epoll_wait");

    for (int i = 0; i < nfds; ++i) {
      if (events[i].data.fd == sfd) {
        memset(&remote_addr, 0, sizeof(struct sockaddr_in));
        cfd = accept(sfd, (struct sockaddr *)&remote_addr, &addrlen);
        if (cfd == -1)
          handle_error("accept");

      } else {
        // set up non-blocking
        int flags = fcntl(cfd, F_GETFL, 0);
        if (flags == -1)
          handle_error("fcntl");
        flags |= O_NONBLOCK;
        if (fcntl(cfd, F_SETFL, flags) == -1)
          handle_error("fcntl");

        ev.events = EPOLLIN | EPOLLOUT;
        ev.data.fd = cfd;
        if (epoll_ctl(epollfd, EPOLL_CTL_ADD, cfd, &ev) == -1)
          handle_error("epoll_ctl: conn_sock");

        // initialize clients
        clients[i].active = true;
        clients[i].sfd = cfd;
        clients[i].ip = remote_addr.sin_addr.s_addr;
        clients[i].port = remote_addr.sin_port;
        clients[i].buf_len = 0;
      }

      printf("client connected!\n");
      while ((num_read = read(events[i].data.fd, buf, BUF_SIZE)) > 0) {

        if (write(events[i].data.fd, buf, num_read) != num_read)
          handle_error("write");
        if (num_read == -1)
          handle_error("read");
      }
    }
  }
}

return 0;
}
