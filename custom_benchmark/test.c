#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <omp.h>
#include <math.h>
#include <unistd.h>

#define fori for(int i=0;i<vector_length; i++)

void compute_square_root(int vector_length, long loops){
	int a[vector_length], b[vector_length] , c[vector_length];
	double sum = 0;
	for(int i=0;i<vector_length; i++){
		a[i] = b[i] = i*6;
	}
	while(loops--)
	{
		for(int i=0;i<vector_length;i++){
			c[i] = sqrt(a[i])+sqrt(b[i]);
		}
		for(int i=0;i<vector_length; i++){
			a[i] = a[i]*a[i]*a[i];
			b[i] = a[i]+b[i];
		}
	}
	fori
		sum += c[i];
}  

void sum_loop(unsigned long vector_length, unsigned long run, long loops, unsigned long sleep){
	int a[vector_length], b[vector_length];

	fori
	a[i] = b[i] = i*6;

	while(loops--){
		for(int j=0;j<run;j++)
		{
			fori
			b[i] = a[i]+b[i];

			fori
			{
				b[i] = b[i]/a[i];
				a[i] = a[i]*b[i];
			}
		}

		for(int k=0;k<sleep; k++){
			usleep(1);
		}
	}
	fori
		b[0] += b[i];
}

int main(int argc, char **argv){
	int num_threads = 1;
	long loops = 100000;
	unsigned long vector_length = 16;
	unsigned long run = 1000;

	if(argc>1){
		sscanf(argv[1], "%d", &num_threads);
		sscanf(argv[2], "%ld", &loops);
		sscanf(argv[3], "%lu", &run);
		omp_set_num_threads(num_threads);
	}

#pragma omp parallel
	{
		int id = omp_get_thread_num();
		num_threads = omp_get_num_threads();
		if(id > num_threads)//*0.80)
			sum_loop(vector_length, run/10, loops/2, 8);//30% util
		else
			sum_loop(vector_length, run, loops/10, 0);//1);//88% util
	}

	printf("Total ops=%ld\n",num_threads*run*loops/10*vector_length);
	return 0;
}
