/*
 * This program provides a synthetic workload creation for testing of TurboSched
 * feature.
 * 
 * It uses two kinds of threads:
 * 1. High util threads: high utilization threads which does important tasks and
 * should be given most resources for longer durations.
 * 2. Low util threads: These are basically jitters which are of least importance
 * from performance point of view.
 *
 * The program outputs total operations performed during workload execution by
 * the high util threads.
 *
 * Compile: gcc turbo_bench.c -o turbo_bench -lpthread
 * Test 1: turbo_bench -t 30 -h 4 -n 16
 * Spawns 16 threads: 4 high_util and 12 low_util jitters for 30 sec
 *
 * Test 2 with thread binding: turbo_bench -t 30 -h 4 -n 16 -b
 * Along with thread spawining it binds the threads as indicated in array
 * `high_task_binds` and `low_task_binds`.
 *
 *
 * Benchmarking TurboSched feature:
 * ================================
 * mkdir /sys/fs/cgroup/cpu/jitters
 * echo 1 > /sys/fs/cgroup/cpu/jitters/cpu.turbo_sched
 * ./turbo_bench -t 30 -h 10 -n 10 &
 * ./turbo_bench -t 30 -h 0 -n 10 & 
 * echo $! > /sys/fs/cgroup/cpu/jitters/cgroup.procs
 *
 * Note: For binding feature, please change the array to specific CPUs available
 * in the system, as current code is written for a system with 64CPUs.
 *
 * Author(s): Parth Shah <pshah015@linux.ibm.com>
 */
#define _GNU_SOURCE
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <time.h>
#include <sys/time.h>
#include <sched.h>
#include <pthread.h>
#include <errno.h>
#include <getopt.h>
#include <sys/syscall.h>
#include <signal.h>
#include <linux/sched.h>
#include <linux/kernel.h>
#include <linux/unistd.h>
#include <linux/types.h>
#include <sys/syscall.h>

struct sched_attr {
	__u32 size;
	__u32 sched_policy;
	__u64 sched_flags;

	/* SCHED_NORMAL, SCHED_BATCH */
	__s32 sched_nice;
	/* SCHED_FIFO, SCHED_RR */
	__u32 sched_priority;
	/* SCHED_DEADLINE */
	__u64 sched_runtime;
	__u64 sched_deadline;
	__u64 sched_period;
	__u32 sched_util_min;
	__u32 sched_util_max;
	__u32 is_jitter;
};

static int nr_threads;
static int highutil_count;
static long long unsigned int array_size;
static long long unsigned timeout;
static long long unsigned int output = 0;
static int bind = 0;
static int do_syscall = 0;
pthread_mutex_t output_lock;
struct sched_attr sattr;

/*
 * Change below array values with specific CPUs to use for thread binding.
 * Also change the htb/ltb_len = length of the array.
 */
int ltb_len =16;
static int low_task_binds[] = {1,5,9,13,17,21,25,29,33,37,41,45,49,53,57,61};
int htb_len = 16;
static int high_task_binds[] = {0,4,8,12,16,20,24,28,32,36,40,44,48,52,56,60};

void tv_copy(struct timeval* tdest, struct timeval* tsrc)
{
	tdest->tv_sec = tsrc->tv_sec;
	tdest->tv_usec = tsrc->tv_usec;
}

void tvsub(struct timeval * tdiff, struct timeval * t1, struct timeval * t0)
{
	tdiff->tv_sec = t1->tv_sec - t0->tv_sec;
	tdiff->tv_usec = t1->tv_usec - t0->tv_usec;
	if (tdiff->tv_usec < 0 && tdiff->tv_sec > 0) {
		tdiff->tv_sec--;
		tdiff->tv_usec += 1000000;
		if (tdiff->tv_usec < 0) {
			fprintf(stderr, "lat_fs: tvsub shows test time ran \
					backwards!\n"); exit(1);
		}
	}

	/* time shouldn't go backwards!!! */
	if (tdiff->tv_usec < 0 || t1->tv_sec < t0->tv_sec) {
		tdiff->tv_sec = 0;
		tdiff->tv_usec = 0;
	}
}

/*
 * returns the difference between start and stop in usecs.  Negative values are
 * turned into 0
 */
unsigned long long tvdelta(struct timeval *start, struct timeval *stop)
{
	struct timeval td;
	unsigned long long usecs;

	tvsub(&td, stop, start);
	usecs = td.tv_sec;
	usecs *= 1000000;
	usecs += td.tv_usec;
	return (usecs);
}

int stick_this_thread_to_cpus(int *cpumask, int cpumask_length)
{
	int num_cores = sysconf(_SC_NPROCESSORS_ONLN);

	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	for(int i=0;i<cpumask_length; i++){
		if (cpumask[i] < 0 || cpumask[i] >= num_cores)
			return EINVAL;
		CPU_SET(cpumask[i], &cpuset);
	}

	pthread_t current_thread = pthread_self();    
	return pthread_setaffinity_np(current_thread, sizeof(cpu_set_t),
			&cpuset); }

