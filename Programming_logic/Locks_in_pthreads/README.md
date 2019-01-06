## Analysis of Locking in C: Mutex vs Spinlocks vs Futex

The program `pthread_locks.c` contains code with the implementation of all the three locking mechanisms.
Once can get specific code for a lock with `-E` parameter to gcc.
e.g. to get pre-processed code for Spinlocks only: `gcc pthread_locks.c -lpthread -DUSE_SPINLOCK -DLOOPS=100000000 -E | less`

The program is intended for 2 parallel threads trying to complete `LOOPS`.
### How to compile
- For use with Mutex: `gcc pthread_locks.c -lpthread -DLOOPS=100000000`
- For use with SPIN_LOCK: `gcc pthread_locks.c -lpthread -DUSE_SPINLOCK -DLOOPS=100000000`
- For use with FUTEX: `gcc pthread_locks.c -lpthread -DUSE_FUTEX -DLOOPS=100000000`

## Analysis
Use `perf` tool to find the CPUs utilized to find the difference in all three mechanism. 
- Futex will consume least CPU and Spin lock the most.
- Instructions per Cycles will be more in Futex, though the time taken by futex will be more.
- Futex are user space optimization of mutexs and tries to make least syscalls leading to increase in IPC, but since it gets less cycles the time taken will be more.
