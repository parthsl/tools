/* coppermonkey
 * This program does CPU cycles wasting.
 * It is perfect demonstration of using 
 * while(1); and asm nop ops.
 */
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <getopt.h>
#include<stdlib.h>


long int timeout = 1*60;
int compile_opt = 0;

void complete_waste(time_t s, time_t e)
{
	while(time(NULL)-s < timeout)
		asm volatile("nop");
}

enum {
	HELP_LONG_OPT = 1,
};
char *option_string = "t:c";
static struct option long_options[] = {
	{"timeout", required_argument, 0, 't'},
	{"compile_opt", no_argument, 0, 'c'},
	{"help", no_argument, 0, HELP_LONG_OPT},
	{0, 0, 0, 0}
};

static void print_usage(void)
{
	fprintf(stderr, "wofbench usage:\n"
			"\t-t (--timeout): timeout in milliseconds (def: 60)\n"
			"\t-r (--compile_opt): Turns on execution of asm nop instructions (def:disabled)\n"
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
				sscanf(optarg,"%ld",&timeout);
				break;
			case 'c' :
				compile_opt = 1;
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

/*
 * Simple infinite loop
 */
int main(int argc, char**argv)
{
	time_t s,e;
	
	parse_options(argc, argv);
	s = time(NULL);

	if(!compile_opt)
		while(time(NULL)-s < timeout)usleep(75);
	else
		complete_waste(s,e);

	return 0;
}


/*
 * GCC optimizes the while(1) loop to perform less instructions
 * One can see the difference in Instructions per Cycles.
 * The loop with while(1); will dispatch less instrcutions
 * and the asm volatile dispatches more instrcutions.
 * While keeping the cycles consumed by both is the same.
 */
