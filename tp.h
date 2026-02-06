#ifndef TP_H
#define TP_H
#include <threads.h>

typedef struct ThreadPool ThreadPool;

typedef void (*TCallback)(void *data);

typedef struct Task {
  TCallback callback;
  void *data;

  struct Task *next;
} Task;

typedef struct {
  Task *head;
  Task *tail;
} Tasklist;

struct ThreadPool {
  thrd_t *threads;
  size_t threads_len;

  mtx_t q_lock;
  cnd_t q_work;
  Tasklist tasks;
};

int tp_init(ThreadPool *tp, size_t thread_cnt);
int tp_push_task(ThreadPool *pool, TCallback callback, void *data);

void tp_join(ThreadPool *tp);

#endif // TP_H
