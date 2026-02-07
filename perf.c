#include <stdatomic.h>
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
  uint64_t now = get_current_time();
  atomic_compare_exchange_strong_explicit(
      &data->start_time, &(uint_fast64_t){0}, now, memory_order_relaxed,
      memory_order_relaxed);

  return (PerfInstance){
      .data = data,
      .start = now,
  };
}

void perf_iend(PerfInstance *instance) {
  PerfData *data = instance->data;
  uint64_t end = get_current_time();

  atomic_fetch_add_explicit(&data->total_time, end - instance->start,
                            memory_order_relaxed);
  atomic_fetch_add_explicit(&data->num_clients, 1, memory_order_relaxed);
  *instance = (PerfInstance){0};
}

void perf_end(PerfData *data) {
  // prevent reordering stores from before to after here
  atomic_thread_fence(memory_order_release);
  data->end_time = get_current_time();
}
