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

/**
 * Subcommand to save the dump.
 */
class SaveDump : public Subcommand {

    public:
        /**
         * Creates a new SaveDump object.
         */
        SaveDump()
        throw ();

        /**
         * Deletes a SaveDump object.
         */
        ~SaveDump()
        throw ();

    public:
        /**
         * Returns the name of the subcommand (save_dump).
         */
        const char *getName() const
        throw ();

        /**
         * Executes the function.
         *
         * @throw KError on any error. No exception indicates success.
         */
        void execute()
        throw (KError);

    protected:
        void saveDump(const RootDirURLVector &urlv)
        throw (KError);

        void copyMakedumpfile()
        throw (KError);

        void generateInfo()
        throw (KError);

        void generateRearrange()
        throw (KError);

        void fillVmcoreinfo()
        throw (KError);

        void copyKernel()
        throw (KError);

        std::string findKernel()
        throw (KError);

        std::string findMapfile()
        throw (KError);

        void checkAndDelete(const RootDirURLVector &urlv)
        throw (KError);

        void sendNotification(bool failure, const RootDirURLVector &urlv)
        throw ();

        std::string getKernelReleaseCommandline()
        throw (KError);

        /**
         * Returns a Transfer object suitable for the provided URL.
         *
         * @param[in] url the URL
         * @return the Transfer object
         *
         * @exception KError if parsing the URL failed or there's no
         *            implementation for that class.
         */
	Transfer *getTransfer(const RootDirURLVector &urlv)
	throw (KError);

    private:
        FilePath m_dump;
        Transfer *m_transfer;
        bool m_usedDirectSave;
        bool m_useMakedumpfile;
	unsigned long m_split;
	unsigned long m_threads;
        std::string m_crashtime;
        std::string m_crashrelease;
        std::string m_rootdir;
        std::string m_hostname;
        bool m_nomail;

        void check_one(const RootDirURL &parser)
        throw (KError);
};

//}}}

#endif /* SAVE_DUMP_H */

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:
