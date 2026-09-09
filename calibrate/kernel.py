#! /usr/bin/python3

import sys
import argparse
import re

parser = argparse.ArgumentParser()
parser.add_argument('-d', '--debug', action='store_true',
                    help='print debugging messages on stderr')
cmdline = parser.parse_args()

re_freeing = re.compile('Freeing (.*) memory: (\\d+)K$')

freed = 0
unpack = False
for line in sys.stdin:
    line = line.rstrip()
    match = re_freeing.search(line)
    if match:
        if unpack:
            freed += int(match[2])
            if cmdline.debug:
                print('Add {}K for {}'.format(match[2], match[1]),
                      file=sys.stderr)
        elif cmdline.debug:
            print('Ignore {}K for {}'.format(match[2], match[1]),
                  file=sys.stderr)
    elif 'Trying to unpack rootfs image' in line:
        unpack = True

print('KERNEL_INIT={:d}'.format(freed))
