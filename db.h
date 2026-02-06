#ifndef DB_H
typedef struct {
  int id;
  int age;
  char name[32];
} DbEntry;

#define DB_SIZE (1UL << 16)

#define DB_H
#endif
