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
#%stage: filesystem
#%depends: storage
#%provides: kdump

#
# check if we are called with the -f kdump parameter
#
use_kdump=
if use_script kdump ; then
    use_kdump=1
else
    return 0
fi

. /lib/kdump/setup-kdump.functions

#
# Get the mount modules for a given file system type
# Parameters: 1) fstype:  filesystem type
# Output:
#                mntmod:  modules required by the fs, if any
function get_mntmod()
{
    local fstype="$1"

    # We assume that we always have to load a module for the fs
    mntmod=$fstype

    # Check if we have to load a module for the fs type
    # XXX: This check should happen more generically for all modules
    if [ ! "$(find $root_dir/lib/modules/$kernel_version/ -name $mntmod.ko)" ]
    then
        if grep -q ${mntfstype}_fs_type "$map" ; then
            # No need to load a module, since this is compiled in
            mntmod=
        fi
    fi
}

#
# Add the mount helper to kdump_fsprog if necessary.
# Parameters: 1) fstype: filesystem type
# Global state:
#                kdump_fsprog: updated with the mount helper if needed
function add_mntprog()
{
    local fstype="$1"
    local helper="$root_dir/sbin/mount.${fstype}"
    if [ -e "$helper" ]; then
        kdump_fsprog="$kdump_fsprog $helper"
    fi
}

#
# Add a filesystem to the initrd
# Parameters: 1) fstype: filesystem type
# Global state:
#     kdump_fsmod:  updated with required kernel modules
#     kdump_fsprog: updated with required helper programs
function add_fstype()							   # {{{
{
    local fstype="$1"

    get_mntmod "$mntfstype"
    kdump_fsmod="$kdump_fsmod $mntmod"
    add_mntprog "$mntfstype"
}									   # }}}

#
# Translate a block device path into its path by UUID
# Parameters: 1) blkdev: block device
# Returns:    blkdev by UUID (or blkdev if blkdev is not a local block device)
function blkdev_by_uuid()                                                  # {{{
{
    local blkdev="$1"

    if [ -b "$blkdev" ] ; then
	local uuid=$(/sbin/blkid "$blkdev" |
	    sed -e 's/.*UUID="\([-a-fA-F0-9]*\)".*/\1/')
        if [ -n "$uuid" ] && [ -b "/dev/disk/by-uuid/$uuid" ] ; then
            blkdev="/dev/disk/by-uuid/$uuid"
        fi
    fi
    echo "$blkdev"
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

    # add "nolock" for NFS
    if [ "$fstype" = "nfs" ]; then
        opts="${opts},nolock"
    fi

    echo "$fsname $mntdir $fstype $opts $freq $passno" \
        >> ${tmp_mnt}/etc/fstab.kdump
}                                                                          # }}}

#
# Add a fstab entry by its mount point
# Parameters: 1) human-readable type of the mount
#             2) block device to mount
#             3) mountpoint in kdump environment
#             4) file system type
#             5) mount options
function kdump_add_mount						   # {{{
{
    local desc="$1"
    local blkdev="$2"
    local mountpoint="$3"
    local fstype="$4"
    local opts="$5"

    add_fstype "$fstype"
    blkdev=$(blkdev_by_uuid $(resolve_device "$desc" "$blkdev"))
    add_fstab "$blkdev" "/kdump${mountpoint}" "$fstype" "$opts" 0 0
    blockdev="$blockdev $blkdev"
    echo "$blkdev"
}									   # }}}

################################################################################

#
# check mount points
#

kdump_get_mountpoints || return 1

#
# Add mount points (below /kdump)
#

touch "${tmp_mnt}/etc/fstab.kdump"

# add the boot partition
if [ -n "${kdump_mnt[0]}" ]
then
    bootdev=$(kdump_add_mount "Boot" "${kdump_dev[0]}" "/mnt0" \
	"${kdump_fstype[0]}" "${kdump_opts[0]}" )
fi

# additional mount points
i=1
while [ $i -lt ${#kdump_mnt[@]} ]
do
    dumpdev=$(kdump_add_mount "Dump" "${kdump_dev[i]}" "/mnt$i" \
	"${kdump_fstype[i]}" "${kdump_opts[i]}" )
    i=$((i+1))
done

#
# Get the list of filesystem modules
#

kdump_fsmod=
for protocol in "${kdump_Protocol[@]}" ; do
    if [ "$protocol" = "nfs" ]; then
        interface=${interface:-default}
        add_fstype nfs
    fi

    if [ "$protocol" = "cifs" -o "$protocol" = "smb" ]; then
        interface=${interface:-default}
        add_fstype cifs
    fi
done

# vim: set sw=4 ts=4 et:
