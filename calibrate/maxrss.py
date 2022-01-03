#! /usr/bin/python3

import sys
import argparse

class rss_change:
    def __init__(self):
        self.added = list()
        self.removed = list()
        self.addsize = 0
        self.removesize = 0

    def add(self, name, amount):
        self.added.append(name)
        self.addsize += amount

    def remove(self, name, amount):
        self.removed.append(name)
        self.removesize += amount

parser = argparse.ArgumentParser()
parser.add_argument('-d', '--debug', action='store_true',
                    help='print debugging messages on stderr')
cmdline = parser.parse_args()

processes = []
events = {}

memfree = None

try:
    while True:
        (category, data) = input().split(':', 1)

        if category == 'taskstats':
            fields = data.split(maxsplit=3)
            etime = int(fields[0])
            btime = etime - int(fields[1])
            rss = int(fields[2])

            procidx = len(processes)
            processes.append((fields[3], rss))
            events.setdefault(btime, rss_change()).add(procidx, rss)
            events.setdefault(etime, rss_change()).remove(procidx, rss)

        elif category == 'meminfo':
            (key, value) = data.split(':')
            if key == 'MemFree':
                memfree = int(value.split()[0])

        else:
            if cmdline.debug:
                print('Unknown category: {}'.format(category),
                      file=sys.stderr)

except EOFError:
    pass

if memfree is None:
    print('Cannot determine MemFree', file=sys.stderr)
    exit(1)

rss = 0
maxrss = 0
running = set()
maxrunning = set()
for t in sorted(events):
    change = events[t]
    rss += change.addsize
    running.update(change.added)
    if rss > maxrss:
        maxrss = rss
        maxrunning = set(running)
    running.difference_update(change.removed)
    rss -= change.removesize

print(maxrss)

if cmdline.debug:
    print('Max RSS processes:', file=sys.stderr)
    for idx in maxrunning:
        p = processes[idx]
        print('-', p[1], p[0], file=sys.stderr)
