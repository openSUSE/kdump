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
#ifndef SAVE_DUMP_H
#define SAVE_DUMP_H

#include "fileutil.h"
#include "subcommand.h"
#include "urlparser.h"
#include "rootdirurl.h"

class Transfer;

//{{{ SaveDump -----------------------------------------------------------------

class SaveDump {
    protected:
        FilePath m_dump;
        std::string m_crashrelease;
        std::string m_rootdir;
        std::string m_hostname;
        bool m_nomail;

    public:
        SaveDump();
        ~SaveDump();

        int create();

        const FilePath& dumpPath() const
        { return m_dump; }
        void dumpPath(const FilePath& dumpPath)
        { m_dump = dumpPath; }

        const std::string& kernelVersion() const
        { return m_crashrelease; }
        void kernelVersion(const std::string& kernelversion)
        { m_crashrelease = kernelversion; }

        const std::string& rootDir() const
        { return m_rootdir; }
        void rootDir(const std::string& rootdir)
        { m_rootdir = rootdir; }

        const std::string& hostName() const
        { return m_hostname; }
        void hostName(const std::string& hostname)
        { m_hostname = hostname; }

        bool noMail() const
        { return m_nomail; }
        void noMail(bool nomail)
        { m_nomail = nomail; }

    protected:
        void saveDump(const RootDirURLVector &urlv);

        void copyMakedumpfile();

        void generateInfo();

        void generateRearrange();

        void fillVmcoreinfo();

        void copyKernel();

        std::string findKernel();

        std::string findMapfile();

        void checkAndDelete(const RootDirURLVector &urlv);

        void sendNotification(bool failure, const RootDirURLVector &urlv);

        std::string getKernelReleaseCommandline();

        /**
         * Returns a Transfer object suitable for the provided URL.
         *
         * @param[in] url the URL
         * @return the Transfer object
         *
         * @exception KError if parsing the URL failed or there's no
         *            implementation for that class.
         */
	Transfer *getTransfer(const RootDirURLVector &urlv);

    private:
        unsigned long m_split;
        Transfer *m_transfer;
        bool m_usedDirectSave;
        bool m_useMakedumpfile;
        unsigned long m_threads;
        unsigned long long m_crashtime;

        void checkOne(const RootDirURL &parser);
};

//}}}
//{{{ SaveDumpCommand ----------------------------------------------------------

/**
 * Subcommand to save the dump.
 */
class SaveDumpCommand : protected SaveDump, public Subcommand {

    public:
        /**
         * Creates a new SaveDumpCommand object.
         */
        SaveDumpCommand();

    public:
        /**
         * Returns the name of the subcommand (save_dump).
         */
        const char *getName() const;

        /**
         * Executes the function.
         *
         * @throw KError on any error. No exception indicates success.
         */
        void execute();
};

//}}}

#endif /* SAVE_DUMP_H */

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:
