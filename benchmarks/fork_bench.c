#define _GNU_SOURCE
#include <stdio.h>
#include <sys/time.h>
#include <pthread.h>
#include <sched.h>
#include <getopt.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

static double runtime = -1;
static int nrthreads = 100;

#define NPROCS 1024
static int nr_per_cpus[NPROCS];
static int aggr_nrscheduled[NPROCS];
pthread_mutex_t lock;

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
	int cur_cpu = sched_getcpu();

	pthread_mutex_lock(&lock);
	if (nr_per_cpus[cur_cpu]) {
		fprintf(stderr,"Error: %d Threads scheduled on CPU=%d\n",++nr_per_cpus[cur_cpu], cur_cpu);
		pthread_mutex_unlock(&lock);
	} else {
		nr_per_cpus[cur_cpu]++;
	}
	pthread_mutex_unlock(&lock);
	
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

enum {
	HELP_LONG_OPT = 1,
};
char *option_string = "t:r:h";
static struct option long_options[] = {
	{"threads", required_argument, 0, 't'},
	{"runtime", required_argument, 0, 'r'},
	{"help", no_argument, 0, HELP_LONG_OPT},
	{0, 0, 0, 0}
};

static void print_usage(void)
{
	fprintf(stderr, "fork_bench usage:\n"
			"\t-r (--runtime): Execution time for the workload in sec. Using this flag imposes mutiple iteration of thread creations for runtime sec (def: -1) \n"
			"\t-t (--threads): Total threads to be spawned (def: 100)\n"
	       );
	exit(1);
}

static void parse_options(int ac, char **av)
{
	int c;

	while (1) {
		int option_index = 0;

		c = getopt_long(ac, av, option_string,
				long_options, &option_index);

		if (c == -1)
			break;

		switch(c) {
			case 'r':
				sscanf(optarg, "%lf", &runtime);	
				break;
			case 't':
				sscanf(optarg, "%d", &nrthreads);
				break;
			case '?':
			case 'h':
			case HELP_LONG_OPT:
				print_usage();
				break;
			default:
				break;
		}
	}

	if (optind < ac) {
		fprintf(stderr, "Error Extra arguments '%s'\n", av[optind]);
		exit(1);
	}
}

int main(int argc, char **argv) {
	double best_time = 1e9;
	const double start_t = get_time();
	int total_threads = 0;

	parse_options(argc, argv);
	total_threads = nrthreads;

	printf("t=%d r=%lf\n",nrthreads, runtime);
	printf("Benchmark: Create/teardown of %d threads...\n", nrthreads);
	fflush(stdout);

	do{
		memset(nr_per_cpus, 0, sizeof(int)*NPROCS);

		thread_t *threads = (thread_t*)malloc(sizeof(thread_t)*nrthreads);
		const double t0 = get_time();

		// Create all the child threads.
		for (int i = 0; i < nrthreads; ++i) {
			threads[i] = create_thread();
		}

		// Wait for all the child threads to finish.
		for (int i = 0; i < nrthreads; ++i) {
			join_thread(threads[i]);
		}

		double dt = get_time() - t0;
		if (dt < best_time) {
			best_time = dt;
		}
	}while (runtime > 0 && (get_time() - start_t < runtime));

	// Pretty print output statistics
	printf("%f us / thread\n", (best_time / (double)nrthreads) * 1000000.0);
	printf("Number of threads per CPU, {cpuid : nrscheduled}\n {");
	for(int i=0; i<NPROCS; i++) {
		if (nr_per_cpus[i])
			printf("%d : %d, ", i, nr_per_cpus[i]);
	}
	printf("}\n");
	fflush(stdout);

	return 0;
}
