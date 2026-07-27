#ifndef HELPER_H
#define HELPER_H
#include "server.h"
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

#define handle_error(msg)                                                                          \
  do {                                                                                             \
    perror(msg);                                                                                   \
    exit(EXIT_FAILURE);                                                                            \
  } while (0)

typedef struct {
  uint8_t type;
  uint32_t ip;
  uint16_t port;
  uint8_t data[1024];
  size_t data_len;
} msg_t;

typedef struct {
  int fd;
  uint32_t ip;
  uint16_t port;
  bool active;
} client_t;

#endif
