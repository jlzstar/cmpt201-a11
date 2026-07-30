#include "client.h"
#include "helper.h"
#include <errno.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define BUF_SIZE 1024

typedef struct {
  uint8_t type;
  uint32_t ip;
  uint16_t port;
  uint8_t data[1024];
  size_t data_len;
} msg_t;

typedef struct {
  int sfd;
  FILE *log_fp;
  int num_msgs;
  bool active;
} client_t;

void add_msg2file(FILE *fp, const char *ip, uint16_t port, const char *str) {
  fprintf(fp, "%-15s%-10u%s", ip, port, str);
}

int convert_b2str(uint8_t *buf, ssize_t buf_size, char *str, ssize_t str_size) {
  if (buf == NULL || str == NULL || buf_size <= 0 || str_size < (buf_size * 2 + 1)) {
    return -1;
  }
  for (int i = 0; i < buf_size; i++) {
    sprintf(str + i * 2, "%02X", buf[i]);
  }
  str[buf_size * 2] = '\0';
  return 0;
}

void *sender_thread(void *args) {
  client_t *c = (client_t *)args;

  for (int i = 0; i < c->num_msgs; i++) {

    uint8_t buf[10];
    char str[10 * 2 + 1];
    getentropy(buf, 10);
    if (convert(buf, sizeof(buf), str, sizeof(str)) != 0)
      handle_error("convert");
  }
  return 0;
}

void *receiver_thread(void *args) {}

int main(int argc, char *argv[]) {

  if (argc != 5)
    handle_error("incorrect args");

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

  client_t c;
  c.sfd = sfd;
  c.num_msgs = (uint8_t)atoi(argv[3]);
  c.log_fp = fopen(argv[4], "w");
  c.active = false;

  pthread_t sender_tid, receiver_tid;
  pthread_create(&sender_tid, NULL, sender_thread, &c);
  pthread_create(&receiver_tid, NULL, receiver_thread, &c);

  pthread_join(sender_tid, NULL);
  pthread_join(receiver_tid, NULL);

  fclose(c.log_fp);
  close(c.sfd);

  return 0;
}
