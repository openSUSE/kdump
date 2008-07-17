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
#include <errno.h>
#include <memory>
#include <sstream>

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

using std::string;
using std::cout;
using std::endl;
using std::auto_ptr;
using std::stringstream;

//{{{ SaveDump -----------------------------------------------------------------

// -----------------------------------------------------------------------------
SaveDump::SaveDump()
    throw ()
    : m_dump(DEFAULT_DUMP), m_transfer(NULL), m_usedMakedumpfile(false)
{
    Debug::debug()->trace("SaveDump::SaveDump()");
}

// -----------------------------------------------------------------------------
SaveDump::~SaveDump()
    throw ()
{
    Debug::debug()->trace("SaveDump::~SaveDump()");

    delete m_transfer;
}

// -----------------------------------------------------------------------------
const char *SaveDump::getName() const
    throw ()
{
    return "save_dump";
}

// -----------------------------------------------------------------------------
OptionList SaveDump::getOptions() const
    throw ()
{
    OptionList list;

    Debug::debug()->trace("SaveDump::getOptions()");

    list.push_back(Option("dump", 'u', OT_STRING,
        "Use the specified dump instead of " DEFAULT_DUMP "."));

    return list;

}

// -----------------------------------------------------------------------------
void SaveDump::parseCommandline(OptionParser *optionparser)
    throw (KError)
{
    Debug::debug()->trace("SaveDump::parseCommandline(%p)", optionparser);

    if (optionparser->getValue("dump").getType() != OT_INVALID)
        m_dump = optionparser->getValue("dump").getString();

    Debug::debug()->dbg("dump = " + m_dump);
}

// -----------------------------------------------------------------------------
void SaveDump::execute()
    throw (KError)
{
    Debug::debug()->trace(__FUNCTION__);
    Configuration *config = Configuration::config();

    // check if the dump file actually exists
    if (!FileUtil::exists(m_dump))
        throw KError("The dump file " + m_dump + " does not exist.");

    // build the transfer object
    // prepend a time stamp to the save dir
    string savedir = config->getSavedir();
    savedir = FileUtil::pathconcat(savedir,
        Stringutil::formatCurrentTime(ISO_DATETIME));
    m_transfer = URLTransfer::getTransfer(config->getSavedir().c_str());

    cout << "Saving dump to " << savedir << "." << endl;

    try {
        saveDump();
    } catch (const KError &error) {
        setErrorCode(1);
        if (config->getContinueOnError())
            cout << error.what() << endl;
        else
            throw;
    }

    try {
        if (m_usedMakedumpfile)
            copyMakedumpfile();
    } catch (const KError &error) {
        setErrorCode(1);
        if (config->getContinueOnError())
            cout << error.what() << endl;
        else
            throw;
    }

    try {
        generateInfo();
    } catch (const KError &error) {
        setErrorCode(1);
        if (config->getContinueOnError())
            cout << error.what() << endl;
        else
            throw;
    }
}

// -----------------------------------------------------------------------------
void SaveDump::saveDump()
    throw (KError)
{
    Configuration *config = Configuration::config();

    // build the data provider object
    int dumplevel = config->getDumpLevel();
    if (dumplevel < 0 || dumplevel > 31) {
        Debug::debug()->info("Dumplevel %d is invalid. Using 0.", dumplevel);
        dumplevel = 0;
    }

    // dump format
    string dumpformat = config->getDumpFormat();
    DataProvider *provider;

    bool useElf = strcasecmp(dumpformat.c_str(), "elf") == 0;
    bool useCompressed = strcasecmp(dumpformat.c_str(), "compressed") == 0;
    string name;

    if (useElf && dumplevel == 0) {
        // use file source?
        provider = new FileDataProvider(m_dump.c_str());
        name = "vmcore";
    } else {
        // use makedumpfile
        stringstream cmdline;
        cmdline << "makedumpfile";
        cmdline << config->getMakedumpfileOptions() << " ";
        cmdline << "-d " << config->getDumpLevel() << " ";
        // flattened
        cmdline << "-F ";
        if (useElf)
            cmdline << "-E ";
        if (useCompressed)
            cmdline << "-c ";
        cmdline << m_dump;

        Debug::debug()->dbg("Command line: %s", cmdline.str().c_str());
        name = "vmcore.flattened";
        provider = new ProcessDataProvider(cmdline.str().c_str());
        m_usedMakedumpfile = true;
    }

    try {
        Terminal terminal;
        if (m_usedMakedumpfile) {
            cout << "Saving dump using makedumpfile" << endl;
            terminal.printLine();
        }
        TerminalProgress progress("Saving dump");
        provider->setProgress(&progress);
        m_transfer->perform(provider, name.c_str());
        if (m_usedMakedumpfile)
            terminal.printLine();
    } catch (...) {
        delete provider;
        throw;
    }
}

// -----------------------------------------------------------------------------
void SaveDump::copyMakedumpfile()
    throw (KError)
{
    StringList paths;

    paths.push_back("/bin/makedumpfile-R.pl");
    paths.push_back("/sbin/makedumpfile-R.pl");
    paths.push_back("/usr/bin/makedumpfile-R.pl");
    paths.push_back("/usr/sbin/makedumpfile-R.pl");
    paths.push_back("/usr/local/bin/makedumpfile-R.pl");
    paths.push_back("/usr/local/sbin/makedumpfile-R.pl");
    paths.push_back("/root/bin/makedumpfile-R.pl");

    string makedumpfile_binary;

    for (StringList::const_iterator it = paths.begin();
            it != paths.end(); ++it) {
        if (FileUtil::exists(*it)) {
            makedumpfile_binary = *it;
            break;
        }
    }

    if (makedumpfile_binary.size() == 0)
        throw KError("makedumpfile-R.pl not found.");

    auto_ptr<DataProvider> provider(
        new FileDataProvider(makedumpfile_binary.c_str())
    );;

    TerminalProgress progress("Saving makedumpfile-R.pl");
    provider->setProgress(&progress);
    m_transfer->perform(provider.get(), "makedumpfile-R.pl");
}

// -----------------------------------------------------------------------------
void SaveDump::generateInfo()
    throw (KError)
{

}


//}}}

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:
