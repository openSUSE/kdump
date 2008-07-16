/*
 * (c) 2008, Bernhard Walle <bwalle@suse.de>, SUSE LINUX Products GmbH
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */
#ifndef CONFIGURATION_H
#define CONFIGURATION_H

#include "global.h"
#include "optionparser.h"
#include "subcommand.h"

//{{{ Configuration ------------------------------------------------------------

/**
 * Configuration. This is a singleton object. To use it, call
 *
 *   Configuration::config()->readConfig(filename)
 *
 * once. If you didn't call this, you'll get a KError on every attempt to
 * read a value.
 *
 * The actual parsing is done in the ConfigParser class.
 */
class Configuration {

    public:
        /**
         * Returns the only configuration object.
         *
         * @return the configuration object
         */
        static Configuration *config()
        throw ();

        /**
         * Reads a configuration file.
         *
         * @param filename the file name to read
         * @param variables a list of variables where to read
         * @exception if the @c filename was not found or the shell that
         *            is necessary to parse the configuration file could not
         *            be spawned
         */
        void readFile(const std::string &filename)
        throw (KError);

        /**
         * Returns the value of KDUMP_KERNELVER.
         *
         * @return the kernel version
         * @exception KError if Configuration::readFile() was not called
         */
        std::string getKernelVersion() const
        throw (KError);

        /**
         * Returns the value of KDUMP_COMMANDLINE.
         *
         * @return the kernel command line for the kdump kernel
         * @exception KError if Configuration::readFile() was not called
         */
        std::string getCommandLine() const
        throw (KError);

        /**
         * Returns the value of KDUMP_COMMANDLINE_APPEND.
         *
         * @return the append kernel command line
         * @exception KError if Configuration::readFile() was not called
         */
         std::string getCommandLineAppend() const
         throw (KError);

         /**
          * Returns the value of KEXEC_OPTIONS.
          *
          * @return the the options for kexec
          * @exception KError if Configuration::readFile() was not called
          */
         std::string getKexecOptions() const
         throw (KError);

         /**
          * Returns the value of MAKEDUMPFILE_OPTIONS.
          *
          * @return the options for makedumpfile
          * @exception KError if Configuration::readFile() was not called
          */
         std::string getMakedumpfileOptions() const
         throw (KError);

         /**
          * Returns the value of KDUMP_IMMEDIATE_REBOOT.
          *
          * @return the @c true if the system should be rebooted after
          *         reboot immediately, @c false otherwise
          * @exception KError if Configuration::readFile() was not called
          */
         bool getImmediateReboot() const
         throw (KError);

         /**
          * Returns the value of KDUMP_TRANSFER.
          *
          * @return the custom transfer script
          * @exception KError if Configuration::readFile() was not called
          */
         std::string getCustomTransfer() const
         throw (KError);

         /**
          * Returns the value of KDUMP_SAVEDIR.
          *
          * @return the URL where the dump should be saved to
          * @exception KError if Configuration::readFile() was not called
          */
         std::string getSavedir() const
         throw (KError);

         /**
          * Returns the value of KDUMP_KEEP_OLD_DUMPS.
          *
          * @return the number of old dumps that should be kept
          * @exception KError if Configuration::readFile() was not called
          */
         int getKeepOldDumps() const
         throw (KError);

         /**
          * Returns the value of KDUMP_FREE_DISK_SIZE.
          *
          * @return the disk size in megabytes that should stay free
          * @exception KError if Configuration::readFile() was not called
          */
         int getFreeDiskSize() const
         throw (KError);

         /**
          * Returns the value of KDUMP_VERBOSE.
          *
          * @return a bit mask that represents the verbosity
          * @exception KError if Configuration::readFile() was not called
          */
         int getVerbose() const
         throw (KError);

         /**
          * Returns the value of KDUMP_DUMPLEVEL.
          *
          * @return the dump level (for makedumpfile)
          * @exception KError if Configuration::readFile() was not called
          */
         int getDumpLevel() const
         throw (KError);

         /**
          * Returns the value of KDUMP_DUMPFORMAT.
          *
          * @return the dump format (@c ELF, @c compressed, @c "")
          * @exception KError if Configuration::readFile() was not called
          */
         std::string getDumpFormat() const
         throw (KError);

         /**
          * Returns the value of KDUMP_CONTINUE_ON_ERROR.
          *
          * @return @c true if kdump should continue on error, @c false
          *         otherwise
          * @exception KError if Configuration::readFile() was not called
          */
         bool getContinueOnError() const
         throw (KError);

         /**
          * Returns the value of KDUMP_REQUIRED_PROGRAMS.
          *
          * @return a space-separated list of required programs
          * @exception KError if Configuration::readFile() was not called
          */
         std::string getRequiredPrograms() const
         throw (KError);

         /**
          * Returns the value of KDUMP_PRESCRIPT.
          *
          * @return script that should be executed before the dump is saved
          * @exception KError if Configuration::readFile() was not called
          */
         std::string getPrescript() const
         throw (KError);

         /**
          * Returns the value of KDUMP_POSTSCRIPT.
          *
          * @return the kernel version
          * @exception KError if Configuration::readFile() was not called
          */
         std::string getPostscript() const
         throw (KError);

         /**
          * Returns the value of KDUMP_COPY_KERNEL.
          *
          * @return @c true if the full kernel should be copied, @c false
          *         otherwise
          * @exception KError if Configuration::readFile() was not called
          */
         bool getCopyKernel() const
         throw (KError);

    protected:
        Configuration()
        throw ();

        virtual ~Configuration()
        throw () {}

    private:
        static Configuration *m_instance;
        bool m_readConfig;
        std::string m_kernelVersion;
        std::string m_commandLine;
        std::string m_commandLineAppend;
        std::string m_kexecOptions;
        std::string m_makedumpfileOptions;
        bool m_immediateReboot;
        std::string m_customTransfer;
        std::string m_savedir;
        int m_keepOldDumps;
        int m_freeDiskSize;
        int m_verbose;
        int m_dumpLevel;
        std::string m_dumpFormat;
        bool m_continueOnError;
        std::string m_requiredPrograms;
        std::string m_prescript;
        std::string m_postscript;
        bool m_copyKernel;
};

//}}}

#endif /* CONFIGURATION_H */

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:
