#ifndef DB_H
typedef struct {
  int id;
  int age;
  char name[32];
} DbEntry;

#define DB_SIZE (1UL << 26)

#define DB_H
#endif
