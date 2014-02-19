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
# check if we are called with the -f kdump parameter
#
if ! (( $use_kdump )) ; then
    return 0
fi

# /lib/kdump/setup-kdump.functions was sourced from setup-kdumpfs.sh already

#
# get the configuration
#
kdump_config=$( kdumptool dump_config --format=shell )
if [ $? -ne 0 ] ; then
    echo "kdump configuration failed"
    return 1
fi
eval "$kdump_config"

#
# create target configuration
#
mkdir -p "${tmp_mnt}${CONFIG%/*}"
modify_config > "${tmp_mnt}${CONFIG}"

#
# remember the host name
#
hostname >> ${tmp_mnt}/etc/hostname.kdump

#
# check if extra modules are needed
#
if module_has_device hpwdt; then
    kdump_fsmod="$kdump_fsmod hpwdt"
fi

#
# copy public and private key if needed
#
if [ -n "$kdump_over_ssh" ] ; then
    copy_ssh_keys "$tmp_mnt"
fi

# create modified multipath.conf
if [ -e /etc/multipath.conf ] ; then
    modify_multipath "$blockdev" > "${tmp_mnt}/etc/multipath.conf"
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

# vim: set sw=4 ts=4 et:
