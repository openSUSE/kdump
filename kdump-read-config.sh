#!/bin/bash

# Sanitize kdump config options and set their default values.
# May be sourced to set the options in the current shell.

# source the config file
. /etc/sysconfig/kdump

function kdump_read_config_main() 
{
	# define all kdump config options and their defaults here:
	option bool 	 KDUMP_AUTO_RESIZE false
	option string 	 KDUMP_COMMANDLINE ""
	option string 	 KDUMP_COMMANDLINE_APPEND ""
	option bool 	 KDUMP_CONTINUE_ON_ERROR true
	option int 	 KDUMP_CPUS 1
	option string 	 KDUMP_DUMPFORMAT "compressed"
	option int 	 KDUMP_DUMPLEVEL 31
	option bool 	 KDUMP_FADUMP false
	option bool 	 KDUMP_FADUMP_SHELL false
	option int 	 KDUMP_FREE_DISK_SIZE 64
	option string 	 KDUMP_HOST_KEY ""
	option bool 	 KDUMP_IMMEDIATE_REBOOT true
	option int 	 KDUMP_KEEP_OLD_DUMPS 0
	option string 	 KDUMP_KERNELVER ""
	option string 	 KDUMP_NETCONFIG "auto"
	option int 	 KDUMP_NET_TIMEOUT 30
	#option string 	 KDUMP_NOTIFICATION_CC ""	#not implemented yet
	option deprecated KDUMP_NOTIFICATION_CC
	#option string 	 KDUMP_NOTIFICATION_TO ""	#not implemented yet
	option deprecated KDUMP_NOTIFICATION_TO
	option string 	 KDUMP_POSTSCRIPT ""
	option string 	 KDUMP_PRESCRIPT ""
	option string 	 KDUMP_REQUIRED_PROGRAMS ""
	option string 	 KDUMP_SAVEDIR "/var/crash"
	#option string 	 KDUMP_SMTP_PASSWORD ""		#not implemented yet
	option deprecated KDUMP_SMTP_PASSWORD
	#option string 	 KDUMP_SMTP_SERVER ""		#not implemented yet
	option deprecated KDUMP_SMTP_SERVER
	#option string 	 KDUMP_SMTP_USER ""		#not implemented yet
	option deprecated KDUMP_SMTP_USER
	option string 	 KDUMP_SSH_IDENTITY ""
	option string 	 KDUMP_TRANSFER ""
	option int 	 KDUMP_VERBOSE 0
	option string 	 KEXEC_OPTIONS ""
	option string 	 MAKEDUMPFILE_OPTIONS ""
	
	option deprecated KDUMPTOOL_FLAGS ""
	option deprecated KDUMP_COPY_KERNEL ""
	
	
	# parse the protocol from KDUMP_SAVEDIR into a KDUMP_PROTO
	# (avoiding the below code in all scripts)
	case "${KDUMP_SAVEDIR,,}" in
		/*|file://*)
			KDUMP_PROTO=file
			;;
		ftp://*)
			KDUMP_PROTO=ftp
			;;
		scp://*|sftp://*)
			KDUMP_PROTO=sftp
			;;
		ssh://*)
			KDUMP_PROTO=ssh
			;;
		nfs://*)
			KDUMP_PROTO=nfs
			;;
		smb://*|cifs://*)
			KDUMP_PROTO=cifs
			;;
		*)
			KDUMP_PROTO=""
			echo "KDUMP_SAVEDIR (${KDUMP_SAVEDIR}) is invalid" >&2
			;;
	esac
	$PRINT_ALL && echo KDUMP_PROTO=${KDUMP_PROTO}

	return 0
}

#################################################################################
function option() {
	TYPE="$1"
	NAME="$2"
	declare -n OPTION="$2"
	DEFAULT="$3"

	if [[ -z ${OPTION} ]]; then
		# set a default value
		OPTION="${DEFAULT}"
	else
		# option has a value; check if it's valid
		case ${TYPE} in
			string)
				# strings need no checks
				;;

			bool)
				# treat "yes" and "1" as "true" and "no" and "0" as "false"
				# anything else is an error
				case "${OPTION}" in
					yes | 1 | true)
						OPTION=true
						;;
					no | 0 |false)
						OPTION=false
						;;
					*)
						echo "config option ${NAME} has invalid value '$OPTION', should be 'true' or 'false; ignoring'" >&2
						OPTION=${DEFAULT}
						;;
				esac
				;;
			int)
				# check that the value is a number
				if ! [[ "${OPTION}" =~ ^[0-9]+$ ]]; then
						echo "config option ${NAME} has invalid value '$OPTION', must be a number; ignoring'" >&2
						OPTION=${DEFAULT}
				fi
				;;

			deprecated)
				echo "config option ${NAME} is deprecated, ignoring" >&2
				return
				;;
		esac
	fi

	if $PRINT_ALL; then
		# single-quote and escape existing single quotes
		echo "${NAME}='${OPTION//\'/\'\\\'\'}'"
		return
	fi
	[[ "${OPTION}" == "${DEFAULT}" ]] && return
	$PRINT_NONDEFAULT && echo "${NAME}=\'${OPTION//\'/\'\\\'\'}\'"

}

PRINT_NONDEFAULT=false
PRINT_ALL=false
[[ "$1" == "--print-nondefault" ]] && PRINT_NONDEFAULT=true
[[ "$1" == "--print" ]] && PRINT_ALL=true

kdump_read_config_main
