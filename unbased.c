#include <arpa/inet.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#include "db.h"
#include "tp.h"

typedef struct {
  int connfd;
  const DbEntry *db;
} ClientReq;

void handle_client_request(void *data) {
  ClientReq req = *(ClientReq *)data;

  unsigned long addr;
  free(data);

  ssize_t bytes_read = read(req.connfd, &addr, sizeof(addr));
  if (bytes_read < 0) {
    perror("failed to read address from client");
    goto out;
  } else if ((size_t)bytes_read < sizeof(addr)) {
    fprintf(stderr, "Client provided too small a buffer\n");
    goto out;
  }

  printf("Client asked for index %lx\n", addr);

  if (addr >= DB_SIZE) {
    ssize_t bytes_written = write(req.connfd, "Get out\n", sizeof("Get out\n"));

    if (bytes_written < 0 || (size_t)bytes_written < sizeof("Get out\n"))
      perror("Failed to write full message");

    goto out;
  }

  const DbEntry *e = &req.db[addr];

  ssize_t written = write(req.connfd, e, sizeof(DbEntry));
  if (written < 0)
    perror("failed to write response");
  else if ((size_t)written < sizeof(DbEntry))
    perror("failed to write full response");
  else
    printf("Successfully responded to client\n");

out:
  if (shutdown(req.connfd, SHUT_RDWR) < 0)
    perror("failed to shutdown");
  if (close(req.connfd) < 0)
    perror("failed to close");
}

DbEntry *map_database_file(const char *file) {
  int fd = open(file, O_RDONLY);
  if (fd < 0)
    return NULL;

  DbEntry *db =
      mmap(NULL, DB_SIZE * sizeof(DbEntry), PROT_READ, MAP_SHARED, fd, 0);
  close(fd);

  return db;
}

int main(void) {
  DbEntry *db = map_database_file("loong.db");
  if (!db) {
    perror("failed to map database file");
    return 1;
  }

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

  if (bind(sockfd, (struct sockaddr *)&sock_addr, sizeof(sock_addr)) < 0) {
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
    req->db = db;

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
