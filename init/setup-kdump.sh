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
#%stage: crypto
#%provides: kdump

#
# check if we are called with the -f kdump parameter
#
use_kdump=
if use_script kdump ; then
    use_kdump=1
fi

if (( $use_kdump )) ; then
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
hostname --fqdn >> ${tmp_mnt}/etc/hostname.kdump

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
for program in "$KDUMP_REQUIRED_PROGRAMS" ; do
    if [ ! -f "$program" ] ; then
        echo >&2 ">>> $program does not exist. Skipping!"
        continue
    fi

    dir=$(dirname "$program")
    mkdir -p "${tmp_mnt}/${dir}"
    cp_bin "$program" "${tmp_mnt}/${dir}"
done


save_var use_kdump

# vim: set sw=4 ts=4 et:
