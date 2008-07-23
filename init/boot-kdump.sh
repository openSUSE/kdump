#!/bin/bash
#%stage: setup
#%depends: mount network
#%programs: /bin/makedumpfile /bin/grep /sbin/kdumptool /sbin/showmount mount.cifs mount.nfs /bin/gdb
#%if: "$use_kdump"
#%udevmodules: cifs

#
# If KDUMP_IMMEDIATE_REBOOT is false, then open a shell. If it's true, then
# reboot.
function handle_exit()
{
    if [ $KDUMP_IMMEDIATE_REBOOT = "yes" \
            -o "$KDUMP_IMMEDIATE_REBOOT" = "YES" ] ; then
        reboot -f
    else
        bash
    fi
}

#
# Checks the given parameter. If the status was non-zero, then it checks
# KDUMP_CONTINUE_ON_ERROR if it should reboot or just continue.
# In any case, it prints an error message.
function continue_error()
{
    local status=$?

    if [ $status -eq 0 ] ; then
        return
    fi

    echo "Last command failed ($status)."

    if ! [ "$KDUMP_CONTINUE_ON_ERROR" = "true" -o
                "$KDUMP_CONTINUE_ON_ERROR" = "TRUE" ] ; then
        handle_exit
    fi
}

#
# sanity check
if ! grep elfcorehdr /proc/cmdline &>/dev/null ; then
    echo "Kdump initrd booted in non-kdump kernel"
    return 1
fi

. /etc/sysconfig/kdump

#
# start LED blinking in background
kdumptool led_blink --background

#
# verbose output if requested
if (( $KDUMP_VERBOSE & 8 )) ; then
    KDUMPTOOL_OPTIONS="$KDUMPTOOL_OPTIONS -D"
fi

#
# delete old dumps
kdumptool delete_dumps $KDUMPTOOL_OPTIONS
continue_error $?

#
# save the dump
kdumptool save_dump --root=/root $KDUMPTOOL_OPTIONS

handle_exit

