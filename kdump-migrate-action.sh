#!/bin/sh

systemctl is-active kdump
if [ $? -ne 0 ]; then
        exit 0
fi

export PATH="$PATH:/usr/bin:/usr/sbin"
/usr/lib/kdump/load.sh --update
