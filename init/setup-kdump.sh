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
# Checks whether there is a device in the system which is handled
# by the specified module.
# Parameters:
#   1) modname: kernel module name
# Exit status:
#   zero     if a matching device was found
#   non-zero otherwise
function module_has_device()                                               # {{{
{
    local modname="$1"
    local -a aliaslist
    local line

    while read line; do
	aliaslist[${#aliaslist[@]}]="$line"
    done < <(modinfo -k "$kernel_version" -F alias "$modname" 2>/dev/null)

    # for each device in the system, check the device modalias file ...
    find /sys/devices -type f -name modalias -print0 | xargs -0 cat | \
    (
	while read line; do
	    # ... against each modalias of the checked module
	    for modalias in "${aliaslist[@]}"; do
		case "$line" in
		    $modalias)
			exit 0
			;;
	        esac
	    done
	done
	exit 1
    )
}                                                                          # }}}

#
# check if we are called with the -f kdump parameter
#
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

#
# read in the configuration
source "$CONFIG"

#
# replace the KDUMP_SAVEDIR with the resolved path
if [ "$kdump_protocol" = "file" ] ; then
    KDUMP_SAVEDIR="file://$kdump_path"
fi

#
# get the host key, if needed
if [ "$kdump_protocol" = "sftp" -a -z "$KDUMP_HOST_KEY" ] ; then
    KDUMP_HOST_KEY=$(ssh-keygen -F "$kdump_host" 2>/dev/null | \
                     awk '/^[^#]/ { if (NF==3) { print $3; exit } }')
    if [ -z "$KDUMP_HOST_KEY" ] ; then
        echo "WARNING: target SSH host key not found. " \
             "Man-in-the-middle attack is possible." >&2
    fi
fi

#
# copy the configuration file, modifying:
#   KDUMP_SAVEDIR  -> resolved path
#   KDUMP_HOST_KEY -> target host public key
mkdir -p ${tmp_mnt}/etc/sysconfig/
sed -e 's#^[ 	]*\(KDUMP_SAVEDIR\)=.*#\1="'"$KDUMP_SAVEDIR"'"#g' \
    -e 's#^[ 	]*\(KDUMP_HOST_KEY\)=.*#\1="'"$KDUMP_HOST_KEY"'"#g' \
    "$CONFIG" > "${tmp_mnt}${CONFIG}"

#
# add the host key explicitly if the option was missing
grep '^[ 	]*KDUMP_HOST_KEY=' ${tmp_mnt}${CONFIG} >/dev/null 2>&1 \
    || echo 'KDUMP_HOST_KEY="'"$KDUMP_HOST_KEY"'"' >> "${tmp_mnt}${CONFIG}"

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
if [ "$kdump_protocol" = "sftp" ] ; then
    if [ -f /root/.ssh/id_dsa ] && [ -f /root/.ssh/id_dsa.pub ] ; then
        mkdir -p ${tmp_mnt}/.ssh
        cp /root/.ssh/id_dsa ${tmp_mnt}/.ssh
        cp /root/.ssh/id_dsa.pub ${tmp_mnt}/.ssh
    fi
    if [ -f /root/.ssh/id_rsa ] && [ -f /root/.ssh/id_rsa.pub ] ; then
        mkdir -p ${tmp_mnt}/.ssh
        cp /root/.ssh/id_rsa ${tmp_mnt}/.ssh
        cp /root/.ssh/id_rsa.pub ${tmp_mnt}/.ssh
    fi
fi

#
# copy modified multipath.conf
#
if [ -e /etc/multipath.conf ] ; then
    i=0
    for bd in $blockdev ; do
	scsi_id=$(/lib/udev/scsi_id --whitelisted --device="$bd")
	[ -z "$scsi_id" ] && continue
	wwids[i++]="wwid "\""$scsi_id"\"
    done
    kdumptool multipath "${wwids[@]}" \
	< /etc/multipath.conf > "${tmp_mnt}/etc/multipath.conf"
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

save_var use_kdump
save_var kdump_fsmod
save_var bootdev
save_var dumpdev

# vim: set sw=4 ts=4 et:
