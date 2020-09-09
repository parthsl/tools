/* Benchmark fork() calls
 * Each thread should spread across all CPUs uniformly
 *
 * $> gcc fork_bench.c -lpthread -o fork_bench
 * $> ./fork_bench -t 10 -l 2 -r 2                                                                                                                        (base) 
 * t=10 r=2.000000
 * Benchmark: Create/teardown of 10 threads...
 * 200202.798843 us / thread
 * Number of threads per CPU, {cpuid : nrscheduled}
 * {88 : 1, 100 : 1, 108 : 1, 116 : 1, 124 : 1, 132 : 1, 136 : 1, 144 : 1, 152 : 1, 164 : 1} 
 * 200192.999840 us / thread
 * Number of threads per CPU, {cpuid : nrscheduled}
 * {88 : 1, 100 : 1, 108 : 1, 124 : 1, 132 : 1, 136 : 1, 144 : 1, 152 : 1, 164 : 1, 168 : 1}
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <sys/time.h>
#include <pthread.h>
#include <sched.h>
#include <getopt.h>
#include <unistd.h>
#include <stdlib.h>
#include <string.h>

static int loop = 1;
static double runtime = 5;
static int nrthreads = 100;
static int dowork = 1;
#ifndef WSS
#define WSS 1024*8
#endif

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

	if(dowork) {
		int ws[WSS];
		volatile int sum = 0;
		FILE *fd = fopen("/dev/null", "w");
		double start_t	= get_time();
		while(get_time()-start_t < runtime) {
			for(int i=0;i<WSS; i++)
				sum += ws[i];
		}
		fprintf(fd,"%d",sum);
		fclose(fd);

	}
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
char *option_string = "t:r:hdl:";
static struct option long_options[] = {
	{"threads", required_argument, 0, 't'},
	{"runtime", required_argument, 0, 'r'},
	{"doworkd", no_argument, 0, 'd'},
	{"help", no_argument, 0, HELP_LONG_OPT},
	{"loop", required_argument, 0, 'l'},
	{0, 0, 0, 0}
};

static void print_usage(void)
{
	fprintf(stderr, "fork_bench usage:\n"
			"\t-r (--runtime): Worker threads does runtime sec of work (def: 5sec) \n"
			"\t-t (--threads): Total threads to be spawned (def: 100)\n"
			"\t-d (--dowork): Do some work after fork (def: 1, true, set 0 to false)\n"
			"\t-l (--loop): loop the program for given number of iterations (def: 1)\n"
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
			case 'l':
				sscanf(optarg, "%d", &loop);
				break;
			case 'd':
				dowork = 0;
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

		// Pretty print output statistics
		printf("%f us / thread\n", (best_time / (double)nrthreads) * 1000000.0);
		printf("Number of threads per CPU, {cpuid : nrscheduled}\n {");
		for(int i=0; i<NPROCS; i++) {
			if (nr_per_cpus[i])
				printf("%d : %d, ", i, nr_per_cpus[i]);
		}
		printf("\b\b}\n");
		fflush(stdout);

	}while (--loop);

	return 0;
}
