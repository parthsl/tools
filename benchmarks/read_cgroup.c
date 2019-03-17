#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <math.h>
#include <time.h>
#include <sys/time.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>

int nr_cgroups = 10000;

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

int main(int argc, char **argv)
{
	int* fp;
	char cg[1000];
	long long unsigned int *memlim;
	struct timeval start, now;

	if(argc>1)
		sscanf(argv[1], "%d", &nr_cgroups);

	fp = (int*)malloc(sizeof(int)*nr_cgroups);
	memlim = (long long unsigned int*)calloc(sizeof(long long unsigned int),nr_cgroups);

	for(int i=0;i<nr_cgroups; i++)
	{
		sprintf(cg,"/sys/fs/cgroup/memory/grp%d/memory.limit_in_bytes",i);
		fp[i] = open(cg, O_SYNC|O_RDWR);
	}

	for(int k=5; k>=0; k--)
	{
#ifdef READ_FUNC
		gettimeofday(&start, NULL);
		for(int i=0;i<nr_cgroups; i++)
		{
			char buf[100];
			read(fp[i], buf, 100);
			sscanf(buf, "%llu", &memlim[i]);
		}
		gettimeofday(&now, NULL);
		printf("read time = %llu\n",tvdelta(&start, &now));
#endif

#ifdef WRITE_FUNC
		gettimeofday(&start, NULL);
		for(int i=0; i<nr_cgroups; i++)
		{
			char buf[100];
			sprintf(buf,"%llu",memlim[i]+(i%2-1+1*(i%2))*1000);
			if(write(fp[i], buf, 100)==-1)
				fprintf(stderr, "Unable to write on file: grp%d\n",i);;
			//fprintf(fp[i],"%ld", memlim[i]+(i%2-1+1*(i%2))*1000);
		}
		gettimeofday(&now, NULL);
		printf("write time = %llu\n",tvdelta(&start, &now));
#endif
		
	}

	for(int i=0;i<nr_cgroups; i++)
	{
		close(fp[i]);
	}

	return 0;
}
