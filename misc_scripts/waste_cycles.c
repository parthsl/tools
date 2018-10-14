/* coppermonkey
 * This program does CPU cycles wasting.
 * It is perfect demonstration of using 
 * while(1); and asm nop ops.
 */
#include<stdio.h>

void complete_waste()
{
	while(1)
		asm volatile("nop");
}

/*
 * pass argv[1] = 1 for while(1)
 * else it will run with above fun.
 */
int main(int argc, char**argv)
{
	int compile_opt = 0;
	if(argc > 1){
		sscanf(argv[1], "%d", &compile_opt);
	}
	
	if(!compile_opt)printf("set argv[1]=1 for compiler optimization loop\nUse perf stat ./a.out to see difference in IPC\n");

	if(compile_opt)
		while(1);
	else
		complete_waste();

	return 0;
}


/*
 * GCC optimizes the while(1) loop to perform less instructions
 * One can see the difference in Instructions per Cycles.
 * The loop with while(1); will dispatch less instrcutions
 * and the asm volatile dispatches more instrcutions.
 * While keeping the cycles consumed by both is the same.
 */
