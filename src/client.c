#define _DARWIN_C_SOURCE
#include "client.h"
#include "helper.h"
#include <arpa/inet.h>
#include <errno.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/un.h>
#include <unistd.h>

#define BUF_SIZE 1024
pthread_mutex_t sendLock;
pthread_mutex_t receiveLock;

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
  atomic_bool active;
} client_t;

void add_msg2file(FILE *fp, const char *ip, uint16_t port, const char *str) {
  fprintf(fp, "%-15s%-10u%s", ip, port, str);
  fprintf(fp, "\n");
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
  

  // generate n msgs
  for (int i = 0; i < c->num_msgs; i++) {
    int byte = 32;
    uint8_t buf[byte];
    char str[byte * 2 + 1];
    int res = getentropy(buf, sizeof(buf));
    if (res != 0)
      handle_error("getentropy");
    if (convert_b2str(buf, sizeof(buf), str, sizeof(str)) != 0)
      handle_error("convert");

    // build the message protocal client to server [type][data][\n]
    uint8_t out_msg[1 + sizeof(str) + 1];

    out_msg[0] = 0;
    memcpy(out_msg + 1, str, sizeof(str));
    size_t out_msg_len = 1 + strlen(str);
    out_msg[out_msg_len] = '\n';
    out_msg_len++;

    // send the byte to server one by one
    ssize_t total_msg_sent = 0;
    while (total_msg_sent < out_msg_len) {
      ssize_t n = write(c->sfd, out_msg + total_msg_sent, out_msg_len - total_msg_sent);
      if (n == -1)
        handle_error("write");
      total_msg_sent += n;
    }
  }
  // send a type 1 msg after sending all the type 0 msgs
  uint8_t end_msg[2] = {1, '\n'};
  ssize_t m = write(c->sfd, end_msg, sizeof(end_msg));
  if (m == -1)
    handle_error("write");
  atomic_store(&c->active, false);

  return NULL;
}

// receives msgs from other clients from the server
void *receiver_thread(void *args) {
  client_t *c = (client_t *)args;
  uint8_t buf[BUF_SIZE];
  size_t buf_len = 0;
  // ssize_t n = 0;

  //
  while (1) {
    // check for a message in buf
    uint8_t *newline = NULL;
    for (size_t i = 0; i < buf_len; i++) {
      if (buf[i] == '\n') {
        newline = &buf[i];
        break;
      }
    }

    // if it's not a complete msg, read from socket for more bytes
    if (newline == NULL) {
      ssize_t n = read(c->sfd, buf + buf_len, sizeof(buf) - buf_len);
      if (n == -1)
        handle_error("read");
      if (n == 0)
        break;
      buf_len += n;
      continue;
    }

    uint8_t type = buf[0];
    // type 0 msg
    if (type == 0) {
      uint32_t ip;
      uint16_t port;
      memcpy(&ip, buf + 1, 4);
      memcpy(&port, buf + 1 + 4, 2);

      size_t header_len = 1 + 4 + 2; // byte size of type, ip, port
      size_t msg_len = (newline - buf) - header_len;

      char msg[BUF_SIZE + 1];
      memcpy(msg, buf + header_len, msg_len);
      msg[msg_len] = '\0';

      char ip_final[32];
      inet_ntop(AF_INET, &ip, ip_final, sizeof(ip_final));
      uint16_t port_final = ntohs(port);

      // log and display msg
      add_msg2file(c->log_fp, (const char *)ip_final, port_final, msg);
      printf("%-15s%-10u%s", ip_final, port_final, msg);

      // type 1 msg
    } else if (type == 1) {
      atomic_store(&c->active, false);
      return NULL;
    }

    // keep leftover bytes and remove handled bytes
    size_t handled = (newline - buf) + 1;
    size_t remaining = buf_len - handled;
    memmove(buf, buf + handled, remaining);
    buf_len = remaining;
  }
  return NULL;
}

int main(int argc, char *argv[]) {

  if (argc != 5)
    handle_error("incorrect args");

  struct sockaddr_in addr;
  ssize_t num_read;
  char buf[BUF_SIZE];
  int sfd;
  int16_t port = (int16_t)atoi(argv[2]);

  sfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sfd == -1)
    handle_error("socket");

  memset(&addr, 0, sizeof(struct sockaddr_in));
  addr.sin_family = AF_INET;
  addr.sin_port = htons(port);
  if (inet_pton(AF_INET, argv[1], &addr.sin_addr) <= 0)
    handle_error("inet_pton");

  if (connect(sfd, (struct sockaddr *)&addr, sizeof(struct sockaddr_in)) == -1)
    handle_error("connect");

  client_t c;
  c.sfd = sfd;
  c.num_msgs = (int)atoi(argv[3]);
  c.log_fp = fopen(argv[4], "w");
  atomic_store(&c.active, true);

  pthread_t sender_tid, receiver_tid;
  pthread_create(&sender_tid, NULL, sender_thread, &c);
  pthread_create(&receiver_tid, NULL, receiver_thread, &c);

  pthread_join(sender_tid, NULL);
  pthread_join(receiver_tid, NULL);

  fclose(c.log_fp);
  close(c.sfd);

  return 0;
}
