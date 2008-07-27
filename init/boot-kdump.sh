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
#%programs: /bin/makedumpfile /bin/awk /bin/grep /sbin/kdumptool /sbin/showmount /sbin/mount.cifs mount.nfs /bin/gdb
#%if: "$use_kdump"
#%udevmodules: cifs nls_utf8

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

    if ! [ "$KDUMP_CONTINUE_ON_ERROR" = "true" -o \
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

ROOTDIR=/root

#
# start LED blinking in background
kdumptool ledblink --background

#
# create mountpoint for NFS/CIFS
mkdir /mnt

if [ -n "$KDUMP_TRANSFER" ] ; then
    echo "Running $KDUMP_TRANSFER"
    eval "$KDUMP_TRANSFER"
else

    # get the target
    target=$(kdumptool print_target)
    protocol=$(echo "$target" | grep '^Protocol' | awk '{ print $2 }')
    path=$(echo "$target" | grep '^Path' | awk '{ print $2 }')

    # mount all partitions in fstab
    mv /etc/fstab.kdump /etc/fstab
    mount -a

    #
    # prescript
    if [ -n "$KDUMP_PRESCRIPT" ] ; then
        echo "Running $KDUMP_PRESCRIPT"
        eval "$KDUMP_PRESCRIPT"
        continue_error $?
    fi

    #
    # delete old dumps
    kdumptool delete_dumps $KDUMPTOOL_OPTIONS
    continue_error $?

    #
    # save the dump (HOME=/ to find the public/private key)
    read hostname < /etc/hostname.kdump
    HOME=/ kdumptool save_dump --root=$ROOTDIR \
        --fqdn=$hostname $KDUMPTOOL_OPTIONS

    #
    # postscript
    if [ -n "$KDUMP_POSTSCRIPT" ] ; then
        echo "Running $KDUMP_POSTSCRIPT"
        eval "$KDUMP_POSTSCRIPT"
        continue_error $?
    fi
fi

handle_exit

# vim: set sw=4 ts=4 et:
