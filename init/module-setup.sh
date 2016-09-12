#!/bin/bash
# module-setup.sh for kdump dracut module

. /lib/kdump/setup-kdump.functions

kdump_check_net() {
    kdump_neednet=
    for protocol in "${kdump_Protocol[@]}" ; do
	if [ "$protocol" != "file" -a "$protocol" != "srcfile" ]; then
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

    local _if _mode
    if [ "$KDUMP_NETCONFIG" = "auto" ] ; then
	_if=default
	_mode=auto
    else
	set -- ${KDUMP_NETCONFIG//:/ }
	local _if=$1
	local _mode=$2
    fi

    [ "$_if" = "default" ] && _if=$(kdump_default_netdev)
    [ "$_mode" = "auto" ] && _mode=$(kdump_netdev_mode "$_if")

    kdump_ifname_config "$_if"
    echo -n "$kdump_netif"

    case "$_mode" in
	static)
	    printf " %s" \
		$(kdump_ip_config "$_if" "$kdump_iface") \
		$(kdump_ip6_config "$_if" "$kdump_iface")
	    ;;
	dhcp|dhcp4)
	    echo " ip=${kdump_iface}:dhcp"
	    ;;
	dhcp6)
	    echo " ip=${kdump_iface}:dhcp6"
	    ;;
	auto6)
	    echo " ip=${kdump_iface}:auto6"
	    ;;
	*)
	    derror "Wrong KDUMP_NETCONFIG mode: $_mode"
	    ;;
    esac
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

    # Convert system root mounts to bind mounts
    if [ "$KDUMP_FADUMP" = "yes" ] ; then
	local i line
	for i in "${!fstab_lines[@]}"
	do
	    line=( ${fstab_lines[i]} )
	    if [ "${line[1]%/*}" = "/kdump" ] ; then
		fstab_lines[i]="/sysroot ${line[1]} none bind"
	    fi
	done
    fi

    kdump_setup_files "$initdir" "$kdump_mpath_wwids"

    inst_hook cmdline 50 "$moddir/kdump-root.sh"
    if dracut_module_included "systemd" ; then
	inst_binary "$moddir/device-timeout-generator" \
	    "$systemdutildir"/system-generators/kdump-device-timeout-generator

	inst_simple /lib/kdump/save_dump.sh
	awk -v mountpoints="${kdump_mnt[*]}" '{
		gsub(/@KDUMP_MOUNTPOINTS@/, mountpoints)
		print
	    }' "$moddir/kdump-save.service.in" > \
	    "$initdir/$systemdsystemunitdir"/kdump-save.service
	ln_r "$systemdsystemunitdir"/kdump-save.service \
	    "$systemdsystemunitdir"/initrd.target.wants/kdump-save.service
    else
	[ "$KDUMP_FADUMP" != yes ] && \
	    inst_hook mount 30 "$moddir/mount-kdump.sh"

	inst_hook pre-pivot 90 /lib/kdump/save_dump.sh
    fi

    inst_multiple makedumpfile makedumpfile-R.pl kdumptool \
	$KDUMP_REQUIRED_PROGRAMS
    inst_simple /etc/resolv.conf
}
