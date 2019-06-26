#define _GNU_SOURCE
#include <unistd.h>
#include <linux/types.h>
#include <sys/syscall.h>
#include <stdio.h>
#include <getopt.h>
#include <stdlib.h>

#define SCHED_FLAG_RECLAIM		0x02
#define SCHED_FLAG_DL_OVERRUN		0x04
#define SCHED_FLAG_KEEP_POLICY		0x08
#define SCHED_FLAG_KEEP_PARAMS		0x10
#define SCHED_FLAG_UTIL_CLAMP_MIN	0x20
#define SCHED_FLAG_UTIL_CLAMP_MAX	0x40

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
};

struct sched_attr attr;
__u64 uclamp_min, uclamp_max = 0;
int user_defined = 1;//2=min only, 3=max only, 6=both
int pid = 1;

enum {
	HELP_LONG_OPT = 1,
};
char *option_string = "b:t:p:";
static struct option long_options[] = {
	{"throttle", required_argument, 0, 't'},
	{"boost", required_argument, 0, 'b'},
	{"pid", required_argument, 0, 'p'},
	{"help", no_argument, 0, HELP_LONG_OPT},
	{0, 0, 0, 0}
};

static void print_usage(void)
{
	fprintf(stderr, "turbo_bench usage:\n"
			"\t-t (--throttle): Set util.max to throttle frequency (def: 0) \n"
			"\t-b (--boost): Set util.min to boost the frequency (def: 0) \n"
			"\t-p (--pid): PID of a task to CLAMP the util\n"
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
				sscanf(optarg,"%llu",&uclamp_max);
				user_defined *= 3;
				break;
			case 'b':
				sscanf(optarg,"%llu",&uclamp_min);
				user_defined *= 2;
				break;
			case 'p':
				sscanf(optarg,"%d",&pid);
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


int main(int argc, char** argv)
{
	__u64 uclamp_min, uclamp_max;
	int pid;
	int ret;

	parse_options(argc, argv);

	attr.size = sizeof(struct sched_attr);
	attr.sched_flags = 0;
	if (user_defined%2 != 0) {
		attr.sched_util_min = uclamp_min;
		attr.sched_flags |= SCHED_FLAG_UTIL_CLAMP_MIN;
	}
	if (user_defined%3 != 0) {
		attr.sched_util_max = uclamp_max;
		attr.sched_flags |= SCHED_FLAG_UTIL_CLAMP_MAX;
	}

	ret = syscall(SYS_sched_setattr, pid, &attr, 0);
	if (ret)
		printf("Unable to do syscall\n");

	return 0;
}

