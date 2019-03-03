#define _GNU_SOURCE
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<math.h>
#include<time.h>
#include<sys/time.h>
#include <sched.h>
#include <pthread.h>
#include <errno.h>
#include <getopt.h>
#include <sys/syscall.h>
#include <signal.h>

static int nr_threads;
static int highutil_count;
static long long unsigned int array_size;
static long long unsigned timeout;
static long long unsigned int output = 0;
static int bind = 0;
pthread_mutex_t output_lock;

#ifdef Sseperate
int ltb_len = 4;
static int low_task_binds[] = {60,61,62,63};
#else
int ltb_len =16;
static int low_task_binds[] = {1,5,9,13,17,21,25,29,33,37,41,45,49,53,57,61};
#endif

int htb_len = 16;
static int high_task_binds[] = {0,4,8,12,16,20,24,28,32,36,40,44,48,52,56,60};
//static int low_task_binds[] = {16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,32,33,34,35,36,37,38,39,40,41,42,43,44,45,46,47};
//static int high_task_binds[] = {0,4,8,12};
/*
 * gcc wof_load.c -lm -fopenmp -lpthread
 * Taskset high util thread to faster cpu and monitor impact on light util thread when setting it to faster or slower cpu will not impact workload much
 * taskset -c 12,6 ./a.out
 */

void tv_copy(struct timeval* tdest, struct timeval* tsrc){
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
			fprintf(stderr, "lat_fs: tvsub shows test time ran backwards!\n");
			exit(1);
		}
	}

	/* time shouldn't go backwards!!! */
	if (tdiff->tv_usec < 0 || t1->tv_sec < t0->tv_sec) {
		tdiff->tv_sec = 0;
		tdiff->tv_usec = 0;
	}
}

/*
 * returns the difference between start and stop in usecs.  Negative values
 * are turned into 0
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

int stick_this_thread_to_cpus(int *cpumask, int cpumask_length) {
	int num_cores = sysconf(_SC_NPROCESSORS_ONLN);

	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	for(int i=0;i<cpumask_length; i++){
		if (cpumask[i] < 0 || cpumask[i] >= num_cores)
			return EINVAL;
		CPU_SET(cpumask[i], &cpuset);
	}

	pthread_t current_thread = pthread_self();    
	return pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset);
}

void handle_sigint(int sig) 
{
	pthread_exit(NULL);
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

	if(bind)
		stick_this_thread_to_cpus(low_task_binds, ltb_len);

	while(1){
		gettimeofday(&t1,NULL);
		ops = 0;
		for(int j=0;j<4*array_size; j++)
			sum += 45;
		gettimeofday(&t2,NULL);
		wall_clock = tvdelta(&t1,&t2);
		usleep(run_period-wall_clock);
		/*
		 pthread_mutex_lock(&output_lock);
		output+= array_size;
		pthread_mutex_unlock(&output_lock);
		*/
	}

	return NULL;
}



enum {
	HELP_LONG_OPT = 1,
};
char *option_string = "t:h:n:bl:";
static struct option long_options[] = {
	{"timeout", required_argument, 0, 't'},
	{"highutil", required_argument, 0, 'h'},
	{"threads", required_argument, 0, 'n'},
	{"bind", no_argument, 0, 'b'},
	{"length", required_argument, 0, 'l'},
	{"help", no_argument, 0, HELP_LONG_OPT},
	{0, 0, 0, 0}
};

static void print_usage(void)
{
	fprintf(stderr, "wofbench usage:\n"
			"\t-t (--timeout): Execution time for the workload in s (def: 10) \n"
			"\t -h (--highutil): Count of the high utilization threads (def: 1)\n"
			"\t -n (--threads): Total threads to be spawned (def: 16)\n"
			"\t -b (--bind): Bind the threads to the cpus\n"
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
			case 'l':
				htb_len = atoi(optarg);
#ifndef Sseperate
				ltb_len = htb_len;
#endif
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
	nr_threads = 1;
	array_size = 10000;
	highutil_count = 1;
	timeout = 10000000;

	parse_options(argc, argv);
	//printf("Running with array_size=%lld highutil_count=%d\n",array_size, highutil_count);

	srand(time(NULL));
	tid = (pthread_t*)malloc(sizeof(pthread_t)*nr_threads);

	signal(SIGUSR1, handle_sigint);

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
		if(tvdelta(&t1,&t2) >= timeout){
			for(int i=0; i<nr_threads; i++)
				pthread_kill(tid[i], SIGUSR1);
			break;
		}
		else
		usleep(100);
	}

	for(int i=0; i<nr_threads; i++)
		pthread_join(tid[i], NULL);

	printf("OPS=%llu\n",output);

	return 0;
}
