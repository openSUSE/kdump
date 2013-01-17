/**
 * This file is included from different places with different
 * definitions of DEFINE_OPT, which should be a macro that takes
 * four arguments:
 *
 *      @name    option name (C string constant)
 *      @type    data type (string, int, bool)
 *      @val     class member for the value
 *      @defval  default value (may be empty!)
 */

DEFINE_OPT("KDUMP_KERNELVER", string, m_kernelVersion, )
DEFINE_OPT("KDUMP_CPUS", int, m_CPUs, 1)
DEFINE_OPT("KDUMP_COMMANDLINE", string, m_commandLine, )
DEFINE_OPT("KDUMP_COMMANDLINE_APPEND", string, m_commandLineAppend, )
DEFINE_OPT("KEXEC_OPTIONS", string, m_kexecOptions, )
DEFINE_OPT("MAKEDUMPFILE_OPTIONS", string, m_makedumpfileOptions, )
DEFINE_OPT("KDUMP_IMMEDIATE_REBOOT", bool, m_immediateReboot, true)
DEFINE_OPT("KDUMP_TRANSFER", string, m_customTransfer, )
DEFINE_OPT("KDUMP_SAVEDIR", string, m_savedir, "/var/log/dump")
DEFINE_OPT("KDUMP_KEEP_OLD_DUMPS", int, m_keepOldDumps, 0)
DEFINE_OPT("KDUMP_FREE_DISK_SIZE", int, m_freeDiskSize, 64)
DEFINE_OPT("KDUMP_VERBOSE", int, m_verbose, 0)
DEFINE_OPT("KDUMP_DUMPLEVEL", int, m_dumpLevel, 0)
DEFINE_OPT("KDUMP_DUMPFORMAT", string, m_dumpFormat, "compressed")
DEFINE_OPT("KDUMP_CONTINUE_ON_ERROR", bool, m_continueOnError, true)
DEFINE_OPT("KDUMP_REQUIRED_PROGRAMS", string, m_requiredPrograms, )
DEFINE_OPT("KDUMP_PRESCRIPT", string, m_prescript, )
DEFINE_OPT("KDUMP_POSTSCRIPT", string, m_postscript, )
DEFINE_OPT("KDUMP_COPY_KERNEL", bool, m_copyKernel, )
DEFINE_OPT("KDUMPTOOL_FLAGS", string, m_kdumptoolFlags, )
DEFINE_OPT("KDUMP_NETCONFIG", string, m_netConfig, "auto")
DEFINE_OPT("KDUMP_SMTP_SERVER", string, m_smtpServer, )
DEFINE_OPT("KDUMP_SMTP_USER", string, m_smtpUser, )
DEFINE_OPT("KDUMP_SMTP_PASSWORD", string, m_smtpPassword, )
DEFINE_OPT("KDUMP_NOTIFICATION_TO", string, m_notificationTo, )
DEFINE_OPT("KDUMP_NOTIFICATION_CC", string, m_notificationCc, )
DEFINE_OPT("KDUMP_HOST_KEY", string, m_hostKey, )
