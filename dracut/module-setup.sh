#!/bin/bash
# module-setup.sh for kdump dracut module

test -n "$KDUMP_LIBDIR" || KDUMP_LIBDIR=/usr/lib/kdump

check() {
	# Building a kdump initrd?
	if ! [[ " $dracutmodules $add_dracutmodules $force_add_dracutmodules" == *\ $_mod\ * ]]; then
		# Do not include kdump by default
		return 1
	fi

	. "$KDUMP_LIBDIR"/kdump-read-config.sh || return 1
	. "$moddir"/setup-kdump.functions || return 1

	# no network needed by default
	kdump_neednet=

	# check if fence_kdump_send is installed and configured
	if [ -f "$FENCE_KDUMP_SEND" ] &&
	   [[ "$KDUMP_POSTSCRIPT" =~ "$FENCE_KDUMP_SEND" ]] ; then
		# add fence_kdump_send to initrd automatically
		KDUMP_REQUIRED_PROGRAMS="$KDUMP_REQUIRED_PROGRAMS $FENCE_KDUMP_SEND"
		# set up network (unless explicitly disabled by KDUMP_NETCONFIG)
		kdump_neednet=y
	fi

	# check if network is needed
	if [ -z "$KDUMP_NETCONFIG" ]; then
		# network explicitly disabled in configuration
		kdump_neednet=
	elif [ "${KDUMP_NETCONFIG%:force}" != "$KDUMP_NETCONFIG" ]; then
		# always set up network
		kdump_neednet=y
	else
		# everything other than "file" needs network
		[[ ${KDUMP_PROTO} == file ]] || kdump_neednet=y
	fi

	return 0
}

depends() {
	local -A _modules

	# drm is needed to get console output, but it is not included
	# automatically, because kdump does not use plymouth
	_modules[drm]=

	[ "$kdump_neednet" = y ] && _modules[network]=

	local _mod=watchdog-modules
	if [ -e "$dracutsysrootdir$dracutbasedir"/modules.d/??$_mod ]; then
		_modules[$_mod]=
	fi

	echo ${!_modules[@]}
}

kdump_ip_set_explicitly() {
	local _opt
	local opts

	if [ "$KDUMP_FADUMP" = true ]; then
		_opts="`cat /proc/cmdline`"
	else
		_opts="$KDUMP_COMMANDLINE $KDUMP_COMMANDLINE_APPEND"
	fi

	for _opt in $_opts
	do
		if [ "${_opt%%=*}" = "ip" -a \
			 "${_opt#*=}" != "$_opt" ]
		then
			return 0
		fi
	done
	return 1
}

# Output variables:
#	kdump_kmods  additional kernel modules updated
#	kdump_ifmap  hardware network interface map updated
kdump_cmdline_ip() {
	[ "$kdump_neednet" = y ] || return 0

	kdump_ip_set_explicitly && return 0

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

installkernel() {
	[ -n "$kdump_kmods" ] || return 0
	hostonly='' instmods $kdump_kmods
}

install() {
	# generate command line for network configuration
	kdump_cmdline_ip > "${initdir}/etc/cmdline.d/99kdump-net.conf"
	[[ -s "${initdir}/etc/cmdline.d/99kdump-net.conf" ]] || rm "${initdir}/etc/cmdline.d/99kdump-net.conf"

	# parse the configuration, check values and initialize defaults 
	"$KDUMP_LIBDIR"/kdump-read-config.sh --print > ${initdir}/etc/kdump.conf
	# remember the host name
	hostname >> "${initdir}/etc/hostname.kdump"
	# filter out problematic sysctl settings
	kdump_filter_sysctl "${initdir}" #FIXME needed?

	inst_hook cmdline 50 "$moddir/kdump-root.sh"
	inst_script "$moddir"/kdump-save /kdump/kdump-save
	if dracut_module_included "systemd" ; then
		inst_simple "$moddir/kdump-save.service" "$systemdsystemunitdir/kdump-save.service"

		mkdir -p "$initdir/$systemdsystemunitdir"/initrd.target.wants
		ln_r "$systemdsystemunitdir"/kdump-save.service \
			"$systemdsystemunitdir"/initrd.target.wants/kdump-save.service
	else
		inst_hook mount 30 "$moddir/mount-kdump.sh"
		inst_hook pre-pivot 90 "$KDUMP_LIBDIR"/kdump-save
	fi
	
	# per-protocol config
	case ${KDUMP_PROTO} in
		ssh)
			if ! inst_multiple ssh; then
				dfatal "Kdump needs ssh for ${KDUMP_SAVEDIR}."
				exit 1
			fi

			[[ ${KDUMP_NET_TIMEOUT} -gt 0 ]] && inst_multiple ping
			kdump_init_ssh
			;;
		ftp)
			if ! inst_multiple lftp /usr/lib64/lftp/*/proto-ftp.so; then 
				dfatal "Kdump needs lftp for ${KDUMP_SAVEDIR}."
				exit 1
			fi
			[[ ${KDUMP_NET_TIMEOUT} -gt 0 ]] && inst_multiple ping
			;;
		sftp)
			if ! inst_multiple lftp /usr/lib64/lftp/*/proto-sftp.so ssh; then
				dfatal "Kdump needs lftp and ssh for ${KDUMP_SAVEDIR}."
				exit 1
			fi
			[[ ${KDUMP_NET_TIMEOUT} -gt 0 ]] && inst_multiple ping
			kdump_init_ssh
			;;
		file)
			[[ ${KDUMP_FREE_DISK_SIZE} -gt 0 ]] && inst_multiple df
			
			# dereference symlinks, because they might not work in the
			# kdump environment when the directory is mounted elsewhere
			KDUMP_SAVEDIR_REALPATH=$(realpath -m "${KDUMP_SAVEDIR#*://}")
			echo "KDUMP_SAVEDIR='${KDUMP_SAVEDIR_REALPATH//\'/\'\\\'\'}'" >> ${initdir}/etc/kdump.conf
			;;
	esac

	inst_multiple makedumpfile date sleep $KDUMP_REQUIRED_PROGRAMS
	
	if [ "$kdump_neednet" = y ]; then
		# Install /etc/resolv.conf to provide initial DNS configuration. The file
		# is resolved first to install directly the target file if it is a symlink.
		local resolv=$(realpath /etc/resolv.conf)
		inst_simple "$resolv" /etc/resolv.conf
		inst_simple /etc/hosts
		# set up hardware interfaces
		kdump_setup_hwif "${initdir}"
	fi
	inst_simple /etc/hostname
	inst_simple /usr/share/zoneinfo/UTC
	inst_simple /etc/localtime
}

function kdump_init_ssh()						   # {{{
{
	local SSH_DIR="${initdir}/root/.ssh"
	mkdir -m 0700 "${SSH_DIR}"

	URL=${KDUMP_SAVEDIR#*://}
	HOST="${URL%%/*}"
	HOST="${HOST#*@}"

	# host keys
	if [[ "${KDUMP_HOST_KEY}" == "*" ]]; then
		echo "StrictHostKeyChecking no" >> "${SSH_DIR}/config"
	else 
		echo "StrictHostKeyChecking yes" >> "${SSH_DIR}/config"
		if [[ -z "${KDUMP_HOST_KEY}" ]]; then
			if ! ssh-keygen -F "${HOST}" > "${SSH_DIR}/known_hosts"; then
			       dfatal "SSH host key neither specified in KDUMP_HOST_KEY nor found by 'ssh-keygen -F $HOST'; SSH host key checking will fail."
			       exit 1
			fi
		else
			echo "${HOST} ${KDUMP_HOST_KEY}" > "${SSH_DIR}/known_hosts"
		fi
	fi

	# identity files
	pushd ~root/.ssh >/dev/null #identity files are relative to this directory
	[[ -z "${KDUMP_SSH_IDENTITY}" ]] && KDUMP_SSH_IDENTITY="id_rsa id_dsa id_ecdsa id_ed25519"
	for i in ${KDUMP_SSH_IDENTITY}; do
		[[ -f "$i" ]] || continue
		cp "$i" "${SSH_DIR}/"
		[[ -f "${i}.pub" ]] && cp "${i}.pub" "${SSH_DIR}/"
		[[ -f "${i}-cert.pub" ]] && cp "${i}-cert.pub" "${SSH_DIR}/"
		f=$(basename $i)
		echo "IdentityFile /root/.ssh/${f}" >> "${SSH_DIR}/config"
	done
	popd >/dev/null
}
