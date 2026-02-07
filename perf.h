#ifndef PERF_H
#include <threads.h>
#include <stddef.h>
#include <stdint.h>
#include <stdatomic.h>

typedef struct {
  atomic_uint_fast64_t total_time;
  atomic_uint_fast64_t num_clients;
  uint64_t start_time;
  uint64_t end_time;
  const char *name;
} PerfData;

typedef struct {
  PerfData *data;
  uint64_t start;
} PerfInstance;

void perf_init(PerfData *data, const char *name);

PerfInstance perf_istart(PerfData *data);
void perf_iend(PerfInstance *instance);

void perf_end(PerfData *data);

#define PERF_H
#endif // PERF_H
