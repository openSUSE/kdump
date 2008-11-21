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
#%stage: boot
#%provides: kdump

#
# Returns the UUID for a given disk
# Parameters: 1) partition device (e.g. /dev/sda5)
# Returns:    The UUID
function get_uuid_for_disk()                                               # {{{
{
    local uuid=
    local device=$1

    local output=$(/sbin/blkid ${device})
    local uuid=$(echo "$output" | sed -e 's/.*UUID="\([-a-fA-F0-9]*\)".*/\1/')
    echo "$uuid"
}                                                                          # }}}

#
# Adds an entry to a kdump fstab.
# Parameters: 1) fsname: file system name
#             2) mntdir: mount directory
#             3) fstype: file system type
#             4) opts:   mount options
#             5) freq:   dump frequency
#             6) passno: pass number on parallel fsck
function add_fstab()                                                       # {{{
{
    local fsname=$1
    local mntdir=$2
    local fstype=$3
    local opts=$4
    local freq=$5
    local passno=$6

    # translate block devices into UUID
    if [ -b "$fsname" ] ; then
        local uuid=$(get_uuid_for_disk "$fsname")
        if [ -n "$uuid" ] && [ -b "/dev/disk/by-uuid/$uuid" ] ; then
            fsname="/dev/disk/by-uuid/$uuid"
        fi
    fi

    echo "$fsname $mntdir $fstype $opts $freq $passno" \
        >> ${tmp_mnt}/etc/fstab.kdump
}                                                                          # }}}


#
# check if we are called with the -f kdump parameter
#
use_kdump=
if use_script kdump ; then
    use_kdump=1
fi

if ! (( $use_kdump )) ; then
    return 0
fi

#
# copy /etc/sysconfig/kdump
#
CONFIG=/etc/sysconfig/kdump

if [ ! -f "$CONFIG" ] ; then
    echo "kdump configuration not installed"
    return 1
fi

source "$CONFIG"
mkdir -p ${tmp_mnt}/etc/sysconfig/
cp "$CONFIG" ${tmp_mnt}/etc/sysconfig/

#
# remember the host name
#
hostname >> ${tmp_mnt}/etc/hostname.kdump

#
# copy public and private key
#
if [ -f /root/.ssh/id_dsa ] && [ -f /root/.ssh/id_dsa.pub ] ; then
    mkdir ${tmp_mnt}/.ssh
    cp /root/.ssh/id_dsa ${tmp_mnt}/.ssh
    cp /root/.ssh/id_dsa.pub ${tmp_mnt}/.ssh
fi

#
# copy required programs
#
for program in $KDUMP_REQUIRED_PROGRAMS ; do
    if [ ! -f "$program" ] ; then
        echo >&2 ">>> $program does not exist. Skipping!"
        continue
    fi

    dir=$(dirname "$program")
    mkdir -p "${tmp_mnt}/${dir}"
    cp_bin "$program" "${tmp_mnt}/${dir}"
done

#
# get the save directory and protocol
#
target=$(kdumptool print_target)
if [ -z "$target" ] ; then
    echo >&2 "kdumptool print_target failed."
    return 1
fi
protocol=$(echo "$target" | grep '^Protocol' | awk '{ print $2 }')
path=$(echo "$target" | grep '^Realpath' | awk '{ print $2 }')

#
# replace the KDUMP_SAVEDIR with the resolved path
if [ "$protocol" = "file" ] ; then
    sed -i "s#KDUMP_SAVEDIR=.*#KDUMP_SAVEDIR=\"file://$path\"#g" ${tmp_mnt}/$CONFIG
fi

#
# add mount points (below /root)
#

touch ${tmp_mnt}/etc/fstab.kdump
while read line ; do
    device=$(echo "$line" | awk '{ print $1 }')
    mountpoint=$(echo "$line" | awk '{ print $2 }')
    filesystem=$(echo "$line" | awk '{ print $3 }')
    opts=$(echo "$line" | awk '{ print $4 }')

    # add the boot partition
    if [ "$mountpoint" = "/boot" ] ; then
        add_fstab "$device" "/root$mountpoint" "$filesystem" "$opts" 0 0
    fi

    # add the target file system
    if [ "$protocol" = "file" ] &&
            [ "$mountpoint" != "/" ] &&
            echo "$path" | grep "$mountpoint" &> /dev/null ; then
        add_fstab "$device" "/root$mountpoint" "$filesystem" "$opts" 0 0
    fi
done < /etc/mtab

save_var use_kdump

# vim: set sw=4 ts=4 et:
