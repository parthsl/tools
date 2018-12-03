#define _GNU_SOURCE
#include<stdio.h>
#include<stdlib.h>
#include<unistd.h>
#include<math.h>
#include<time.h>
#include<sys/time.h>
#include<omp.h>
#include <sched.h>
#include <pthread.h>
#include <errno.h>

#define N 1000000

/*
 * gcc wof_load.c -lm -fopenmp -lpthread
 * Taskset high util thread to faster cpu and monitor impact on light util thread when setting it to faster or slower cpu will not impact workload much
 * taskset -c 12,6 ./a.out
 */
static int mf = 1;

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
 *  * returns the difference between start and stop in usecs.  Negative values
 *   * are turned into 0
 *    */
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

void freq_sensitive(int *a)
{
	int sum = 0;
	struct timeval t1, t2;

	gettimeofday(&t1, NULL);

	for(int j=0;j<mf*1000;j++)
	for(int i=0;i<N;i++)sum += a[i];

	gettimeofday(&t2, NULL);

	printf("sum = %d, timespent=%llu\n",sum, tvdelta(&t1, &t2));

}

void freq_sensitive2(int *a)
{
	int sum = 0;
	struct timeval t1, t2;

	gettimeofday(&t1, NULL);

	for(int i=0;i<N;i++)a[i] = sqrt(a[i]);
	for(int i=0;i<N;i++)sum += a[i];

	gettimeofday(&t2, NULL);

	printf("sqrt sum = %d, timespent=%llu\n",sum, tvdelta(&t1, &t2));
}

void io_ops(int *a)
{
	FILE *fp = fopen("some.temp","w");
	int i;
	struct timeval t1, t2;

	gettimeofday(&t1, NULL);

	for(i=0;i<N;i++){
		fprintf(fp, "%d", a[i]);
		fseek(fp, 0,0);
		if(i%10==0)
		usleep(1);
	}

	gettimeofday(&t2, NULL);
	fclose(fp);

	printf("io ops sum = %d, timespent=%llu\n",i=N, tvdelta(&t1, &t2));
}


enum spin_const{
	P1,
	P2
};

enum spin_const spinlock = P1;

int stick_this_thread_to_core(int core_id) {
	int num_cores = sysconf(_SC_NPROCESSORS_ONLN);
	if (core_id < 0 || core_id >= num_cores)
		return EINVAL;

	cpu_set_t cpuset;
	CPU_ZERO(&cpuset);
	CPU_SET(core_id, &cpuset);

	pthread_t current_thread = pthread_self();    
	return pthread_setaffinity_np(current_thread, sizeof(cpu_set_t), &cpuset);
}

void* high_util(void *data)
{
	int sum = 0;
	int i=N/1000;
	struct timeval t1, t2;
	int* a = (int*)data;

	stick_this_thread_to_core(12);
	printf("high util cpu=%d\n",sched_getcpu());
	gettimeofday(&t1, NULL);
	while(i--)
	{
		while(spinlock!=P1);

		for(int i=0;i<N;i++)sum += a[i];

		spinlock = P2;
	}
	gettimeofday(&t2, NULL);
	printf("ops = %d, timespent=%llu\n",i=N/1000*N, tvdelta(&t1, &t2));
}

void* low_util(void *data)
{
	int i=N/1000;
	int* a = (int*)data;

	printf("low util cpu=%d\n",sched_getcpu());
	while(i--)
	{
		while(spinlock != P2){
			for(int i=0;i<10000;i++)
				if(spinlock == P2)break;
			usleep(1);
		}

		spinlock = P1;
	}
}

int main(int argc, char**argv){
	int a[N];
	int nr_threads = 2;
	pthread_t *tid;

	srand(time(NULL));
	tid = (pthread_t*)malloc(sizeof(pthread_t)*nr_threads);

	if(argc>1){
		sscanf(argv[1],"%d",&mf);
	}

	for (int i=0;i<N;i++)a[i] = rand();

	pthread_create(&tid[0],NULL, high_util, (void*)a);
	pthread_create(&tid[1],NULL, low_util, (void*)a);

	pthread_join(tid[0], NULL);
	pthread_join(tid[1], NULL);

	/*
#pragma omp parallel
	{
		int id = omp_get_thread_num();
		if(id==0)
			freq_sensitive(a);
		else 
			io_ops(a);
	}
	*/
	return 0;
}
