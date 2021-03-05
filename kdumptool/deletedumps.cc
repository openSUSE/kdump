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
#include <iostream>
#include <string>
#include <cerrno>
#include <memory>
#include <sstream>
#include <algorithm>
#include <ext/algorithm>

#include "subcommand.h"
#include "debug.h"
#include "savedump.h"
#include "util.h"
#include "fileutil.h"
#include "rootdirurl.h"
#include "transfer.h"
#include "configuration.h"
#include "dataprovider.h"
#include "progress.h"
#include "stringutil.h"
#include "vmcoreinfo.h"
#include "deletedumps.h"
#include "stringvector.h"

using std::string;
using std::cout;
using std::endl;
using std::stringstream;
using std::cerr;
using std::back_inserter;

//{{{ DeleteDumps --------------------------------------------------------------

// -----------------------------------------------------------------------------
DeleteDumps::DeleteDumps()
    : m_dryRun(false)
{
    Debug::debug()->trace("DeleteDumps::DeleteDumps()");

    m_options.push_back(new StringOption("root", 'R', &m_rootdir,
        "Use the specified root directory instead of /"));
    m_options.push_back(new FlagOption("dry-run", 'y', &m_dryRun,
        "Don't delete, just print out what to delete"));
}

// -----------------------------------------------------------------------------
const char *DeleteDumps::getName() const
{
    return "delete_dumps";
}

// -----------------------------------------------------------------------------
void DeleteDumps::execute()
{
    Debug::debug()->trace("DeleteDumps::execute()");
    Debug::debug()->dbg("Using root dir %s, dry run: %d",
        m_rootdir.c_str(), m_dryRun);

    Configuration *config = Configuration::config();

    int oldDumps = config->KDUMP_KEEP_OLD_DUMPS.value();
    Debug::debug()->dbg("keep %d old dumps", oldDumps);

    if (oldDumps == 0) {
        cerr << "Deletion of old dumps disabled." << endl;
        return;
    }

    std::istringstream iss(config->KDUMP_SAVEDIR.value());
    string elem;
    while (iss >> elem) {
        RootDirURL url(elem, m_rootdir);
        delete_one(url, oldDumps);
    }
}

// -----------------------------------------------------------------------------
void DeleteDumps::delete_one(const RootDirURL &url, int oldDumps)
{
    if (url.getProtocol() != URLParser::PROT_FILE) {
        cerr << "Deletion of old dump only on local disk." << endl;
        return;
    }

    FilePath dir = url.getRealPath();
    Debug::debug()->dbg("Using directory %s", dir.c_str());

    if (!dir.exists()) {
        cerr << "Nothing to delete in " + dir + "." << endl;
        return;
    }

    StringVector contents = dir.listDir(FilterKdumpDirs());
    int deleteItems;
    if (oldDumps == -1)
        deleteItems = contents.size();
    else if (oldDumps > int(contents.size())) {
        Debug::debug()->dbg("Nothing to delete.");
        return;
    } else
        deleteItems = contents.size() - oldDumps;

    Debug::debug()->dbg("Deleting the oldest %d entries.", deleteItems);

    StringVector toDelete;
    std::copy_n(contents.begin(), deleteItems, back_inserter(toDelete));

    for (StringVector::const_iterator it = toDelete.begin();
            it != toDelete.end(); ++it) {
        Debug::debug()->info("Deleting %s.", (*it).c_str());
        if (!m_dryRun) {
            FilePath fp = dir;
            fp.appendPath(*it);
            fp.rmdir(true);
        }
    }
}

//}}}

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:
