#include <stdio.h>
#include <pthread.h>
#include <unistd.h>
#include <sys/syscall.h>
#include <errno.h>
#include <sys/time.h>
#include <time.h>
#include <stdlib.h>

#ifdef USE_FUTEX
#include <linux/futex.h>
#include <syscall.h>
#endif

#ifndef LOOPS
#define LOOPS 10000000
#endif

long long int a[LOOPS];
long long int iter = 0;

#ifdef USE_SPINLOCK
pthread_spinlock_t spinlock;
#else
#ifdef USE_FUTEX
int futex_addr = 0;

int futex_wait(void* addr, int val1){
	  return syscall(SYS_futex,&futex_addr,val1, NULL, NULL, 0);
}
int futex_wake(void* addr, int n){
	  return syscall(SYS_futex, addr, FUTEX_WAKE, n, NULL, NULL, 0);
}
#else
pthread_mutex_t mutex;
#endif
#endif

pid_t gettid() { return syscall( __NR_gettid ); }

void *consumer(void *ptr)
{
	long long int i;
	int id = *(int*)ptr;
	printf("Consumer TID %lu\n", (unsigned long)gettid());

	while (1)
	{
#ifdef USE_SPINLOCK
		pthread_spin_lock(&spinlock);
#else
#ifdef USE_FUTEX
		futex_wait(&futex_addr,id);
#else
		pthread_mutex_lock(&mutex);
#endif
#endif

		a[iter] = a[iter]*2;
		iter++;

#ifdef USE_SPINLOCK
		pthread_spin_unlock(&spinlock);
#else
#ifdef USE_FUTEX
		futex_wake(&futex_addr, (id+1)%2);//Program intended for 2 threads only
#else
		pthread_mutex_unlock(&mutex);
#endif
#endif

		if(iter >= LOOPS)
			break;
	}

	return NULL;
}

int main()
{
    long long int i;
    pthread_t t1, t2;
    struct timeval tv1, tv2;

    srand(time(0));

#ifdef USE_SPINLOCK
    pthread_spin_init(&spinlock, 0);
#else
#ifdef USE_FUTEX
#else
    pthread_mutex_init(&mutex, NULL);
#endif
#endif

    // Creating the list content...
    for (i = 0; i < LOOPS; i++)
        a[i] = rand();

    // Measuring time before starting the threads...
    gettimeofday(&tv1, NULL);

    int thread0_id = 0, thread1_id=1;
    pthread_create(&t1, NULL, consumer, (void*)&thread0_id);
    pthread_create(&t2, NULL, consumer, (void*)&thread1_id);

    pthread_join(t1, NULL);
    pthread_join(t2, NULL);

    // Measuring time after threads finished...
    gettimeofday(&tv2, NULL);

    if (tv1.tv_usec > tv2.tv_usec)
    {
        tv2.tv_sec--;
        tv2.tv_usec += 1000000;
    }

    printf("Result - %ld.%ld\n", tv2.tv_sec - tv1.tv_sec,
        tv2.tv_usec - tv1.tv_usec);

#ifdef USE_SPINLOCK
    pthread_spin_destroy(&spinlock);
#else
#ifdef USE_FUTEX
#else
    pthread_mutex_destroy(&mutex);
#endif
#endif

    return 0;
}


