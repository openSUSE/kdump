#! /bin/sh
# -*- mode: shell-script; indent-tabs-mode: nil; sh-basic-offset: 4; -*-
# ex: ts=8 sw=4 sts=4 et filetype=sh

# An Initrd with dump capturing support can boot a production kernel
# as well (FADump). In such scenario, apply optimizations only while
# booting the capture kernel - this is a kernel that
#   a) has a /proc/vmcore file waiting to be saved.
#   b) reboots after saving the dump.

# apply multipath optimizations
if [ -s /proc/vmcore ]; then
    # Replace the multipath.conf file with the one optimized for kdump.
    rm -f /etc/multipath.conf
    mv /etc/multipath.conf.kdump /etc/multipath.conf
fi
