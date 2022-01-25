#! /bin/bash
#
#  Copyright Petr Tesarik <ptesarik@suse.com>
#  Load the kdump kernel specified in /etc/sysconfig/kdump

KDUMPTOOL=/usr/sbin/kdumptool
KEXEC=/sbin/kexec
FADUMP_ENABLED=/sys/kernel/fadump_enabled
FADUMP_REGISTERED=/sys/kernel/fadump_registered
UDEV_RULES_DIR=/run/udev/rules.d

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
# Get the name of kernel parameter to limit CPUs
# Linux 2.6.34+ has nr_cpus, older versions must use maxcpus
function cpus_param()
{
    # nr_cpus does not work properly on ppc/ppc64/ppc64le
    case $(uname -m) in
        ppc*)
	    echo maxcpus
	    return 0
	    ;;
    esac

    local version=$(get_kernel_version "$1")
    version="${version%%-*}"
    local verhex=$(IFS=. ; set -- $version ; printf "0x%02x%02x%02x" $1 $2 $3)
    if [ $(( $verhex )) -ge $(( 0x020622 )) ] ; then
	echo nr_cpus
    else
	echo maxcpus
    fi
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
            nr_cpus=$(cpus_param "$kdump_kernel")=${KDUMP_CPUS:-1}
        fi
        # Use deadline for saving the memory footprint
        commandline="$commandline elevator=deadline sysrq=yes reset_devices acpi_no_memhotplug cgroup_disable=memory nokaslr"
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
# Builds the kexec options from KEXEC_OPTIONS
# Parameters: 1) kernel
function build_kexec_options()
{
    local kdump_kernel="$1"
    local options="$KEXEC_OPTIONS"

    # remove `--args-linux' for x86 type kernel files here
    if [ $($KDUMPTOOL identify_kernel -t "$kdump_kernel") = "x86" ] ; then
        options=$(echo "$options" | sed -e 's/--args-linux//g')
    fi

    # add --noio on ia64
    if [ $(uname -i) = "ia64" ] ; then
        options="$options --noio"
    fi

    echo "$options"
}

#
# Load kdump using kexec
function load_kdump_kexec()
{
    local result
    local output

    if [ ! -f "$kdump_initrd" ] ; then
        echo "No kdump initial ramdisk found. Tried to locate $kdump_initrd."
	return 6
    fi

    local kdump_commandline=$(build_kdump_commandline "$kdump_kernel")
    local kexec_options=$(build_kexec_options "$kdump_kernel")

    KEXEC_CALL="$KEXEC -p $kdump_kernel --append=\"$kdump_commandline\""
    KEXEC_CALL="$KEXEC_CALL --initrd=$kdump_initrd $kexec_options"

    kdump_echo "Starting load kdump kernel with kexec_file_load(2)"
    kdump_echo "kexec cmdline: $KEXEC_CALL -s"

    output=$(eval "$KEXEC_CALL -s" 2>&1)
    result=$?

    # print stderr in any case to show warnings that normally
    # would be supressed (bnc#374185)
    echo "$output"

    if [ $result -eq 0 ] ; then
        kdump_logger "Loaded kdump kernel: $KEXEC_CALL -s, Result: $output"
        return 0
    fi

    # kexec_file_load(2) failed
    kdump_echo "kexec_file_load(2) failed"
    kdump_echo "Starting load kdump kernel with kexec_load(2)"
    kdump_echo "kexec cmdline: $KEXEC_CALL"

    output=$(eval "$KEXEC_CALL" 2>&1)
    result=$?
    echo "$output"

    if [ $result -eq 0 ] ; then
        kdump_logger "Loaded kdump kernel: $KEXEC_CALL, Result: $output"
    else
        kdump_logger "FAILED to load kdump kernel: $KEXEC_CALL, Result: $output"
    fi

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
        if [ "$boot_opts" != "fadump=on" ] ; then
            pbl --add-option fadump=on --config
        fi
    elif [ -n "$boot_opts" ] ; then
        pbl --del-option fadump --config
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
    if [ $((${KDUMP_VERBOSE:-0} & 1)) -gt 0 ] ; then
	logger -t kdump "$msg"
    fi

    return $result
}

#
# Find the desired kernel and initrd
function find_kernel()
{
    local output
    output=$($KDUMPTOOL find_kernel)
    test $? -eq 0 || return 1

    kdump_kernel=$(echo "$output" | grep ^Kernel | cut -f 2)
    kdump_initrd=$(echo "$output" | grep ^Initrd | cut -f 2)

    return 0
}

#
# Rebuild the kdump initramfs if necessary
function rebuild_kdumprd()
{
    local output
    output=$(mkdumprd -K "$kdump_kernel" -I "$kdump_initrd" 2>&1)
    if [ $? -ne 0 ] ; then
	echo "$output"
	return 1
    fi

    return 0
}

############################################################
# MAIN PROGRAM STARTS HERE
#

eval $($KDUMPTOOL dump_config)

if [ $((${KDUMP_VERBOSE:-0} & 4)) -gt 0 ] ; then
    function kdump_echo()
    {
        echo "$@"
    }
else
    function kdump_echo(){ :; }
fi

if [ $((${KDUMP_VERBOSE:-0} & 1)) -gt 0 ] ; then
    function kdump_logger()
    {
        logger -t kdump "$@"
    }
else
    function kdump_logger(){ :; }
fi

if [ $((${KDUMP_VERBOSE:-0} & 16)) -gt 0 ] ; then
    KDUMPTOOL="$KDUMPTOOL -D"
fi

update=
shrink=
while [ $# -gt 0 ] ; do
    case "$1" in
        --update)
            update=yes
            ;;
        --shrink)
            shrink=yes
            ;;
        *)
            echo "Unknown option: $1" >&2
            exit 1
            ;;
    esac
    shift
done

find_kernel || exit 6

if [ "$update" = yes ] ; then
    rebuild_kdumprd || exit 1
fi

if [ "$shrink" = yes ] ; then
    $KDUMPTOOL calibrate --shrink > /dev/null
fi

if [ "$KDUMP_FADUMP" = "yes" ] ; then
    load_kdump_fadump
else
    fadump_bootloader off
    load_kdump_kexec
fi
result=$?

if [ $result = 0 ] ; then
    echo 1 > /proc/sys/kernel/panic_on_oops
    mkdir -p "$UDEV_RULES_DIR"
    cp /usr/lib/kdump/70-kdump.rules "$UDEV_RULES_DIR"/70-kdump.rules
fi

exit $result
