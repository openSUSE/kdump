#!/bin/bash
#
# Save the dump using kdumptool - this script is supposed to be
# executed in the kdump environment.
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

# Certain shell variables must be set:
#
#   dumpdev  space-separated list of block devices used for dumping
#

#
# Checks if fadump is enabled
#
# Returns: 0 (true) if fadump is enabled
#          1 (false) if fadump is not availabled or disabled
function fadump_enabled()
{
    test -f $FADUMP_ENABLED && test "$(cat $FADUMP_ENABLED)" = "1"
}

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

    if fadump_enabled; then
        # release memory if possible
        test -f $FADUMP_RELEASE_MEM && echo 1 > $FADUMP_RELEASE_MEM
        if [ "$KDUMP_FADUMP_SHELL" = "yes" \
                -o "$KDUMP_FADUMP_SHELL" = "YES" ] ; then
            echo
            echo "Dump saving completed."
            echo "Type 'reboot -f' to reboot the system or 'exit' to"
            echo "boot in a normal system."
            bash
        fi
        if [ $KDUMP_IMMEDIATE_REBOOT = "yes" \
                -o "$KDUMP_IMMEDIATE_REBOOT" = "YES" ] ; then
            umount -a
            reboot -f
        fi

        # unmount kdump directories
        dirs=
        while read dev mp rest
        do
            test "${mp#$ROOTDIR}" = "${mp}" && continue
            dirs="$mp $dirs"
        done < /proc/mounts
        umount $dirs
    elif [ $KDUMP_IMMEDIATE_REBOOT = "yes" \
            -o "$KDUMP_IMMEDIATE_REBOOT" = "YES" ] ; then
        umount -a
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
# Wait for a given device to appear
wait_for_dumpdev()
{
    local dev="$1"
    test -z "$dev" && return 0
    test -e "$dev" && return 0

    case "$dev" in
        *:/*) return 0 ;;
        /dev/nfs) return 0 ;;
    esac
    check_for_device "$@"
}

#
# Mounts all partitions listed in /etc/fstab.kdump
function mount_all()
{
    local ret=0

    test -f /etc/fstab.kdump || return 0

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
if [ ! -f /proc/vmcore ] ; then
    echo "Kdump initrd booted in non-kdump kernel"
    return 1
fi

. /etc/sysconfig/kdump

ROOTDIR=/kdump
FADUMP_ENABLED=/sys/kernel/fadump_enabled
FADUMP_RELEASE_MEM=/sys/kernel/fadump_release_mem

#
# start LED blinking
kdumptool ledblink &

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
    # wait for /boot device and dump device(s)
    for dev in $dumpdev ; do
	wait_for_dumpdev "$dev" dump
	if ! continue_error $?; then
            return
	fi
    done

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
    kdumptool delete_dumps --root=$ROOTDIR $KDUMPTOOL_OPTIONS
    if ! continue_error $?;then
        return
    fi

    #
    # save the dump (set HOME to find the public/private key)
    read hostname < /etc/hostname.kdump
    HOME=$ROOTDIR TMPDIR=$ROOTDIR/tmp kdumptool save_dump --root=$ROOTDIR \
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
