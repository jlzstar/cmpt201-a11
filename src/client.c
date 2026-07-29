#include "client.h"
#include "helper.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>
#define BUF_SIZE 1024

int main(int argc, char *argv[]) {
  if (argc != 5)
    handle_error("incorrect args");

  uint32_t ip = (uint32_t)atoi(argv[1]);
  uint16_t port = (uint16_t)atoi(argv[2]);
  uint8_t num_msgs = (uint8_t)atoi(argv[3]);
  char *lfp = argv[4];

  struct sockaddr_in addr;
  ssize_t num_read;
  char buf[BUF_SIZE];
  int sfd;

  sfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sfd == -1)
    handle_error("socket");

  memset(&addr, 0, sizeof(struct sockaddr_in));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  if (inet_pton(AF_INET, argv[2], &addr.sin_addr) <= 0)
    handle_error("inet_pton");

  if (connect(sfd, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)) == -1)
    handle_error("connect");
  while ((num_read = read(STDIN_FILENO, buf, BUF_SIZE)) > 0) {
    if (write(sfd, buf, num_read) != num_read)
      handle_error("write");
  }
  if (num_read == -1)
    handle_error("read");

  exit(EXIT_SUCCESS);

  return 0;
}
