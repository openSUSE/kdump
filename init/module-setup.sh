#!/bin/bash
# module-setup.sh for kdump dracut module

. /lib/kdump/setup-kdump.functions

kdump_check_net() {
    kdump_neednet=
    for protocol in "${kdump_Protocol[@]}" ; do
	if [ "$protocol" != "file" ]; then
	    kdump_neednet=y
	fi
    done

    # network configuration
    if [ -n "$KDUMP_SMTP_SERVER" -a -n "$KDUMP_NOTIFICATION_TO" ]; then
	kdump_neednet=y
    fi

    # network explicitly disabled in configuration?
    [ -z "$KDUMP_NETCONFIG" ] && kdump_neednet=
}

check() {
    # Get configuration
    kdump_import_targets || return 1
    kdump_get_config || return 1
    kdump_check_net

    return 255
}

depends() {
    local -A _modules

    # drm is needed to get console output, but it is not included
    # automatically, because kdump does not use plymouth
    _modules[drm]=

    [ "$kdump_neednet" = y ] && _modules[network]=

    for protocol in "${kdump_Protocol[@]}" ; do
	if [ "$protocol" = "nfs" ]; then
	    _modules[nfs]=
	fi
	if [ "$protocol" = "cifs" -o "$protocol" = "smb" ]; then
            _modules[cifs]=
	fi
    done

    echo ${!_modules[@]}
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

kdump_cmdline_ip() {
    [ "$kdump_neednet" = y ] || return 0

    echo -n "rd.neednet=1"
    if [ "$KDUMP_NETCONFIG" = "auto" ] ; then
	echo -n " ip=any"
    else
	set -- ${KDUMP_NETCONFIG//:/ }
	local _if=$1
	local _mode=$2

	if [ "$_if" = "default" ] ; then
	    _if=$(kdump_default_netdev)
	fi
	printf " %s" $(kdump_ifname_config "$_if")

	if [ "$_mode" = "static" ] ; then
	    printf " %s" $(kdump_ip_config "$_if")
	else
	    echo -n " ip=${_if}:dhcp"
	fi
    fi
}

cmdline() {
    kdump_cmdline_ip
}

install() {
    if [[ $hostonly_cmdline == "yes" ]] ; then
        local _cmdline=$(cmdline)
        [ -n "$_cmdline" ] && printf "%s\n" "$_cmdline" >> "${initdir}/etc/cmdline.d/99kdump.conf"
    fi

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
