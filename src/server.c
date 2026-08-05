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
pthread_mutex_t clientLock2;
pthread_mutex_t num_clientLock;
static int num_clients_connected = 0;
static int num_type1_received = 0;

typedef struct {
  int sfd;
  uint32_t ip;
  uint16_t port;
  bool active;
  bool sent_type1;
  uint8_t buf[BUF_SIZE];
  ssize_t buf_len;
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

void close_all_sfd(client_t *clients, int num_clients) {
  for (int i = 0; i < num_clients; i++) {
    if (clients[i].active == true) {
      clients[i].active = false;
      // close(clients[i].sfd);

      // drain leftover unread bytes
      char drain_buf[1000];
      ssize_t n;
      while ((n = read(clients[i].sfd, drain_buf, sizeof(drain_buf))) > 0) {
      }

      close(clients[i].sfd);
    }
  }
  return;
}

size_t s2c_msging_protocol(uint8_t *client_buf, size_t client_buf_len, uint32_t *ip_str,
                           uint16_t port, char *out_buf) {

  size_t offset = 0;
  out_buf[offset] = client_buf[0];
  offset += 1;

  memcpy(out_buf + offset, ip_str, 4);
  offset += 4;

  memcpy(out_buf + offset, &port, 2);
  offset += 2;

  memcpy(out_buf + offset, client_buf + 1, client_buf_len - 1);
  offset += client_buf_len - 1;
  return offset;
}

int send_all(int fd, void *buf, size_t buf_len) {
  char *temp = buf;
  size_t bytes_sent = 0;

  while (bytes_sent < buf_len) {
    ssize_t n = send(fd, temp + bytes_sent, buf_len - bytes_sent, MSG_NOSIGNAL);
    if (n > 0) {
      bytes_sent += n;
      continue;
    }

    if (n == -1 && errno == EAGAIN && errno == EWOULDBLOCK) {
      continue;
    } else {
      return -1;
    }
  }
  return 0;
}

bool handle_client_msg(client_t *clients, int num_clients, int sender_index) {

  client_t *c = &clients[sender_index];
  uint8_t end_msg[2] = {1, '\n'};

  for (;;) {
    // check for msg in the client's buf
    uint8_t *newLine = NULL;
    for (int i = 0; i < c->buf_len; i++) {
      if (c->buf[i] == '\n') {
        newLine = &c->buf[i];
        break;
      }
    }

    if (newLine == NULL) { // not a complete msg
      break;               // wait for next read() in main()
    }
    size_t msg_len = (newLine - c->buf) + 1; // len of the msg from start to \n
    int type = c->buf[0];

    // type 1 msg - increment type1 msg received, write type 1 msg to the sender
    if (type == 1) {
      if (c->sent_type1 == false) {
        num_type1_received++;
        c->sent_type1 = true;
        // send(c->sfd, end_msg, 2, MSG_NOSIGNAL);
      }
      // write(c->sfd, end_msg, 2);
    } else if (type == 0) {
      // type 0 msg - send it to all clients (including sender)
      // parse msg into [type][ip][port][msg][\n] format
      // send it
      char out_buf[1 + 4 + 2 + BUF_SIZE + 1];
      size_t out_buf_len = s2c_msging_protocol(c->buf, msg_len, &c->ip, c->port, out_buf);

      for (int i = 0; i < num_clients; i++) {
        if (clients[i].active == true) {
          if (send_all(clients[i].sfd, out_buf, out_buf_len) == -1)
            continue;
        }
      }
    }
    // check if server should terminate
    if (num_clients_connected == num_clients && num_type1_received >= num_clients_connected) {
      // uint8_t end_msg[2] = {1, '\n'};
      for (int i = 0; i < num_clients; i++) {
        if (clients[i].active == true) {
          if (send_all(clients[i].sfd, end_msg, 2) == -1)
            handle_error("send");
        }
      }
      return true; // server terminates
    }

    // shift leftover bytes to the front of buf
    size_t leftover_len = c->buf_len - msg_len;
    memmove(c->buf, c->buf + msg_len, leftover_len);
    c->buf_len = leftover_len;
  }
  return false;
}

int find_client_index_by_fd(client_t *clients, int num_clients, int fd) {
  for (int i = 0; i < num_clients; i++) {
    if (clients[i].sfd == fd && clients[i].active == true)
      return i;
  }
  return -1;
}

int find_free_slot(client_t *clients, int n_cl) {
  for (int i = 0; i < n_cl; i++) {
    if (clients[i].active == false)
      return i;
  }
  return -1;
}

int main(int argc, char *argv[]) {
  if (argc != 3) {
    handle_error("incorrect arguments");
  }
  uint16_t port = (uint16_t)atoi(argv[1]);
  const int NUM_CLIENTS = atoi(argv[2]);
  if (NUM_CLIENTS == 0) {
    handle_error("# clients is 0");
  }

  // printf("num clients: %d\n", NUM_CLIENTS);
  client_t clients[NUM_CLIENTS];
  memset(clients, 0, sizeof(clients));
  struct sockaddr_in client_addr;
  int sfd, cfd, epollfd;
  int nfds;
  socklen_t addrlen = sizeof(struct sockaddr_in);
  uint8_t read_buf[BUF_SIZE];
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

        int indx = find_free_slot(clients, NUM_CLIENTS);
        if (indx == -1) {
          continue;
        } else {
          clients[indx].sfd = cfd;
          clients[indx].active = true;
          clients[indx].ip = client_addr.sin_addr.s_addr;
          clients[indx].port = client_addr.sin_port;
          num_clients_connected++;
        }

      } else {
        // case 2: the incoming signal is an existing client sending data

        int indx = find_client_index_by_fd(clients, NUM_CLIENTS, events[i].data.fd);
        if (indx == -1)
          continue;

        ssize_t num_read = 0;
        while (clients[indx].buf_len < BUF_SIZE) {
          num_read = read(events[i].data.fd, clients[indx].buf + clients[indx].buf_len,
                          BUF_SIZE - clients[indx].buf_len);
          if (num_read <= 0)
            break;

          clients[indx].buf_len += num_read;

          // send the incoming msg to ALL clients (including sender)
          bool term = handle_client_msg(clients, NUM_CLIENTS, indx);
          if (term == true) {
            close_all_sfd(clients, NUM_CLIENTS);
            if (close(sfd) == -1)
              handle_error("close");
            printf("server terminates successfully");
            return 0;
          }
        }

        if (clients[indx].buf_len == BUF_SIZE)
          continue;

        if (num_read == 0) {
          // client disconnects
          epoll_ctl(epollfd, EPOLL_CTL_DEL, clients[indx].sfd, NULL);
          close(clients[indx].sfd);
          if (clients[indx].sent_type1) {
            num_type1_received--;
          }
          num_clients_connected--;
          clients[indx].active = false;

        } else if ((num_read == -1 && errno != EAGAIN && errno != EWOULDBLOCK)) {
          handle_error("read");
        }
      }
    }
  }
  return 0;
}
