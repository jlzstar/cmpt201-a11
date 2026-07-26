#include "server.h"
#include <arpa/inet.h>
#include <errno.h>
#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <unistd.h>

#define BUF_SIZE 1024
#define LISTEN_BACKLOG 32

#define handle_error(msg)                                                                          \
  do {                                                                                             \
    perror(msg);                                                                                   \
    exit(EXIT_FAILURE);                                                                            \
  } while (0)

typedef struct {
  uint8_t type;
  uint32_t sender_ip;
  uint16_t sender_port;
  uint8_t data[BUF_SIZE];
  size_t data_len;
} message_t;

int main(int argc, char *argv[]) {
  if (argc != 3) {
    handle_error("incorrect arguments");
  }
  uint16_t port = (uint16_t)atoi(argv[1]);
  uint8_t num_clients = atoi(argv[2]);
  if (num_clients == 0) {
    handle_error("# clients is 0");
  }

  return 0;
}
