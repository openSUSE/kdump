/**
 * This file is included from different places with different
 * definitions of DEFINE_OPT, which should be a macro that takes
 * four arguments:
 *
 *      @name    option name (C identifier)
 *      @type    data type (String, Int, Bool)
 *      @defval  default value
 */

DEFINE_OPT(KDUMP_KERNELVER, String, "")
DEFINE_OPT(KDUMP_CPUS, Int, 1)
DEFINE_OPT(KDUMP_COMMANDLINE, String, "")
DEFINE_OPT(KDUMP_COMMANDLINE_APPEND, String, "")
DEFINE_OPT(KEXEC_OPTIONS, String, "")
DEFINE_OPT(MAKEDUMPFILE_OPTIONS, String, "")
DEFINE_OPT(KDUMP_IMMEDIATE_REBOOT, Bool, true)
DEFINE_OPT(KDUMP_TRANSFER, String, "")
DEFINE_OPT(KDUMP_SAVEDIR, String, "/var/log/dump")
DEFINE_OPT(KDUMP_KEEP_OLD_DUMPS, Int, 0)
DEFINE_OPT(KDUMP_FREE_DISK_SIZE, Int, 64)
DEFINE_OPT(KDUMP_VERBOSE, Int, 0)
DEFINE_OPT(KDUMP_DUMPLEVEL, Int, 0)
DEFINE_OPT(KDUMP_DUMPFORMAT, String, "compressed")
DEFINE_OPT(KDUMP_CONTINUE_ON_ERROR, Bool, true)
DEFINE_OPT(KDUMP_REQUIRED_PROGRAMS, String, "")
DEFINE_OPT(KDUMP_PRESCRIPT, String, "")
DEFINE_OPT(KDUMP_POSTSCRIPT, String, "")
DEFINE_OPT(KDUMP_COPY_KERNEL, Bool, "")
DEFINE_OPT(KDUMPTOOL_FLAGS, String, "")
DEFINE_OPT(KDUMP_NETCONFIG, String, "auto")
DEFINE_OPT(KDUMP_SMTP_SERVER, String, "")
DEFINE_OPT(KDUMP_SMTP_USER, String, "")
DEFINE_OPT(KDUMP_SMTP_PASSWORD, String, "")
DEFINE_OPT(KDUMP_NOTIFICATION_TO, String, "")
DEFINE_OPT(KDUMP_NOTIFICATION_CC, String, "")
DEFINE_OPT(KDUMP_HOST_KEY, String, "")
