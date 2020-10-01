#! /usr/bin/python
import argparse

examples = """examples
"""

parser = argparse.ArgumentParser(
        description="Toggle sched_feat",
        formatter_class=argparse.RawDescriptionHelpFormatter,
        epilog=examples)
parser.add_argument("-i", "--index", default=-1, help="toggle based on index")
args = parser.parse_args()

def read_parse_schedfeat():
    fd = open("/sys/kernel/debug/sched_features","r")
    p = fd.read()
    fd.close()
    return p

def show_menu(l):
    l = l.split(" ")
    counter = 0
    for i in l:
        feat = i
        print (counter, feat)
        counter += 1

def toggle(index, l):
    fd = open("/sys/kernel/debug/sched_features","w")
    feat = l.split(" ")[index]
    feat = feat.split("NO_")
    isno = not (feat[-1] == feat[0])

    if isno:
        fd.write(feat[-1])
        print("Turned on ", feat[-1])
    else:
        fd.write("NO_"+feat[-1])
        print("Turned off ",feat[-1])

    fd.close()

while True:
    p = read_parse_schedfeat()

    if args.index >= 0:
        toggle(int(args.index), p)
    else:
        show_menu(p)
        toggle(int(input()), p)
