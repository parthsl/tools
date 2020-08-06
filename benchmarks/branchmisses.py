import threading as th
import time
import random as rand
import argparse
import math

examples = """examples
"""
parser = argparse.ArgumentParser(
                description="Python code to miss branches everytime with large WSS",
                formatter_class=argparse.RawDescriptionHelpFormatter,
                epilog=examples)
parser.add_argument("-w", "--wss", default=10000, help="WSS size. Default = 10000")
parser.add_argument("-n", "--nr", default=1, help="Total threads")
args = parser.parse_args()

N = int(args.wss)
nr = int(args.nr)
lock = None

def worker(lock):
    summ = 0
    buf = []
    rand.seed()
    for i in range(N):
        buf.append(rand.randint(0, 10))

    for i in range(N):
        index = rand.randint(0, N-1)
        operation = rand.randint(0, 4)
        if operation == 0:
            summ = buf[index] + rand.random()*N
        elif operation == 1:
            summ = buf[index]
        elif operation == 2:
            summ = buf[index] = buf[rand.randint(0, N-1)] = buf[rand.randint(0, N-1)]
        else:
            summ = buf[index] * 1

def main():
    lock = th.Lock()
    tid = []
    for i in range(nr):
        t1 = th.Thread(target=worker, args=(lock,))
        tid.append(t1)

    for i in tid:
        i.start()

    for i in tid:
        i.join()

if __name__ == "__main__":
    starttime = time.time()
    main()
    print(time.time()-starttime)
