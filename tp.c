#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <threads.h>

#include "tp.h"

static int tasklist_empty(const Tasklist *tl) {
  return tl->head == tl->tail && tl->head == NULL;
}

static Task *tasklist_pop(Tasklist *tl) {
  Task *res = tl->head;

  if (!res->next) {
    tl->head = (tl->tail = NULL);
  } else {
    tl->head = res->next;
  }

  return res;
}

static void tasklist_push(Tasklist *tl, Task *task) {
  task->next = NULL;

  if (tasklist_empty(tl)) {
    tl->head = (tl->tail = task);
  } else {
    tl->tail->next = task;
    tl->tail = task;
  }
}

static int worker_loop(void *arg) {
  ThreadPool *q = arg;

  while (1) {
    mtx_lock(&q->q_lock);

    while (tasklist_empty(&q->tasks))
      cnd_wait(&q->q_work, &q->q_lock);

    // we hold q_lock here
    Task *task = tasklist_pop(&q->tasks);
    mtx_unlock(&q->q_lock);

    Task local = *task;
    free(task);

    // now we run the callback
    local.callback(local.data);
  }

  return 0;
}

int tp_init(ThreadPool *tp, size_t thread_cnt) {
  tp->threads_len = thread_cnt;
  tp->threads = calloc(thread_cnt, sizeof(thrd_t));

  if (!tp->threads)
    return 0;

  mtx_init(&tp->q_lock, mtx_plain);
  cnd_init(&tp->q_work);

  mtx_lock(&tp->q_lock);
  tp->tasks.head = NULL;
  tp->tasks.tail = NULL;
  mtx_unlock(&tp->q_lock);

  for (size_t i = 0; i < thread_cnt; i++)
    thrd_create(&tp->threads[i], worker_loop, tp);

  return 1;
}

int tp_push_task(ThreadPool *tp, TCallback callback, void *data) {
  Task *task = calloc(1, sizeof(Task));
  if (!task)
    return 0;

  task->callback = callback;
  task->data = data;

  mtx_lock(&tp->q_lock);
  tasklist_push(&tp->tasks, task);
  mtx_unlock(&tp->q_lock);
  cnd_signal(&tp->q_work);
  
  return 1;
}

void worker_shutdown(void *data) {
  free(data);
  thrd_exit(0);
}

void tp_join(ThreadPool *tp) {
  // push threads_len shutdown tasks at the end of the queue
  for (size_t i = 0; i < tp->threads_len; i++)
    tp_push_task(tp, worker_shutdown, NULL);

  for (size_t i = 0; i < tp->threads_len; i++)
    thrd_join(tp->threads[i], NULL);

  free(tp->threads);
  mtx_destroy(&tp->q_lock);
  cnd_destroy(&tp->q_work);
}
