print("Enter kernel trace file name")
tracefile = raw_input()
print("Enter output filename")
outfile = raw_input()
print("Enter workload trace file name")
workload_outfile = raw_input()


def parse_kernel_trace(pid, kernel_trace):
    with open(tracefile,"r") as f:
        line = f.readline()
        while line:
            if('t_cpu' in line):
                trace_line = line.split(" ")
                while '' in trace_line:
                    trace_line.remove('')
                pid_entity = trace_line[6]
                if(pid in pid_entity):
                    cpu = int(trace_line[-1].split('=')[1])
                    time = trace_line[3].split(":")[0]
                    kernel_trace.append([cpu, time])
            line = f.readline()


def parse_workload_trace(table):
    with open(workload_outfile, "r") as f:
        pid = 0
        start_tracing = 0
        line = f.readline()
        kernel_trace = []
        workload_trace = []
        while line:
            trace_line = line.split(" ")
            if(':Visi' in line):
                if(start_tracing):
                    table.append([pid, kernel_trace, workload_trace])
                else:
                    start_tracing = 1
                pid = trace_line[0].split(":")[0]
                kernel_trace = []
                parse_kernel_trace(pid, kernel_trace)

                workload_trace = []
            else:
                cpu = int(trace_line[1].split("=")[1])
                time = trace_line[3].split("=")[1]
                workload_trace.append([cpu, time])
            line = f.readline()

        table.append([pid, kernel_trace, workload_trace])

def join_list(table):
    ol = []
    for pid in table:
        joint_table = []
        workload_trace_iter = 0
        wt = pid[2]
        for kt in pid[1]:
            if (kt[0]==wt[workload_trace_iter][0]):
                joint_table.append([kt[0], kt[1], wt[workload_trace_iter][1]])
                workload_trace_iter += 1
            else:
                joint_table.append([kt[0], kt[1], ''])
        ol.append([pid[0], joint_table])

    return ol


table = []
#parse_trace(4906)
parse_workload_trace(table)
print(join_list(table))
