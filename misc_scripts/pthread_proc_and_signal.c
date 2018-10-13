/* @coppermonkey
 * The program will spawn ncpus threads in a 
 * system where 50% of the pthreads will read
 * /proc/pid/stat where pid is of the parent
 * calling process. While the rest threads will
 * send SIGUSR1 signals to proc stat reading 
 * pthreads.
 */

#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <string.h>
#include <pthread.h>
#include <signal.h>
#include <fcntl.h>
#include <time.h>


/*
 * Topology defining data structs
 */
int ncpus = 1;

long int pid;

void sig_handle(int sig){
	asm volatile("nop");
}

/*
 * Worker thread spawned for each CPU
 * Worker takes care of the core they belong to 
 * and request the frequency for the same core
 * @fd_id: the index of respective fd
 */
void* worker(void *args) {
	int proc_fd;
	char parent_pid_file[1024];
	char buffer[8092];
	int id = 0;
	signal(SIGUSR1, sig_handle);

	sprintf(parent_pid_file, "/proc/%ld/stat", pid);
	
	proc_fd = open(parent_pid_file,O_RDONLY | O_SYNC);
	while(1){
		pread(proc_fd, buffer, sizeof(buffer), 0);
	}
	close(proc_fd);
}

void* worker_sig(void *args) {
	long int tid_prev = *(long int*)args;

	while(1)
	pthread_kill(tid_prev, SIGUSR1);

}

int get_nr_cpu(){
	FILE *fp = fopen("/proc/stat","r");
	char line[1000];
	int ncpus = 0;

	while(fgets(line, sizeof(line), fp) != NULL){
		if (!strncmp(line, "cpu", 3)){
			if(!strncmp(line, "cpu ", 4)) continue;
			else ncpus++;
		}
		else
			break;
	}
	fclose(fp);
	return ncpus;
}


/*
 * argv[1] should contain total threads need to be spawned
 */
int main(int argc, char** argv)
{
	unsigned long nr_threads = 1;
	pid = getpid();
	int i;
	pthread_t *tid;
	ncpus = get_nr_cpu();
	nr_threads = ncpus;
	srand(time(0));

	if(argc>1)
		sscanf(argv[1],"%lu",&nr_threads);


	tid = (pthread_t*)malloc(sizeof(pthread_t)*nr_threads);

	for(i=0;i<nr_threads; i++){
		if(i%2 == 1)
			pthread_create(&tid[i],NULL, worker_sig, (void*)&tid[i-1]);
		else
			pthread_create(&tid[i],NULL, worker, NULL);
	}
	
/*
	while(1)
	for(i=0;i<nr_threads; i++)
		pthread_kill(tid[i], SIGUSR1);

*/	while(1);

	for(i=0;i<nr_threads; i++)
	pthread_join(tid[i],NULL);

	return 0;
}

