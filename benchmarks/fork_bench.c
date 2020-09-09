#include <stdio.h>
#include <sys/time.h>
#include <pthread.h>

double get_time() {
	double result;
	struct timeval tv;
	if (gettimeofday(&tv, NULL) == 0) {
		result = ((double)tv.tv_sec) + 0.000001 * (double)tv.tv_usec;
	} else {
		result = 0.0;
	}
	return result;
}


// POSIX thread implementation.
typedef pthread_t thread_t;

static void* thread_fun(void* arg) {
  // We do nothing here...
  (void)arg;
  return (void*)0;
}

static thread_t create_thread() {
  thread_t result;
  pthread_create(&result, (const pthread_attr_t*)0, thread_fun, (void*)0);
  return result;
}

static void join_thread(thread_t thread) {
  pthread_join(thread, (void**)0);
}

#ifndef NUM_THREADS
#define NUM_THREADS 100
#endif

static const double BENCHMARK_TIME = 5.0;

int main() {
  printf("Benchmark: Create/teardown of %d threads...\n", NUM_THREADS);
  fflush(stdout);

  double best_time = 1e9;
  const double start_t = get_time();
  while (get_time() - start_t < BENCHMARK_TIME) {
    thread_t threads[NUM_THREADS];
    const double t0 = get_time();

    // Create all the child threads.
    for (int i = 0; i < NUM_THREADS; ++i) {
      threads[i] = create_thread();
    }

    // Wait for all the child threads to finish.
    for (int i = 0; i < NUM_THREADS; ++i) {
      join_thread(threads[i]);
    }

    double dt = get_time() - t0;
    if (dt < best_time) {
      best_time = dt;
    }
  }

  printf("%f us / thread\n", (best_time / (double)NUM_THREADS) * 1000000.0);
  fflush(stdout);

  return 0;
}
