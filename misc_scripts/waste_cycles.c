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

	if(compile_opt)
		while(1);
	else
		complete_waste();

	return 0;
}
