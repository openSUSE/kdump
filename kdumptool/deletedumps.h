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
#ifndef DELETE_DUMP_H
#define DELETE_DUMP_H

#include "subcommand.h"
#include "rootdirurl.h"

class Transfer;

//{{{ DeleteDumps --------------------------------------------------------------

/**
 * Delete old dumps from disk.
 */
class DeleteDumps {
    protected:
        std::string m_rootdir;
        bool m_dryRun;

    public:
        DeleteDumps();

        void deleteAll();

        const std::string& rootDir() const
        { return m_rootdir; }
        void rootDir(const std::string& rootdir)
        { m_rootdir = rootdir; }

        bool dryRun() const
        { return m_dryRun; }
        void dryRun(bool dryRun)
        { m_dryRun = dryRun; }

    private:
	/**
	 * Helper function to delete one dump target directory.
	 *
	 * @throw KError on any error.
	 */
	void deleteOne(const RootDirURL &url, int oldDumps);
};

//}}}
//{{{ DeleteDumpsCommand -------------------------------------------------------

/**
 * Subcommand to delete old dumps.
 */
class DeleteDumpsCommand : protected DeleteDumps, public Subcommand {

    public:
        /**
         * Creates a new DeleteDumpsCommand object.
         */
        DeleteDumpsCommand();

    public:
        /**
         * Returns the name of the subcommand (delete_dumps).
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

#endif /* DELETE_DUMP_H */

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:
