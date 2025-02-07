#! /bin/bash
#
#  Copyright Petr Tesarik <ptesarik@suse.com>
#  Load the kdump kernel 

KEXEC=/sbin/kexec
FADUMP_ENABLED=/sys/kernel/fadump/enabled
FADUMP_REGISTERED=/sys/kernel/fadump/registered
FADUMP_BOOTARGS_APPEND=/sys/kernel/fadump/bootargs_append
UDEV_RULES_DIR=/run/udev/rules.d
kdump_initrd=/var/lib/kdump/initrd
kdump_kernel=/var/lib/kdump/kernel

#
# Remove an option from the kernel command line
function remove_from_commandline()
{
    local option="$1"

    awk 'BEGIN { ORS="" }
    {
        while(length()) {
            match($0,/^([[:space:]]*)(.*)/,w);
            match(w[2],/(("[^"]*"?|[^"[:space:]])+)(.*)/,p);
            raw=p[1];
            gsub(/"/,"",raw);
            if (raw !~ /^('"$option"')(=|$)/)
                print w[1] p[1];
            $0=p[3];
        }
        print "\n";
    }'
}

# Construct console= parameter for kdump kernel from current Xen cmdline
function set_serial_console()
{
    local com1 com2 unhandled opt
    local maybe_console
    local kdump_console

    test -f /proc/xen/capabilities || return
    grep -wq 'control_d' /proc/xen/capabilities || return
    test -x "$(type -P xl)" || return

    read xen_commandline < <(xl info | awk -F ':' '/^xen_commandline/{print $2}')
    test -n "${xen_commandline}" || return

    for opt in ${xen_commandline}
    do
        unhandled=
        case "${opt}" in
        console=com1) maybe_console='ttyS0' ;;
        console=com2) maybe_console='ttyS1' ;;
        com1=*,*) unhandled=1 ; com1= ;;
        com1=*) com1="${opt#*=}" ;;
        com2=*,*) unhandled=1 ; com2=;;
        com2=*) com2="${opt#*=}" ;;
        console=*com*) unhandled=1 ; maybe_console= ;;
        *) ;;
        esac
        test -n "${unhandled}" && echo >&2 "${opt} in Xen commandline not handled"
    done

    test -n "${maybe_console}" || return

    case "${maybe_console}" in
    ttyS0)
        test -n "${com1}" && kdump_console="console=${maybe_console},${com1}"
    ;;
    ttyS1)
        test -n "${com2}" && kdump_console="console=${maybe_console},${com2}"
    ;;
    esac

    test -n "${kdump_console}" || return

    if test -n "${KDUMP_COMMANDLINE_APPEND}"
    then
        echo >&2 "KDUMP_COMMANDLINE_APPEND is set, assuming it already contains '${kdump_console}'"
        return
    fi

    echo "${kdump_console}"
}

#
# Builds the kdump command line from KDUMP_COMMANDLINE.
function build_kdump_commandline()
{
    local kdump_kernel="$1"
    local commandline="$KDUMP_COMMANDLINE"

    if [ -z "$commandline" ] ; then
        local nr_cpus
        commandline=$(
            remove_from_commandline \
                'root|resume|crashkernel|splash|mem|BOOT_IMAGE|showopts|zfcp\.allow_lun_scan|hugepages|acpi_no_memhotplug|cgroup_disable|unknown_nmi_panic|rd\.udev\.children-max' \
                < /proc/cmdline)
        if [ ${KDUMP_CPUS:-1} -ne 0 ] ; then
            nr_cpus="nr_cpus=${KDUMP_CPUS:-1}"
        fi
        commandline="$commandline sysrq=yes reset_devices acpi_no_memhotplug cgroup_disable=memory nokaslr"
        commandline="$commandline numa=off"
        commandline="$commandline irqpoll ${nr_cpus}"
        commandline="$commandline root=kdump rootflags=bind rd.udev.children-max=8"
        case $(uname -i) in
        i?86|x86_64)
            local boot_apicid=$(
                awk -F':[ \t]' '/^initial apicid[ \t]*:/ {print $2; exit}' \
                    /proc/cpuinfo)
            test -n "$boot_apicid" && \
		commandline="$commandline disable_cpu_apicid=$boot_apicid"
            commandline=$(echo "$commandline" |
                remove_from_commandline 'unknown_nmi_panic|notsc|console=hvc0')
            ;;
	s390*)
	    commandline="$commandline zfcp.allow_lun_scan=0"
	    ;;
        esac

        # Xen does not include the kernel release in VMCOREINFO_XEN
        # so we have to pass it via the command line
        if [[ $(uname -r) =~ -xen$ ]] ; then
            kernelrelease=$(uname -r)
            commandline="$commandline kernelversion=$kernelrelease"
        fi
    fi

    commandline="$commandline $(set_serial_console)"
    commandline="$commandline $KDUMP_COMMANDLINE_APPEND"

    # Add panic=1 unless there is a panic option already
    local cmdline_nopanic
    cmdline_nopanic=$(echo "$commandline" | remove_from_commandline 'panic')
    if [ "$commandline" = "$cmdline_nopanic" ] ; then
	commandline="$commandline panic=1"
    fi

    echo "$commandline"
}

#
# Load kdump using kexec
function load_kdump_kexec()
{
    local result
    local output

    local kdump_commandline=$(build_kdump_commandline "$kdump_kernel")

    KEXEC_CALL="$KEXEC -p $kdump_kernel --append=\"$kdump_commandline\" --initrd=$kdump_initrd $KEXEC_OPTIONS -a"

    $VERBOSE && echo "Starting kdump kernel load; kexec cmdline: $KEXEC_CALL"
    eval "$KEXEC_CALL"
    result=$?

    if [ $result -eq 0 ] ; then
        $VERBOSE && echo "Loaded kdump kernel."
        return 0
    fi

    echo "kexec failed." >&2
    return $result
}

