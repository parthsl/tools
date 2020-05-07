/* 
 * @author: Parth Shah <parths1229@gmail.com>
 */
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
#define SCHED_FLAG_LATENCY_NICE	0X80

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
	__s32 sched_latency_nice;
};

struct sched_attr sattr;
__s32 latency_nice = 0;
int pid = 0;
int sched_getattr = 0;
int sched_setattr = 0;

enum {
	HELP_LONG_OPT = 1,
};
char *option_string = "gs:p:l:";
static struct option long_options[] = {
	{"pid", required_argument, 0, 'p'},
	{"get", no_argument, 0, 'g'},
	{"set", required_argument, 0, 's'},
	{"latencynice", required_argument, 0, 'l'},
	{"help", no_argument, 0, HELP_LONG_OPT},
	{0, 0, 0, 0}
};

static void print_usage(void)
{
	fprintf(stderr, "relnice -p <PID> [-g | -s | -l] \n"
			"\t-p (--pid): PID of a task\n"
			"\t-g (--get): return latency_nice value of the PID\n"
			"\t-s (--set): set latency_nice value of the PID\n"
			"\t-l (latencynice): Same as --set\n"
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
			case 'p':
				sscanf(optarg,"%d",&pid);
				break;
			case 'g':
				sched_getattr = 1;
				break;
			case 's':
			case 'l':
				sscanf(optarg, "%d", &latency_nice);
				sattr.sched_latency_nice = latency_nice;
				sattr.sched_flags |= SCHED_FLAG_LATENCY_NICE;
				sched_setattr = 1;
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
	int ret;

	sattr.size = sizeof(struct sched_attr);
	sattr.sched_flags = 0;

	parse_options(argc, argv);

	if(sched_setattr) {
		ret = syscall(SYS_sched_setattr, pid, &sattr, 0);
		if (ret)
			printf("Failed to do sched_setattr\n");
	}
	if (sched_getattr) {
		ret = syscall(SYS_sched_getattr, pid, &sattr, sizeof(struct sched_attr), 0);
		if (ret)
			printf("Failed to do sched_getattr\n");
		else
			printf("PID=%d latency_nice=%d\n",pid, sattr.sched_latency_nice);
	}

	return 0;
}

