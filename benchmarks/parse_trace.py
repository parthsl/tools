tracefile = "new_trace"
outfile = "out1"
workload_outfile = "f1"


def parse_kernel_trace(pid, kernel_trace):
    with open(tracefile,"r") as f:
        line = f.readline()
        while line:
            if('dest_cpu' in line):
                trace_line = line.split(" ")
                while '' in trace_line:
                    trace_line.remove('')
                pid_entity = trace_line[6]
                if(pid in pid_entity):
                    cpu = int(trace_line[9].split('=')[1])
                    time = trace_line[3].split(":")[0]
                    kernel_trace.append([time, cpu])
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
                pid = trace_line[0].split(":")[0]
                if(start_tracing):
                    table.append([pid, kernel_trace, workload_trace])
                else:
                    start_tracing = 1
                kernel_trace = []
                parse_kernel_trace(pid, kernel_trace)

                workload_trace = []
            else:
                cpu = (trace_line[1].split("=")[1])
                time = trace_line[3].split("=")[1]
                workload_trace.append([cpu, time])
            line = f.readline()

        table.append([pid, kernel_trace, workload_trace])

table = []
#parse_trace(4906)
parse_workload_trace(table)
print(table)
