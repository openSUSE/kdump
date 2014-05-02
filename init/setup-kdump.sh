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
# Copy or create all necessary files for the initrd
#
kdump_setup_files "$tmp_mnt" root "$blockdev"

#
# check if extra modules are needed
#
if kdump_module_has_device hpwdt; then
    kdump_fsmod="$kdump_fsmod hpwdt"
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

mkdir -p "${tmp_mnt}/lib/kdump"
cp -pL /lib/kdump/save_dump.sh "${tmp_mnt}/lib/kdump/"

save_var dumpdev

# vim: set sw=4 ts=4 et:
