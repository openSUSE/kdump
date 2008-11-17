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
#include "transfer.h"
#include "configuration.h"
#include "dataprovider.h"
#include "progress.h"
#include "stringutil.h"
#include "vmcoreinfo.h"
#include "deletedumps.h"

using std::string;
using std::cout;
using std::endl;
using std::auto_ptr;
using std::stringstream;
using std::cerr;
using __gnu_cxx::copy_n;
using std::back_inserter;

//{{{ DeleteDumps --------------------------------------------------------------

// -----------------------------------------------------------------------------
DeleteDumps::DeleteDumps()
    throw ()
    : m_dryRun(false)
{
    Debug::debug()->trace("DeleteDumps::DeleteDumps()");
}

// -----------------------------------------------------------------------------
const char *DeleteDumps::getName() const
    throw ()
{
    return "delete_dumps";
}

// -----------------------------------------------------------------------------
OptionList DeleteDumps::getOptions() const
    throw ()
{
    OptionList list;

    list.push_back(Option("root", 'R', OT_STRING,
        "Use the specified root directory instead of /."));
    list.push_back(Option("dry-run", 'y', OT_STRING,
        "Don't delete, just print out what to delete."));

    return list;
}

// -----------------------------------------------------------------------------
void DeleteDumps::parseCommandline(OptionParser *optionparser)
    throw (KError)
{
    Debug::debug()->trace("DeleteDumps::parseCommandline(%p)", optionparser);

    if (optionparser->getValue("root").getType() != OT_INVALID)
        m_rootdir = optionparser->getValue("root").getString();
    if (optionparser->getValue("dry-run").getType() != OT_INVALID)
        m_dryRun = optionparser->getValue("dry-run").getFlag();

    Debug::debug()->dbg("Using root dir %s, dry run: %d",
        m_rootdir.c_str(), m_dryRun);
}

// -----------------------------------------------------------------------------
void DeleteDumps::execute()
    throw (KError)
{
    Debug::debug()->trace("DeleteDumps::execute()");
    Configuration *config = Configuration::config();

    int oldDumps = config->getKeepOldDumps();
    Debug::debug()->dbg("keep %d old dumps", oldDumps);

    if (oldDumps == 0) {
        cerr << "Deletion of old dumps disabled." << endl;
        return;
    }

    URLParser parser;
    parser.parseURL(config->getSavedir());
    if (parser.getProtocol() != URLParser::PROT_FILE) {
        cerr << "Deletion of old dump only on local disk." << endl;
        return;
    }

    string dir = parser.getPath();
    if (m_rootdir.size() != 0) {
        dir = FileUtil::pathconcat(m_rootdir,
                FileUtil::getCanonicalPathRoot(dir, m_rootdir));
    }
    Debug::debug()->dbg("Using directory %s", dir.c_str());

    if (!FileUtil::exists(dir)) {
        cerr << "Nothing to delete in " + dir + "." << endl;
        return;
    }

    StringVector contents = FileUtil::listdir(dir, true);
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
    copy_n(contents.begin(), deleteItems, back_inserter(toDelete));

    for (StringVector::const_iterator it = toDelete.begin();
            it != toDelete.end(); ++it) {
        Debug::debug()->info("Deleting %s.", (*it).c_str());
        if (!m_dryRun)
            FileUtil::rmdir(FileUtil::pathconcat(dir, (*it)), true);
    }
}

//}}}

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:
