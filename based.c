#include <arpa/inet.h>
#include <assert.h>
#include <fcntl.h>
#include <stdatomic.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#include "config.h"
#include "db.h"
#include "perf.h"
#include "tp.h"

#define PRESENT_MASK (1)

typedef struct {
  ThreadPool *pool;
  unsigned long *ept;
} IoPoolArgs;

typedef struct {
  int connfd;
  PerfInstance response_time;
  const DbEntry *db;

  union {
    IoPoolArgs iop;
    unsigned long addr;
  };
} ClientReq;

atomic_uint_fast64_t offloads = 0;

void end_connection(ClientReq *req) {
  if (shutdown(req->connfd, SHUT_RDWR) < 0)
    perror("failed to shutdown");
  if (close(req->connfd) < 0)
    perror("failed to close");

  perf_iend(&req->response_time);
  free(req);
}

void access_post_response(void *data) {
  ClientReq *req = data;
  unsigned long addr = req->addr;

  printf("Client asked for index %lx\n", addr);

  const DbEntry *e = &req->db[addr];

  ssize_t written = write(req->connfd, e, sizeof(DbEntry));
  if (written < 0)
    perror("failed to write response");
  else if ((size_t)written < sizeof(DbEntry))
    perror("failed to write full response");
  else
    printf("Successfully responded to client\n");

  end_connection(req);
}

void handle_client_request(void *data) {
  ClientReq *req = data;

  unsigned long addr;
  ssize_t bytes_read = read(req->connfd, &addr, sizeof(addr));
  if (bytes_read < 0) {
    perror("failed to read address from client");
    goto err;
  } else if ((size_t)bytes_read < sizeof(addr)) {
    fprintf(stderr, "Client provided too small a buffer\n");
    goto err;
  }

  if (addr >= DB_SIZE) {
    ssize_t bytes_written =
        write(req->connfd, "Get out\n", sizeof("Get out\n"));

    if (bytes_written < 0 || (size_t)bytes_written < sizeof("Get out\n"))
      perror("Failed to write full message");

    goto err;
  }

  unsigned long pte = req->iop.ept[(unsigned long)(req->db + addr) >> 12];

  // only access here if we KNOW that the access will not result in a major page
  // fault
  // We do not want to have to wait. These threads are supposed to just admit clients and process their requests
  // However, by producing the response here if the client is trying to access something that is already mapped
  // we can cut down response time (hopefully)
  if (pte && (pte & PRESENT_MASK)) {
    // after here req is promoted to an access op
    req->addr = addr;
    access_post_response(req);
  } else {
    ThreadPool *iop = req->iop.pool;
    // after here req is promoted to an access op
    req->addr = addr;
    // otherwise offload the request to an IO worker
    tp_push_task(iop, access_post_response, req);
    atomic_fetch_add_explicit(&offloads, 1, memory_order_relaxed);
  }

  return;

err:
  end_connection(req);
}

DbEntry *map_database_file(const char *file) {
  int fd = open(file, O_RDONLY);
  if (fd < 0)
    return NULL;

  DbEntry *db =
      mmap(NULL, DB_SIZE * sizeof(DbEntry), PROT_READ, MAP_SHARED, fd, 0);
  close(fd);

  madvise(db, DB_SIZE * sizeof(DbEntry), MADV_NOHUGEPAGE | MADV_RANDOM);

  return db;
}

unsigned long *map_ept() {
  int fd = open("/dev/ept", O_RDONLY);
  if (fd < 0)
    return NULL;

  unsigned long *ept = mmap(NULL, 1UL << 39, PROT_READ, MAP_PRIVATE, fd, 0);
  close(fd);

  return ept;
}

void pretouch_db_area(unsigned long *ept, DbEntry *db) {
  unsigned long *first_page = ept + ((unsigned long) db >> 12);
  unsigned long db_pages = (DB_SIZE * sizeof(DbEntry)) >> 12;

  printf("Touching %lu pages before starting run\n", db_pages);
  PerfData data;
  perf_init(&data, "Pretouch");
  PerfInstance ins = perf_istart(&data);

  for (unsigned long *p = first_page; p < first_page + db_pages; p++)
     assert(*p == 0);

  perf_iend(&ins);
  perf_end(&data);

  printf("Pretouching took %lu (ns)\n", data.end_time - data.start_time);
}

int main(void) {
  unsigned long *ept = map_ept();
  if (!ept) {
    perror("failed to map exposed page table");
    return 1;
  }

  DbEntry *db = map_database_file("loong.db");
  if (!db) {
    perror("failed to map database file");
    return 1;
  }

  // touch the whole db area to avoid page faults by users on first touch
  pretouch_db_area(ept, db);

  int sockfd = socket(AF_INET, SOCK_STREAM, 0);
  if (sockfd < 0) {
    perror("failed to open socket");
    return 1;
  }

  printf("Translation for sockfd is %lx\n", ept[(unsigned long) &sockfd >> 12]);

  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int)) <
      0) {
    perror("setsockopt(SO_REUSEADDR) failed");
    close(sockfd);
    return 1;
  }

  if (setsockopt(sockfd, SOL_SOCKET, SO_REUSEPORT, &(int){1}, sizeof(int)) <
      0) {
    perror("setsockopt(SO_REUSEADDR) failed");
    close(sockfd);
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

  if (listen(sockfd, 500) < 0) {
    perror("listen failed");
    close(sockfd);
    return 1;
  }

  ThreadPool resp;
  tp_init(&resp, 24);

  ThreadPool iop;
  tp_init(&iop, 12);

  PerfData perf;

  perf_init(&perf, "Response Data");

  for (unsigned long i = 0; i < ITER; i++) {
    printf("Awaiting connection\n");
    int connfd = accept(sockfd, NULL, 0);

    PerfInstance instance = perf_istart(&perf);
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
    req->response_time = instance;
    req->iop.ept = ept;
    req->iop.pool = &iop;

    // we have out connection. Dispatch handling of the request to a different
    // thread
    if (!tp_push_task(&resp, handle_client_request, req)) {
      fprintf(stderr, "ERROR: failed to dispatch client task\n");
      close(connfd);
    }
  }

  tp_join(&resp);
  shutdown(sockfd, SHUT_RDWR);
  close(sockfd);

  perf_end(&perf);
  fflush(stdout);
  printf("Average response time was %lu (ns)\n",
         perf.total_time / perf.num_clients);
  printf("Average throughput was %lu clients/s\n",
         perf.num_clients * 1000000000 / (perf.end_time - perf.start_time));
  printf("Offloaded %ld out of %ld requests to IO pool\n", offloads, ITER);

  return 0;
}
