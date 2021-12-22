#! /usr/bin/python3

import sys

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

processes = []
events = {}

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

        else:
            print('Unknown category: {}'.format(category),
                  file=sys.stderr)

except EOFError:
    pass

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
for idx in maxrunning:
    p = processes[idx]
    print(p[1], p[0])
