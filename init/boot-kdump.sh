#!/bin/bash
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
# 02110-1301, USA.
#
#%stage: setup
#%depends: mount network
#%programs: /bin/makedumpfile /bin/awk /bin/gawk /bin/grep /sbin/kdumptool $kdump_fsprog
#%if: "$use_kdump"
#%udevmodules: nls_utf8 $kdump_fsmod

#
# If KDUMP_IMMEDIATE_REBOOT is false, then open a shell. If it's true, then
# reboot.
function handle_exit()
{
    # restore core dump stuff
    if [ -n "$backup_core_pattern" ] ; then
        echo "$backup_core_pattern" > /proc/sys/kernel/core_pattern
    fi
    if [ -n "$backup_ulimit" ] ; then
        ulimit -c "$backup_ulimit"
    fi
    
    if [ $KDUMP_IMMEDIATE_REBOOT = "yes" \
            -o "$KDUMP_IMMEDIATE_REBOOT" = "YES" ] ; then
        reboot -f
    else
        echo
        echo "Dump saving completed."
        echo "Type 'reboot -f' to reboot the system or 'exit' to"
        echo "boot in a normal system that still is running in the"
        echo "kdump environment with restricted memory!"
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
        return 0
    fi

    echo "Last command failed ($status)."

    if ! [ "$KDUMP_CONTINUE_ON_ERROR" = "true" -o \
                "$KDUMP_CONTINUE_ON_ERROR" = "TRUE" ] ; then
        echo
        echo "Something failed. You can try to debug that here."
        echo "Type 'reboot -f' to reboot the system or 'exit' to"
        echo "boot in a normal system that still is running in the"
        echo "kdump environment with restricted memory!"
        bash
	return 1
    fi
}

#
# Mounts all partitions listed in /etc/fstab.kdump
function mount_all()
{
    local ret=0

    if [ -f /etc/fstab ] ; then
        mv /etc/fstab /etc/fstab.orig
    fi

    cp /etc/fstab.kdump /etc/fstab
    mount -a
    ret=$?

    if [ -f /etc/fstab.orig ] ; then
        mv /etc/fstab.orig /etc/fstab
    else
        rm /etc/fstab
    fi

    return $ret
}

#
# sanity check
if ! grep elfcorehdr /proc/cmdline &>/dev/null ; then
    echo "Kdump initrd booted in non-kdump kernel"
    return 1
fi

. /etc/sysconfig/kdump

ROOTDIR=/root

#
# start LED blinking in background
kdumptool ledblink --background

#
# create core dumps by default here for kdumptool debugging
read backup_core_pattern < /proc/sys/kernel/core_pattern
backup_ulimit=$(ulimit -c)

echo "$ROOTDIR/tmp/core.kdumptool" > /proc/sys/kernel/core_pattern
ulimit -c unlimited

#
# create mountpoint for NFS/CIFS
[ -d /mnt ] || mkdir /mnt

if [ -n "$KDUMP_TRANSFER" ] ; then
    echo "Running $KDUMP_TRANSFER"
    eval "$KDUMP_TRANSFER"
else

    #
    # mount all partitions in fstab
    mount_all
    if ! continue_error $?; then
        return
    fi

    #
    # prescript
    if [ -n "$KDUMP_PRESCRIPT" ] ; then
        echo "Running $KDUMP_PRESCRIPT"
        eval "$KDUMP_PRESCRIPT"
        if ! continue_error $?; then
            return
        fi
    fi

    #
    # delete old dumps
    kdumptool delete_dumps $KDUMPTOOL_OPTIONS
    if ! continue_error $?;then
        return
    fi

    #
    # save the dump (HOME=/ to find the public/private key)
    read hostname < /etc/hostname.kdump
    HOME=/ TMPDIR=/root/tmp kdumptool save_dump --root=$ROOTDIR \
        --hostname=$hostname $KDUMPTOOL_OPTIONS
    if ! continue_error $?; then
        return
    fi

    #
    # postscript
    if [ -n "$KDUMP_POSTSCRIPT" ] ; then
        echo "Running $KDUMP_POSTSCRIPT"
        eval "$KDUMP_POSTSCRIPT"
        if ! continue_error $?; then
           return
        fi
    fi
fi

handle_exit

# vim: set sw=4 ts=4 et:
