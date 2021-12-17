#! /usr/bin/python3

class rss_change:
    def __init__(self):
        self.added = list()
        self.removed = list()
        self.amount = 0

    def add(self, name, amount):
        self.added.append(name)
        self.amount += amount

    def remove(self, name, amount):
        self.removed.append(name)
        self.amount -= amount

processes = []
events = {}

try:
    while True:
        fields = input().split(maxsplit=3)
        btime = int(fields[0])
        etime = btime + int(fields[1]) // 1000000
        rss = int(fields[2])

        procidx = len(processes)
        processes.append((fields[3], rss))
        events.setdefault(btime, rss_change()).add(procidx, rss)
        events.setdefault(etime, rss_change()).remove(procidx, rss)

except EOFError:
    pass

rss = 0
maxrss = 0
running = set()
maxrunning = set()
for t in sorted(events):
    change = events[t]
    rss += change.amount
    running.update(change.added)
    running.difference_update(change.removed)
    if rss > maxrss:
        maxrss = rss
        maxrunning = set(running)

print(maxrss)
for idx in maxrunning:
    p = processes[idx]
    print(p[1], p[0])
