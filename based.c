#include <arpa/inet.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>

#include "tp.h"

typedef struct {
  int connfd;
} ClientReq;

void handle_client_request(void *data) {
  ClientReq req = *(ClientReq *)data;

  unsigned long addr;
  free(data);

  ssize_t bytes_read = read(req.connfd, &addr, sizeof(addr));
  if (bytes_read < 0) {
    perror("failed to read address from client");
    goto out;
  } else if ((size_t) bytes_read < sizeof(addr)) {
    fprintf(stderr, "Client provided too small a buffer\n");
    goto out;
  }

  ssize_t bytes_written = write(req.connfd, "Pong\n", sizeof("Pong\n"));

  if (bytes_written < 0 || (size_t) bytes_written < sizeof("Pong\n")) {
    perror("Failed to write full message");
  } else {
    printf("Client wants: %.16lx\n", addr);
  }

out:
  if (shutdown(req.connfd, SHUT_RDWR) < 0)
    perror("failed to shutdown");
  if (close(req.connfd) < 0)
    perror("failed to close");
}

int main(void) {
  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    perror("failed to open socket");
    return 1;
  }

  struct sockaddr_in sock_addr = {
      .sin_addr = {.s_addr = htonl(INADDR_ANY)},
      .sin_port = htons(2026),
      .sin_family = AF_INET,
  };

  if (bind(sockfd, (const struct sockaddr *)&sock_addr, sizeof(sock_addr)) <
      0) {
    close(sockfd);
    perror("failed to bind port");
    return 1;
  }

  printf("Listening on port 2026\n");

  if (listen(sockfd, 12) < 0) {
    perror("listen failed");
    close(sockfd);
    return 1;
  }

  ThreadPool tp;
  tp_init(&tp, 12);

  while (1) {
    printf("Awaiting connection\n");
    int connfd = accept(sockfd, NULL, 0);

    printf("Accepted connection\n");

    if (connfd < 0) {
      perror("failed to accept connection");
      continue;
    }

    ClientReq *req = calloc(1, sizeof(ClientReq));
    if (!req) {
      fprintf(stderr, "ERROR: failed to create client request\n");
      close(connfd);
      continue;
    }

    req->connfd = connfd;

    // we have out connection. Dispatch handling of the request to a different
    // thread
    if (!tp_push_task(&tp, handle_client_request, req)) {
      fprintf(stderr, "ERROR: failed to dispatch client task\n");
      close(connfd);
    }
  }

  tp_join(&tp);
  close(sockfd);

  return 0;
}
