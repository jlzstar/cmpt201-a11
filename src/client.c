#include "client.h"
#include "helper.h"
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

int main(int argc, char *argv[]) {
  if (argc != 5)
    handle_error("incorrect args");

  uint32_t ip = (uint32_t)atoi(argv[1]);
  uint16_t port = (uint16_t)atoi(argv[2]);
  uint8_t num_msgs = (uint8_t)atoi(argv[3]);
  char *lfp = argv[4];
  return 0;
}
