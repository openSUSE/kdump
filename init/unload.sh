#! /bin/bash
#
#  Copyright 2015 Petr Tesarik <ptesarik@suse.com>
#  Unload the kdump kernel

KDUMPTOOL=/usr/sbin/kdumptool
KEXEC=/sbin/kexec
FADUMP_REGISTERED=/sys/kernel/fadump_registered
UDEV_RULES_DIR=/run/udev/rules.d

eval $($KDUMPTOOL dump_config)

if [ "$KDUMP_FADUMP" = "yes" ]; then
    # The kernel fails with EINVAL if unregistered already
    # (see bnc#814780)
    if [ $(cat "$FADUMP_REGISTERED") != "0" ] ; then
	echo 0 > "$FADUMP_REGISTERED"
    fi
else
    $KEXEC -a -p -u
fi

test $? -eq 0 || exit 1

rm -f "$UDEV_RULES_DIR"/70-kdump.rules
exit 0
