#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <time.h>

#include "perf.h"

static uint64_t get_current_time() {
  struct timespec tp;

  if (clock_gettime(CLOCK_MONOTONIC, &tp) < 0) {
    perror("ERROR: failed to get time");
    exit(-1);
  }

  return tp.tv_nsec + tp.tv_sec * 1000000000;
}

void perf_init(PerfData *data, const char *name) {
  data->total_time = 0;
  data->name = name;
  data->num_clients = 0;
  data->start_time = 0;
}

PerfInstance perf_istart(PerfData *data) {
  if (!data->start_time)
    data->start_time = get_current_time();

  return (PerfInstance){
      .data = data,
      .start = get_current_time(),
  };
}

void perf_iend(PerfInstance *instance) {
  PerfData *data = instance->data;
  uint64_t end = get_current_time();

  atomic_fetch_add(&data->total_time, end - instance->start);
  atomic_fetch_add(&data->num_clients, 1);
  *instance = (PerfInstance){0};
}

void perf_end(PerfData *data) {
  atomic_thread_fence(memory_order_release);
  data->end_time = get_current_time();
}
