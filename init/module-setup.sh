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

    [ "$kdump_neednet" = y ] || return 0

    if [ "$KDUMP_NETCONFIG" = "auto" ] ; then
	kdump_host_if=default
	kdump_net_mode=auto
    else
	set -- ${KDUMP_NETCONFIG//:/ }
	kdump_host_if=$1
	kdump_net_mode=$2
    fi

    if [ "$kdump_host_if" = "default" ] ; then
	kdump_host_if=$(kdump_default_netdev)
    fi
    if [ "$kdump_net_mode" = "auto" ] ; then
	kdump_net_mode=$(kdump_netdev_mode "$kdump_host_if")
    fi

    kdump_ifname_config "$kdump_host_if"
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
    echo -n "$kdump_netif"

    case "$kdump_net_mode" in
	static)
	    printf " %s" \
		$(kdump_ip_config "$kdump_host_if" "$kdump_iface") \
		$(kdump_ip6_config "$kdump_host_if" "$kdump_iface")
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
	    derror "Wrong KDUMP_NETCONFIG mode: $kdump_net_mode"
	    ;;
    esac
}

kdump_gen_mount_units() {
    local line
    local fstab="$initdir/etc/fstab"

    [ -e "$fstab" ] && mv "$fstab" "$fstab.kdumpsave"
    for line in "${fstab_lines[@]}"
    do
	line=($line)
	[ "${line[1]#/kdump}" = "${line[1]}" ] && continue
	[ -z "${line[3]}" ] && line[3]="defaults"
	[ -z "${line[4]}" ] && line[4]="0"
	[ -z "${line[5]}" ] && line[5]="2"
	echo "${line[@]}" >> "$fstab"
    done

    echo "root=kdump" > "$initdir/proc/cmdline"
    inst_binary -l \
	"$systemdutildir/system-generators/systemd-fstab-generator" \
	"/tmp/systemd-fstab-generator"
    chroot "$initdir" "/tmp/systemd-fstab-generator" \
	"$systemdsystemunitdir" \
	"$systemdsystemunitdir" \
	"$systemdsystemunitdir"
    rm -f "$initdir/tmp/systemd-fstab-generator"
    rm -f "$initdir/proc/cmdline"

    if [ -e "$fstab.kdumpsave" ]; then
	mv "$fstab.kdumpsave" "$fstab"
    else
	rm "$fstab"
    fi
}

cmdline() {
    kdump_cmdline_ip
}

installkernel() {
    [ -n "$kdump_kmods" ] || return 0
    hostonly='' instmods $kdump_kmods
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

	kdump_gen_mount_units
    else
	[ "$KDUMP_FADUMP" != yes ] && \
	    inst_hook mount 30 "$moddir/mount-kdump.sh"

	inst_hook pre-pivot 90 /lib/kdump/save_dump.sh
    fi

    inst_multiple makedumpfile makedumpfile-R.pl kdumptool \
	$KDUMP_REQUIRED_PROGRAMS
    inst_simple /etc/resolv.conf
}
