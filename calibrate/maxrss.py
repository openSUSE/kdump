#! /usr/bin/python3

import sys
import argparse

parser = argparse.ArgumentParser()
parser.add_argument('-d', '--debug', action='store_true',
                    help='print debugging messages on stderr')
cmdline = parser.parse_args()

contexts = dict()
running = dict()
rss = 0
maxrss = 0
maxrunning = dict()

memfree = None
cached = None
percpu = None
pagesize = None
sizeofpage = None

try:
    while True:
        (category, data) = input().split(':', 1)

        if category == 'rss_stat':
            index = data.rindex(': rss_stat: ')
            (context, cpu, stamp) = data[:index].rsplit(maxsplit=2)
            mm = None
            size = None
            curr = False
            for field in data[index+12:].split():
                (key, val) = field.split('=')
                if key == 'mm_id':
                    mm = int(val)
                elif key == 'curr':
                    if int(val):
                        curr = True
                elif key == 'size':
                    size = int(val[:-1]) // 1024
            if curr:
                contexts[mm] = context.strip()
            oldsize = running.get(mm, 0)
            if size:
                running[mm] = size
            else:
                del running[mm]
            rss += size - oldsize
            if rss > maxrss:
                maxrss = rss
                maxrunning = running.copy()

        elif category == 'meminfo':
            (key, value) = data.split(':')
            if key == 'MemFree':
                memfree = int(value.split()[0])
            elif key == 'Cached':
                cached = int(value.split()[0])
            elif key == 'Percpu':
                percpu = int(value.split()[0])

        elif category == 'vmcoreinfo':
            (key, value) = data.split('=')
            if key == 'PAGESIZE':
                pagesize = int(value)
            elif key == 'SIZE(page)':
                sizeofpage = int(value)

        else:
            if cmdline.debug:
                print('Unknown category: {}'.format(category),
                      file=sys.stderr)

except EOFError:
    pass

if memfree is None:
    print('Cannot determine MemFree', file=sys.stderr)
    exit(1)

if cached is None:
    print('Cannot determine Cached', file=sys.stderr)
    exit(1)

if percpu is None:
    print('Cannot determine Percpu', file=sys.stderr)
    exit(1)

if pagesize is None:
    print('Cannot determine page size', file=sys.stderr)
    exit(1)

if sizeofpage is None:
    print('Cannot determine sizeof(struct page)', file=sys.stderr)

if cmdline.debug:
    print('Max RSS processes:', file=sys.stderr)
    for (mm, rss) in maxrunning.items():
        desc = contexts.get(mm, 'mm_{}'.format(mm))
        print('-', desc, rss, file=sys.stderr)

print('PAGESIZE={:d}'.format(pagesize))
print('SIZEOFPAGE={:d}'.format(sizeofpage))
print('INIT_MEMFREE={:d}'.format(memfree))
print('INIT_CACHED={:d}'.format(cached))
print('PERCPU={:d}'.format(percpu))
print('USER_BASE={:d}'.format(maxrss))
