#include <arpa/inet.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/socket.h>
#include <unistd.h>

#include "config.h"
#include "db.h"
#include "tp.h"

void do_socket(void *arg) {
  (void)arg;
  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    perror("failed to open socket");
    return;
  }

  struct sockaddr_in sock_addr = {
      .sin_addr = {.s_addr = htonl(INADDR_ANY)},
      .sin_port = htons(2026),
      .sin_family = AF_INET,
  };

  if (connect(sockfd, (struct sockaddr *)&sock_addr, sizeof(sock_addr)) < 0) {
    perror("failed to connect to server");
    close(sockfd);
    return;
  }

  unsigned long addr = rand() % DB_SIZE;
  if (send(sockfd, &addr, sizeof(addr), 0) < 0) {
    perror("failed to send to server");
    shutdown(sockfd, SHUT_RDWR);
    close(sockfd);
    return;
  }

  DbEntry entry;
  ssize_t read_bytes = recv(sockfd, &entry, sizeof(entry), 0);
  if (read_bytes < 0) {
    perror("failed to receive data from server");
    shutdown(sockfd, SHUT_RDWR);
    close(sockfd);
    return;
  } else if ((size_t)read_bytes < sizeof(DbEntry)) {
    perror("Failed to read whole response from server\n");
    shutdown(sockfd, SHUT_RDWR);
    close(sockfd);
    return;
  }

  printf("We are %d y/o. Our id is %d and we are called %s\n", entry.age,
         entry.id, entry.name);
  shutdown(sockfd, SHUT_RDWR);
  close(sockfd);
}

int main(void) {
  ThreadPool tp;
  tp_init(&tp, 12);

  for (unsigned long i = 0; i < ITER; i++)
    tp_push_task(&tp, do_socket, NULL);

  tp_join(&tp);

  return 0;
}