#
# Checks if fadump is enabled
#
# Returns: 0 (true) if fadump is enabled
#          1 (false) if fadump is not availabled or disabled
function fadump_enabled()
{
    test -f "$FADUMP_ENABLED" && test $(cat "$FADUMP_ENABLED") = "1"
}

#
# Update the bootloader options for fadump
function fadump_bootloader()
{
    case $(uname -m) in
	ppc*) ;;
	*) return 0 ;;
    esac

    local newstate="$1"

    # check if the old configuration is still valid
    local boot_opts=$(pbl --get-option fadump)
    if [ "$newstate" = on ] ; then
        # if makedumpfile not configured to filter out user pages (dump level), 
        # or makedumpfile is not used (raw format),
        # set fadump=nocma
        [[ $((${KDUMP_DUMPLEVEL} & 8)) -eq 0 ]] && newstate=nocma
        [[ ${KDUMP_DUMPFORMAT} = raw ]] && newstate=nocma

        if [ "$boot_opts" != "fadump=$newstate" ] ; then
            pbl --add-option fadump=$newstate --config
        fi
    elif [ -n "$boot_opts" ] ; then
        pbl --del-option fadump --config
    fi
}

#
# Pass additional parameters to FADump capture kernel.
function fadump_append_params()
{
	local nr_cpus
        if [ ${KDUMP_CPUS:-1} -ne 0 ] ; then
            nr_cpus="nr_cpus=${KDUMP_CPUS:-1}"
        fi
	
	FADUMP_COMMANDLINE_APPEND="$nr_cpus $FADUMP_COMMANDLINE_APPEND"

	if [[ -f $FADUMP_BOOTARGS_APPEND ]] &&
	   echo "$FADUMP_COMMANDLINE_APPEND" > "$FADUMP_BOOTARGS_APPEND"; then
        	echo "Additional parameters for fadump kernel: $FADUMP_COMMANDLINE_APPEND"
	else
        	echo "WARNING: Failed to setup additional parameters for fadump capture kernel: $FADUMP_COMMANDLINE_APPEND"
	fi
}

#
# Update fadump configuration
function load_kdump_fadump()
{
    fadump_bootloader on

    if ! fadump_enabled ; then
        echo "fadump is not enabled in the kernel."
	return 5
    fi

    local msg
    local result=0
    local output

    # Pass additional parameters to FADump capture kernel.
    fadump_append_params

    # Re-registering of FADump is supported in kernel (see bsc#1108170).
    # So, don't bother about whether FADump was registered already
    output=$( (echo 1 > "$FADUMP_REGISTERED") 2>&1)
    if [ $? -eq 0 ] ; then
        msg="Registered fadump"
    else
        msg="FAILED to register fadump: $output"
        result=1

        # For re-registering on an older kernel that returns -EEXIST/-EINVAL,
        # if fadump was already registered.
        if [ "$(cat $FADUMP_REGISTERED)" == "1" ] ; then
            echo 0 > "$FADUMP_REGISTERED"
            output=$( (echo 1 > "$FADUMP_REGISTERED") 2>&1)
            if [ $? -eq 0 ] ; then
                msg="Registered fadump"
                result=0
            else
                msg="FAILED to register fadump: $output"
            fi
        fi
    fi

    echo "$msg"

    return $result
}

#
# Rebuild the kdump initramfs if necessary
function rebuild_kdumprd()
{
    local output
    output=$(mkdumprd 2>&1)
    if [ $? -ne 0 ] ; then
	echo "$output"
	return 1
    fi

    return 0
}

############################################################
# MAIN PROGRAM STARTS HERE
#

. /usr/lib/kdump/kdump-read-config.sh

VERBOSE=false
[[ $((${KDUMP_VERBOSE:-0} & 5)) -gt 0 ]] && VERBOSE=true

update=
shrink=
while [ $# -gt 0 ] ; do
    case "$1" in
        --update)
            update=yes
            ;;
        --shrink)
            shrink="$KDUMP_AUTO_RESIZE"
            ;;
        *)
            echo "Unknown option: $1" >&2
            exit 1
            ;;
    esac
    shift
done


if [ "$update" = yes ] ; then
    rebuild_kdumprd || exit 1
fi

if [ "$shrink" = true ] ; then
    kdumptool calibrate --shrink > /dev/null
fi

if [ "$KDUMP_FADUMP" = "true" ] ; then
    load_kdump_fadump
else
    fadump_bootloader off

    # check if initrd and the kernel it was built for exist
    # return 6 if not, which is treated as success by 
    # the kdump-early service
    # FIXME: This is a fragile SELinux workaround: use /bin/test instead of [[ ]] 
    # because it runs with a different label which by coincidence
    # is allowed to dereference the symlink
    # see bsc#1213721
    #[[ -f "${kdump_initrd}" ]] || exit 6
    #[[ -f "${kdump_kernel}" ]] || exit 6
    /usr/bin/test -f "${kdump_initrd}" || exit 6
    /usr/bin/test -f "${kdump_kernel}" || exit 6

    load_kdump_kexec
fi
result=$?

if [ $result = 0 ] ; then
    echo 1 > /proc/sys/kernel/panic_on_oops
    mkdir -p "$UDEV_RULES_DIR"
    cp /usr/lib/kdump/70-kdump.rules "$UDEV_RULES_DIR"/70-kdump.rules
fi

exit $result