void handle_sigint(int sig) 
{
	pthread_exit(NULL);
}

static int kill_force = 0;
void kill_signal(int sig)
{
	kill_force = 1;
}

void* high_util(void* data)
{
	double a[array_size];
	int i = array_size/1000;
	double sum = 0;
	struct timeval t1,t2;

	for(int iter=0;iter<array_size;iter++)
		a[iter] = ((double)rand()+0.1);

	if(bind)
		stick_this_thread_to_cpus(high_task_binds, htb_len);

	gettimeofday(&t2, NULL);
	while(1)
	{
		for(int i=0;i<array_size;i++)sum += a[i];
		pthread_mutex_lock(&output_lock);
		output+= array_size;
		pthread_mutex_unlock(&output_lock);
	}


	return NULL;
}

void* low_util(void *data)
{
	long long unsigned int sum = 0;
	long long unsigned int ops = 0;
	struct timeval t1,t2;
	long long unsigned int period = 100000;
	long long unsigned int run_period = 3000;
	long long unsigned int wall_clock;
	pid_t tid = syscall(SYS_gettid);

	if(bind)
		stick_this_thread_to_cpus(low_task_binds, ltb_len);

	if (do_syscall) {
		int ret = syscall(SYS_sched_setattr, tid, &sattr, 0);
		if (ret)
			perror("Unable to set schedattr from the thread\n");
	}


	while(1){
		gettimeofday(&t1,NULL);
		ops = 0;
		for(int j=0;j<4*array_size; j++)
			sum += 45;
		gettimeofday(&t2,NULL);
		wall_clock = tvdelta(&t1,&t2);
		usleep(run_period-wall_clock);
	}

	return NULL;
}



enum {
	HELP_LONG_OPT = 1,
};
char *option_string = "t:h:n:bju";
static struct option long_options[] = {
	{"timeout", required_argument, 0, 't'},
	{"highutil", required_argument, 0, 'h'},
	{"threads", required_argument, 0, 'n'},
	{"bind", no_argument, 0, 'b'},
	{"jitterify", no_argument, 0, 'j'},
	{"uclampmax0", no_argument, 0, 'u'},
	{"help", no_argument, 0, HELP_LONG_OPT},
	{0, 0, 0, 0}
};

static void print_usage(void)
{
	fprintf(stderr, "turbo_bench usage:\n"
			"\t-t (--timeout): Execution time for the workload in s (def: 10) \n"
			"\t -n (--threads): Total threads to be spawned including high_util threads(def: 16)\n"
			"\t -h (--highutil): Count of the high utilization threads from -n:total threads (def: 1)\n"
			"\t -b (--bind): Bind the threads to the cpus as defined low_task_binds & high_task_binds (def: no) \n"
			"\t -j (--jitterify): Classify low_util threads as jitter\n" 
			"\t -u (--uclampmax0): set max uclamp to lowest=0\n"
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
			case 't':
				sscanf(optarg,"%llu",&timeout);
				timeout = timeout*1000000;
				break;
			case 'h':
				highutil_count = atoi(optarg);
				break;
			case 'n':
				nr_threads = atoi(optarg);
				break;
			case 'b':
				bind = 1;
				break;
			case 'u':
				sattr.sched_util_max = 0;
				sattr.sched_flags = 0x40;
				do_syscall = 1;
				break;
			case 'j':
				sattr.is_jitter = 1;
				sattr.sched_flags = 0x80;
				do_syscall = 1;
				break;
			case '?':
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

int main(int argc, char**argv){
	struct timeval t1,t2;
	pthread_t *tid;
	nr_threads = 16;
	array_size = 10000;
	highutil_count = 1;
	timeout = 10000000;

	parse_options(argc, argv);
	printf("Running with array_size=%lld, total threads=%d, highutil_count=%d\n",
			array_size, nr_threads, highutil_count);

	sattr.size = sizeof(struct sched_attr);

	srand(time(NULL));
	tid = (pthread_t*)malloc(sizeof(pthread_t)*nr_threads);

	signal(SIGUSR1, handle_sigint);
	signal(SIGINT, kill_signal);

	gettimeofday(&t1,NULL);
	for(int i=0; i<nr_threads; i++)
	{
		if(i < highutil_count){
			pthread_create(&tid[i], NULL, high_util, NULL);
		}
		else{
			pthread_create(&tid[i],NULL, low_util, NULL);
		}
		usleep(1000);
	}

	while(1){
		gettimeofday(&t2,NULL);
		if(tvdelta(&t1,&t2) >= timeout || kill_force){
			for(int i=0; i<nr_threads; i++)
				pthread_kill(tid[i], SIGUSR1);
			break;
		}
		else
		usleep(100);
	}

	for(int i=0; i<nr_threads; i++)
		pthread_join(tid[i], NULL);

	printf("Total Operations performed=%llu, time passed=%lld us\n",output, tvdelta(&t1,&t2));

	return 0;
}
