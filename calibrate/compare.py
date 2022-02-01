#! /usr/bin/python3

import sys
import argparse

def read_config(path):
    ret = dict()
    with open(path, 'r') as f:
        for line in f:
            (key, val) = line.split('=', 1)
            ret[key] = int(val)
    return ret

if __name__ == '__main__':
    parser = argparse.ArgumentParser()
    parser.add_argument('-t', '--tolerance', type=int, default=0,
                        help='tolerance in percents')
    parser.add_argument('refname', metavar="REFERENCE",
                        help='reference file')
    parser.add_argument('newname', metavar="NEW",
                        help='new file')
    args = parser.parse_args()

    err = 0
    refcfg = read_config(args.refname)
    newcfg = read_config(args.newname)
    for key in refcfg:
        ref = refcfg[key]
        new = newcfg[key]
        if new < ref * (100 - args.tolerance) / 100:
            print('{} value {} is {:.1f} % below reference'.format(
                key, new, 100 - 100 * new / ref))
            err = 1
        elif new > ref * (100 + args.tolerance) / 100:
            print('{} value {} is {:.1f} % above reference'.format(
                key, new, 100 * new / ref - 100))
            err = 1

    exit(err)
