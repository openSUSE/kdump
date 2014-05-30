#!/bin/bash
# module-setup.sh for kdump dracut module

. /lib/kdump/setup-kdump.functions

check() {
    return 255
}

depends() {
    echo drm
}

install() {
    # Get configuration
    kdump_get_config || return 1
    kdump_import_targets

    kdump_setup_files "$initdir" "${!host_fs_types[*]}"

    inst_hook mount 30 "$moddir/mount-kdump.sh"
    inst_hook pre-pivot 90 /lib/kdump/save_dump.sh
    inst_multiple makedumpfile makedumpfile-R.pl kdumptool \
	$KDUMP_REQUIRED_PROGRAMS
    inst_simple /etc/resolv.conf
}
