#!/usr/bin/env python 

import subprocess

process = subprocess.Popen(['rmdir', '/sys/fs/cgroup/memory/nn'], stdout=subprocess.PIPE)
out, err = process.communicate()
process = subprocess.Popen(['mkdir', '-p', '/sys/fs/cgroup/memory/nn'], stdout=subprocess.PIPE)
out, err = process.communicate()

f = open('/sys/fs/cgroup/memory/nn/memory.limit_in_bytes', 'w')
f.write('65536')
f.close()

process = subprocess.Popen(['echo', '$$>/sys/fs/cgroup/memory/nn/tasks'], stdout=subprocess.PIPE)
out, err = process.communicate()
process = subprocess.Popen(['dd','if=/dev/zero','of=./ff', 'oflag=direct', 'count=10000'], stdout=subprocess.PIPE)
out, err = process.communicate()

print(out)
print(err)
