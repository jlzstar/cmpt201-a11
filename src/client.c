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

  uint32_t ip = argv[1];
  uint16_t port = argv[2];

  return 0;
}
