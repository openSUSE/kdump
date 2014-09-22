#!/bin/bash
# module-setup.sh for kdump dracut module

. /lib/kdump/setup-kdump.functions

check() {
    return 255
}

depends() {
    echo drm
}

kdump_add_mpath_dev() {
    local _major=${1%:*}
    local _minor=${1#*:}
    local _wwid

    eval _wwid=\$kdump_mpath_wwid_${_major}_${_minor}
    if [ -n "$_wwid" ] ; then
	kdump_mpath_wwids+=$(printf "%q " "wwid $_wwid")
    fi
}

install() {
    # Get configuration
    kdump_get_config || return 1
    kdump_import_targets

    # Get a list of required multipath devices
    local kdump_mpath_wwids
    kdump_map_mpath_wwid
    for_each_host_dev_and_slaves_all kdump_add_mpath_dev

    kdump_setup_files "$initdir" "$kdump_mpath_wwids"

    if dracut_module_included "systemd" ; then
	rm -f "${initdir}/$systemdutildir"/system-generators/dracut-rootfs-generator
	inst_simple /lib/kdump/save_dump.sh
	inst_simple "$moddir/kdump-save.service" \
	    "$systemdsystemunitdir"/kdump-save.service
	ln_r "$systemdsystemunitdir"/kdump-save.service \
	    "$systemdsystemunitdir"/initrd.target.wants/kdump-save.service
    else
	inst_hook mount 30 "$moddir/mount-kdump.sh"
	inst_hook pre-pivot 90 /lib/kdump/save_dump.sh
    fi

    inst_multiple makedumpfile makedumpfile-R.pl kdumptool \
	$KDUMP_REQUIRED_PROGRAMS
    inst_simple /etc/resolv.conf
}
