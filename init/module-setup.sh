#!/bin/bash
# module-setup.sh for kdump dracut module

. /lib/kdump/setup-kdump.functions

kdump_needed() {
    # Building a kdump initrd?
    if [[ " $dracutmodules $add_dracutmodules $force_add_dracutmodules" == *\ $_mod\ * ]]; then
        return 0
    fi

    # Is FADUMP active?
    if [ "$KDUMP_FADUMP" = "yes" ]; then
        return 0
    fi

    # Do not include kdump by default
    return 1
}

kdump_check_net() {
    if [ -z "$KDUMP_NETCONFIG" ]; then
        # network explicitly disabled in configuration
        kdump_neednet=
    elif [ "${KDUMP_NETCONFIG%:force}" != "$KDUMP_NETCONFIG" ]; then
        # always set up network
        kdump_neednet=y
    else
        for protocol in "${kdump_Protocol[@]}" ; do
	    if [ "$protocol" != "file" -a "$protocol" != "srcfile" ]; then
	        kdump_neednet=y
	    fi
        done

        # network configuration
        if [ -n "$KDUMP_SMTP_SERVER" -a -n "$KDUMP_NOTIFICATION_TO" ]; then
	    kdump_neednet=y
        fi
    fi
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
        UUID=*|LABEL=*|PARTUUID=*|PARTLABEL=*)
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
    _dev=$(kdump_mount_dev "${kdump_dev[_idx]}")
    echo "$_dev ${kdump_mnt[_idx]} ${kdump_fstype[_idx]} ${kdump_opts[_idx]} 0 $_passno" >> "$initdir/etc/fstab"
}

check() {
    # Get configuration
    kdump_get_config || return 1

    kdump_needed || return 1

    # no network needed by default
    kdump_neednet=

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

    # check if fence_kdump_send is installed and configured
    if [ -f "$FENCE_KDUMP_SEND" ] &&
           [[ "$KDUMP_POSTSCRIPT" =~ "$FENCE_KDUMP_SEND" ]] ; then
        # add fence_kdump_send to initrd automatically
        KDUMP_REQUIRED_PROGRAMS="$KDUMP_REQUIRED_PROGRAMS $FENCE_KDUMP_SEND"
        # set up network (unless explicitly disabled by KDUMP_NETCONFIG)
        kdump_neednet=y
    fi

    kdump_check_net

    return 0
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

    local _cfg="${KDUMP_NETCONFIG%:force}"
    if [ "$_cfg" = "auto" ] ; then
	kdump_host_if=default
	kdump_net_mode=auto
    else
	kdump_host_if="${_cfg%%:*}"
	kdump_net_mode="${_cfg#*:}"
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

cmdline_zfcp() {
    local _arch=$(uname -m)
    [ "$_arch" = "s390" -o "$_arch" = "s390x" ] && kdump_cmdline_zfcp
}

cmdline_net() {
    kdump_cmdline_ip
}

installkernel() {
    [ -n "$kdump_kmods" ] || return 0
    hostonly='' instmods $kdump_kmods
}

install() {
    if [[ $hostonly_cmdline == "yes" ]] ; then
        local _cmdline=$(cmdline_zfcp)
        [ -n "$_cmdline" ] && printf "%s\n" "$_cmdline" >> "${initdir}/etc/cmdline.d/99kdump-zfcp.conf"

        _cmdline=$(cmdline_net)
        [ -n "$_cmdline" ] && printf "%s\n" "$_cmdline" >> "${initdir}/etc/cmdline.d/99kdump-net.conf"
    fi

    # Get a list of required multipath devices
    local kdump_mpath_wwids
    kdump_map_mpath_wwid
    for_each_host_dev_and_slaves_all kdump_add_mpath_dev

    kdump_setup_files "$initdir" "$kdump_mpath_wwids"

    inst_hook cmdline 50 "$moddir/kdump-root.sh"
    inst_hook cmdline 50 "$moddir/kdump-boot.sh"
    if dracut_module_included "systemd" ; then
	inst_binary "$moddir/device-timeout-generator" \
	    "$systemdutildir"/system-generators/kdump-device-timeout-generator

	inst_simple /lib/kdump/save_dump.sh
	awk -v mountpoints="${kdump_mnt[*]}" '{
		gsub(/@KDUMP_MOUNTPOINTS@/, mountpoints)
		print
	    }' "$moddir/kdump-save.service.in" > \
	        "$initdir/$systemdsystemunitdir"/kdump-save.service

        local _d _mp
        local _mnt
        _d="$initdir/$systemdsystemunitdir"/initrd-switch-root.target.d
        mkdir -p "$_d"
        (
            echo "[Unit]"
            for _mp in "${kdump_mnt[@]}" ; do
                _mnt=$(systemd-escape -p --suffix=mount "$_mp")
                _d="$initdir/$systemdsystemunitdir/$_mnt".d
                mkdir -p "$_d"
                echo -e "[Unit]\nConditionPathExists=/proc/vmcore" \
                     > "$_d"/kdump.conf
                echo "Conflicts=$_mnt"
            done
        ) > "$_d"/kdump.conf

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
    inst_simple /usr/share/zoneinfo/UTC
    inst_simple /etc/localtime
}
