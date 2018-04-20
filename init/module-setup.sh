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

kdump_get_fs_type() {
    local _dev="/dev/block/$1"
    local _fstype
    if [ -b "$_dev" ] && _fstype=$(get_fs_env "$_dev") ; then
        host_fs_types["$(readlink -f "$_dev")"]="$_fstype"
    elif _fstype=$(find_dev_fstype "$_dev"); then
        host_fs_types["$_dev"]="$_fstype"
    fi
    return 1
}

kdump_add_host_dev() {
    local _dev=$1
    [[ " ${host_devs[@]} " == *" $_dev "* ]] && return
    host_devs+=( "$_dev" )
    check_block_and_slaves_all kdump_get_fs_type "$(get_maj_min "$_dev")"
}

kdump_add_mnt() {
    local _idx=$1
    local _dev="${kdump_dev[_idx]}"
    local _mp="${kdump_mnt[_idx]}"

    # Convert system root mounts to bind mounts
    if [ "$KDUMP_FADUMP" = "yes" -a "${_mp%/*}" = "/kdump" ] ; then
        mkdir -p "$initdir/etc"
        echo "/sysroot $_mp none bind 0 0" >> "$initdir/etc/fstab"
        return
    fi

    case "$_dev" in
        UUID=*|LABEL=*|PARTUUID=*|PARTLABLE=*)
            _dev=$(blkid -l -t "$_dev" -o device)
            ;;
    esac
    kdump_add_host_dev "$_dev"
    host_fs_types["$_dev"]="${kdump_fstype[_idx]}"
    if [ "${kdump_fstype[_idx]}" = btrfs ] ; then
        for _dev in $() ; do
            kdump_add_host_dev "$_dev"
        done
    fi

    mkdir -p "$initdir/etc"
    local _passno=2
    [ "${kdump_fstype[_idx]}" = nfs ] && _passno=0
    echo "${kdump_dev[_idx]} ${kdump_mnt[_idx]} ${kdump_fstype[_idx]} ${kdump_opts[_idx]} 0 $_passno" >> "$initdir/etc/fstab"
}

check() {
    # Get configuration
    kdump_get_config || return 1

    # add mount points
    if ! [[ $mount_needs ]] ; then
        kdump_get_mountpoints || return 1

        local _i=0
        while [ $_i -lt ${#kdump_mnt[@]} ]
        do
	    [ -n "${kdump_mnt[_i]}" ] && kdump_add_mnt $_i
	    _i=$((_i+1))
        done
    fi

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

kdump_cmdline_zfcp() {
    is_zfcp() {
        local _dev=$1
        local _devpath=$(cd -P /sys/dev/block/$_dev ; echo $PWD)
        local _sdev _lun _wwpn _ccw

        [ "${_devpath#*/sd}" == "$_devpath" ] && return 1
        _sdev="${_devpath%%/block/*}"
        [ -e ${_sdev}/fcp_lun ] || return 1
        _ccw=$(cat ${_sdev}/hba_id)
        _lun=$(cat ${_sdev}/fcp_lun)
        _wwpn=$(cat ${_sdev}/wwpn)
        echo "rd.zfcp=${_ccw},${_wwpn},${_lun}"
    }
    [[ $hostonly ]] || [[ $mount_needs ]] && {
        for_each_host_dev_and_slaves_all is_zfcp
    } | sort -u
}

kdump_cmdline_ip() {
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
    [ -n "$kdump_host_if" ] || return 1

    if [ "$kdump_net_mode" = "auto" ] ; then
	kdump_net_mode=$(kdump_netdev_mode "$kdump_host_if")
    fi

    kdump_ifname_config "$kdump_host_if"

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

    echo > "$initdir/proc/cmdline"
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
    local _arch=$(uname -m)
    [ "$_arch" = "s390" -o "$_arch" = "s390x" ] && kdump_cmdline_zfcp

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
    inst_simple /usr/share/zoneinfo/UTC
    inst_simple /etc/localtime
}
