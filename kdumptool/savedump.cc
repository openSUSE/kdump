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
#include "vmcoreinfo.h"

using std::string;
using std::cout;
using std::endl;
using std::auto_ptr;
using std::stringstream;

//{{{ SaveDump -----------------------------------------------------------------

// -----------------------------------------------------------------------------
SaveDump::SaveDump()
    throw ()
    : m_dump(DEFAULT_DUMP), m_transfer(NULL), m_usedDirectSave(false),
      m_useMakedumpfile(false)
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
    m_transfer = URLTransfer::getTransfer(savedir.c_str());

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
        if (!m_usedDirectSave && m_useMakedumpfile)
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

    if (useElf && dumplevel == 0) {
        // use file source?
        provider = new FileDataProvider(m_dump.c_str());
        m_useMakedumpfile = false;
    } else {
        // use makedumpfile
        stringstream cmdline;
        cmdline << "makedumpfile";
        cmdline << config->getMakedumpfileOptions() << " ";
        cmdline << "-d " << config->getDumpLevel() << " ";
        if (useElf)
            cmdline << "-E ";
        if (useCompressed)
            cmdline << "-c ";
        cmdline << m_dump;

        string directCmdline = cmdline.str();
        string pipeCmdline = cmdline.str() + " -F"; // flattened format

        provider = new ProcessDataProvider(pipeCmdline.c_str(),
            directCmdline.c_str());
        m_useMakedumpfile = true;
    }

    try {
        Terminal terminal;
        if (m_useMakedumpfile) {
            cout << "Saving dump using makedumpfile" << endl;
            terminal.printLine();
        }
        TerminalProgress progress("Saving dump");
        provider->setProgress(&progress);
        m_transfer->perform(provider, "vmcore", &m_usedDirectSave);
        if (m_useMakedumpfile)
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
    m_transfer->perform(provider.get(), "makedumpfile-R.pl", NULL);
}

// -----------------------------------------------------------------------------
void SaveDump::generateInfo()
    throw (KError)
{
    Debug::debug()->trace("SaveDump::generateInfo");
    Configuration *config = Configuration::config();

    // get crashing time and also get the crashing kernel release
    string crashtime, crashrelease;

    Vmcoreinfo vm;
    try {
        vm.readFromELF(m_dump.c_str());
        unsigned long long time = vm.getLLongValue("CRASHTIME");

        crashtime = Stringutil::formatUnixTime("%Y-%m-%d %H:%M (%z)", time);
        crashrelease = vm.getStringValue("OSRELEASE");

    } catch (const KError &err) {
        // no fatal error
        Debug::debug()->info("Reading VMCOREINFO failed: %s", err.what());
    }

    Debug::debug()->dbg("Using crashtime: %s, crashrelease: %s",
        crashtime.c_str(), crashrelease.c_str());

    stringstream ss;

    ss << "Kernel crashdump" << endl;
    ss << "----------------" << endl;
    ss << endl;

    if (crashtime.size() > 0)
        ss << "Crash time     : " << crashtime << endl;
    if (crashrelease.size() > 0)
        ss << "Kernel version : " << crashrelease << endl;
    ss << "Dump level     : "
       << Stringutil::number2string(config->getDumpLevel()) << endl;
    ss << "Dump format    : " << config->getDumpFormat() << endl;
    ss << endl;

    if (m_useMakedumpfile && !m_usedDirectSave) {
        ss << "NOTE:" << endl;
        ss << "This dump was saved in makedumpfile flattened format." << endl;
        ss << "To read the dump with crash, run" << endl;
        ss << "    mv vmcore vmcore.flattened" << endl;
        ss << "    perl makedumpfile-R.pl vmcore < vmcore.flattened" << endl;
        ss << "    rm vmcore.flattend" << endl;
    }


    TerminalProgress progress("Generating README");
    ByteVector bv = Stringutil::str2bytes(ss.str());
    BufferDataProvider provider(bv);
    provider.setProgress(&progress);
    m_transfer->perform(&provider, "README.txt", NULL);
}


//}}}

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:
