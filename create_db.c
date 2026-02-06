#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/mman.h>
#include <unistd.h>

#include "db.h"

int main(void) {
  // we create a *very* large file
  // and write a good amount of data in it
  // to simulate a database file
  int fd = open("loong.db", O_RDWR | O_CREAT | O_TRUNC, 0666);

  if (fd < 0) {
    perror("failed to open db file");
    return 1;
  }

  for (size_t i = 0; i < DB_SIZE; i++) {
    DbEntry e;
    e.age = (rand() % 45) + 18;
    e.id = i;

    for (size_t i = 0; i < sizeof(e.name) - 1; i++)
      e.name[i] = rand() % 26 + 'A';

    e.name[sizeof(e.name) - 1] = '\0';

    ssize_t written = write(fd, &e, sizeof(e));
    if (written < 0) {
      perror("failed to write");
      return 1;
    } else if ((size_t)written < sizeof(e)) {
      fprintf(stderr, "failed to write full data\n");
      return 1;
    }
  }

  fsync(fd);
  close(fd);

  return 0;
}
