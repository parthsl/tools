/* 
 * This program is used to do syscall from outside the program.
 *
 * Sample script to do syscall on all the tasks of some program ($bench), one
 * can use something like
 *
 * ```
 * bench="turbobench";
 * until [ `pidof $bench` ]; do :; done;
 * ps -eLf | grep "$bench -h" | grep -v "grep" | cut -d" " -f9 | while read i; do ./setattr -p $i -j; done
 * ```
 *
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
#define SCHED_FLAG_LATENCY_TOLERANCE	0X80

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

	__s32 sched_latency_tolerance;
};

struct sched_attr sattr;
__u64 uclamp_min=1024, uclamp_max = 1024;
__s32 latency_tolerance = 0;

int pid = 1;
int do_syscall = 0;

enum {
	HELP_LONG_OPT = 1,
};
char *option_string = "b:t:p:jl:";
static struct option long_options[] = {
	{"throttle", required_argument, 0, 't'},
	{"boost", required_argument, 0, 'b'},
	{"pid", required_argument, 0, 'p'},
	{"jitterify", no_argument, 0, 'j'},
	{"latency", required_argument, 0, 'l'},
	{"help", no_argument, 0, HELP_LONG_OPT},
	{0, 0, 0, 0}
};

static void print_usage(void)
{
	fprintf(stderr, "sched_setattr set usage:\n"
			"\t-t (--throttle): Set util.max to throttle frequency (def: 0) \n"
			"\t-b (--boost): Set util.min to boost the frequency (def: 0) \n"
			"\t-p (--pid): PID of a task to CLAMP the util\n"
			"\t-j (--jitter): TurboSched RFCv4 based task jiterrify\n"
			"\t-l (--latency): Latency tolerance of the task\n"
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
				sscanf(optarg,"%lu",&uclamp_max);
				sattr.sched_util_max = uclamp_max;
				sattr.sched_flags |= SCHED_FLAG_UTIL_CLAMP_MAX;
				break;
			case 'b':
				sscanf(optarg,"%lu",&uclamp_min);
				sattr.sched_util_min = uclamp_min;
				sattr.sched_flags |= SCHED_FLAG_UTIL_CLAMP_MIN;
				break;
			case 'p':
				sscanf(optarg,"%d",&pid);
				break;
			case 'j':
				sattr.sched_flags |= 0x80;
				do_syscall = 1;
				break;
			case 'l':
				sscanf(optarg, "%d", &latency_tolerance);
				sattr.sched_latency_tolerance = latency_tolerance;
				sattr.sched_flags |= SCHED_FLAG_LATENCY_TOLERANCE;
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

	ret = syscall(SYS_sched_setattr, pid, &sattr, 0);
	if (ret)
		printf("Unable to do syscall\n");

	return 0;
}

