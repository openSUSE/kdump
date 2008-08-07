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
#include <strings.h>
#include <cerrno>
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
#include "identifykernel.h"
#include "debuglink.h"
#include "email.h"

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
      m_useMakedumpfile(false), m_nomail(false)
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
    list.push_back(Option("root", 'R', OT_STRING,
        "Use the specified root directory instead of /."));
    list.push_back(Option("kernelversion", 'k', OT_STRING,
        "Use the specified kernel version instead of auto-detection via "
        "VMCOREINFO."));
    list.push_back(Option("fqdn", 'q', OT_STRING,
        "Use the specified hostname/domainname instead of uname()."));
    list.push_back(Option("nomail", 'M', OT_FLAG,
        "Don't send notification email even if email has been configured."));

    return list;

}

// -----------------------------------------------------------------------------
void SaveDump::parseCommandline(OptionParser *optionparser)
    throw (KError)
{
    Debug::debug()->trace("SaveDump::parseCommandline(%p)", optionparser);

    if (optionparser->getValue("dump").getType() != OT_INVALID)
        m_dump = optionparser->getValue("dump").getString();
    if (optionparser->getValue("root").getType() != OT_INVALID)
        m_rootdir = optionparser->getValue("root").getString();
    if (optionparser->getValue("kernelversion").getType() != OT_INVALID)
        m_crashrelease = optionparser->getValue("kernelversion").getString();
    if (optionparser->getValue("fqdn").getType() != OT_INVALID)
        m_hostname = optionparser->getValue("fqdn").getString();
    if (optionparser->getValue("fqdn").getType() != OT_INVALID)
        m_hostname = optionparser->getValue("fqdn").getString();
    if (optionparser->getValue("nomail").getFlag())
        m_nomail = true;

    Debug::debug()->dbg("dump: %s, root: %s, crashrelease: %s, "
        "hostname: %s, nomail: %d",
        m_dump.c_str(), m_rootdir.c_str(), m_crashrelease.c_str(),
        m_hostname.c_str(), int(m_nomail));
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

    // root dir support
    m_urlParser.parseURL(savedir.c_str());

    if (m_rootdir.size() != 0 &&
                m_urlParser.getProtocol() == URLParser::PROT_FILE) {
        Debug::debug()->dbg("Using root dir support for Transfer (%s)",
            m_rootdir.c_str());

        string newUrl = m_urlParser.getProtocolAsString() + "://" +
            FileUtil::pathconcat(m_rootdir, m_urlParser.getPath());;
        m_transfer = URLTransfer::getTransfer(newUrl.c_str());
    } else
        m_transfer = URLTransfer::getTransfer(savedir.c_str());

    // save the dump
    try {
        saveDump();
    } catch (const KError &error) {
        setErrorCode(1);

        sendNotification(true, savedir);

        // run checkAndDelete() in any case
        try {
            checkAndDelete();
        } catch (const KError &error) {
            cout << error.what() << endl;
        }

        if (config->getContinueOnError())
            cout << error.what() << endl;
        else
            throw error;
    }

    // send the email afterwards
    sendNotification(true, savedir);

    // because we don't know the file size in advance, check
    // afterwards if the disk space is not sufficient and delete
    // the dump again
    try {
        checkAndDelete();
    } catch (const KError &error) {
        setErrorCode(1);
        if (config->getContinueOnError())
            cout << error.what() << endl;
        else
            throw;
    }

    // copy the makedumpfile-R.pl
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
        fillVmcoreinfo();
    } catch (const KError &error) {
        Debug::debug()->info("Error when reading VMCOREINFO: %s",
            error.what());
    }

    // generate the README file
    try {
        generateInfo();
    } catch (const KError &error) {
        setErrorCode(1);
        if (config->getContinueOnError())
            cout << error.what() << endl;
        else
            throw;
    }

    // copy kernel
    try {
        if (config->getCopyKernel())
            copyKernel();
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
        if (config->getVerbose() & Configuration::VERB_PROGRESS)
            provider->setProgress(&progress);
        else
            cout << "Saving dump ..." << endl;
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
    Configuration *config = Configuration::config();

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

    FileDataProvider provider(makedumpfile_binary.c_str());
    TerminalProgress progress("Saving makedumpfile-R.pl");
    if (config->getVerbose() & Configuration::VERB_PROGRESS)
        provider.setProgress(&progress);
    else
        cout << "Saving makedumpfile-R.pl ..." << endl;
    m_transfer->perform(&provider, "makedumpfile-R.pl", NULL);

    generateRearrange();
}

// -----------------------------------------------------------------------------
void SaveDump::generateRearrange()
    throw (KError)
{
    Configuration *config = Configuration::config();

    // and also generate a "rearrange" script
    stringstream ss;

    ss << "#!/bin/sh" << endl;
    ss << endl;
    ss << "# rename the flattened vmcore" << endl;
    ss << "mv vmcore vmcore.flattened || exit 1" << endl;
    ss << endl;
    ss << "# unflatten" << endl;
    ss << "perl makedumpfile-R.pl vmcore < vmcore.flattened || exit 1 " << endl;
    ss << endl;
    ss << "# delete the original dump" << endl;
    ss << "rm vmcore.flattened || exit 1 " << endl;
    ss << endl;
    ss << "# delete the perl script" << endl;
    ss << "rm makedumpfile-R.pl || exit 1 " << endl;
    ss << endl;
    ss << "# delete myself" << endl;
    ss << "rm \"$0\" || exit 1 " << endl;
    ss << endl;
    ss << "exit 0" << endl;
    ss << "# EOF" << endl;

    TerminalProgress progress2("Generating rearrange script");
    ByteVector bv = Stringutil::str2bytes(ss.str());
    BufferDataProvider provider2(bv);
    if (config->getVerbose() & Configuration::VERB_PROGRESS)
        provider2.setProgress(&progress2);
    else
        cout << "Generating rearrange script" << endl;
    m_transfer->perform(&provider2, "rearrange.sh", NULL);
}

// -----------------------------------------------------------------------------
void SaveDump::fillVmcoreinfo()
    throw (KError)
{
    Vmcoreinfo vm;
    vm.readFromELF(m_dump.c_str());
    unsigned long long time = vm.getLLongValue("CRASHTIME");

    m_crashtime = Stringutil::formatUnixTime("%Y-%m-%d %H:%M (%z)", time);

    // don't overwrite m_crashrelease from command line
    if (m_crashrelease.size() == 0)
        m_crashrelease = vm.getStringValue("OSRELEASE");

    Debug::debug()->dbg("Using crashtime: %s, crashrelease: %s",
        m_crashtime.c_str(), m_crashrelease.c_str());
}

// -----------------------------------------------------------------------------
void SaveDump::generateInfo()
    throw (KError)
{
    Debug::debug()->trace("SaveDump::generateInfo");
    Configuration *config = Configuration::config();

    stringstream ss;

    ss << "Kernel crashdump" << endl;
    ss << "----------------" << endl;
    ss << endl;

    if (m_hostname.size() == 0)
        m_hostname = Util::getHostDomain();

    if (m_crashtime.size() > 0)
        ss << "Crash time     : " << m_crashtime << endl;
    if (m_crashrelease.size() > 0)
        ss << "Kernel version : " << m_crashrelease << endl;
    ss << "Host           : " << m_hostname << endl;
    ss << "Dump level     : "
       << Stringutil::number2string(config->getDumpLevel()) << endl;
    ss << "Dump format    : " << config->getDumpFormat() << endl;
    ss << endl;

    if (m_useMakedumpfile && !m_usedDirectSave) {
        ss << "NOTE:" << endl;
        ss << "This dump was saved in makedumpfile flattened format." << endl;
        ss << "To read the dump with crash, run \"sh rearrange.sh\" before."
           << endl;
    }


    TerminalProgress progress("Generating README");
    ByteVector bv = Stringutil::str2bytes(ss.str());
    BufferDataProvider provider(bv);
    if (config->getVerbose() & Configuration::VERB_PROGRESS)
        provider.setProgress(&progress);
    else
        cout << "Generating README" << endl;
    m_transfer->perform(&provider, "README.txt", NULL);
}

// -----------------------------------------------------------------------------
void SaveDump::copyKernel()
    throw (KError)
{
    Debug::debug()->trace("SaveDump::copyKernel()");

    Configuration *config = Configuration::config();

    string mapfile = findMapfile();
    string kernel = findKernel();

    // mapfile
    TerminalProgress mapProgress("Copying System.map");
    FileDataProvider mapProvider(mapfile.c_str());
    if (config->getVerbose() & Configuration::VERB_PROGRESS)
        mapProvider.setProgress(&mapProgress);
    else
        cout << "Copying System.map" << endl;
    m_transfer->perform(&mapProvider, FileUtil::baseName(mapfile).c_str());

    TerminalProgress kernelProgress("Copying kernel");
    FileDataProvider kernelProvider(kernel.c_str());
    if (config->getVerbose() & Configuration::VERB_PROGRESS)
        kernelProvider.setProgress(&kernelProgress);
    else
        cout << "Copying kernel" << endl;
    m_transfer->perform(&kernelProvider, FileUtil::baseName(kernel).c_str());

    // try to find debugging information
    try {
        Debuglink dbg(kernel.c_str());
        dbg.readDebuglink();
        string debuglink = dbg.findDebugfile(m_rootdir.c_str());
        Debug::debug()->dbg("Found debuginfo file: %s", debuglink.c_str());

        TerminalProgress debugkernelProgress("Copying kernel.debug");
        FileDataProvider debugkernelProvider(debuglink.c_str());
        if (config->getVerbose() & Configuration::VERB_PROGRESS)
            debugkernelProvider.setProgress(&debugkernelProgress);
        else
            cout << "Generating kernel.debug" << endl;
        m_transfer->perform(&debugkernelProvider,
            FileUtil::baseName(debuglink).c_str());

    } catch (const KError &err) {
        Debug::debug()->info("Cannot find debug information: %s", err.what());
    }
}

// -----------------------------------------------------------------------------
string SaveDump::findKernel()
    throw (KError)
{
    Debug::debug()->trace("SaveDump::findKernel()");

    // find the kernel binary
    string binary;

    // 1: vmlinux
    binary = FileUtil::pathconcat(m_rootdir, "/boot",
                                  "vmlinux-" + m_crashrelease);
    Debug::debug()->dbg("Trying %s", binary.c_str());
    if (FileUtil::exists(binary))
        return binary;

    // 2: vmlinux.gz
    binary += ".gz";
    Debug::debug()->dbg("Trying %s", binary.c_str());
    if (FileUtil::exists(binary))
        return binary;

    // 3: vmlinuz (check if ELF file)
    binary = FileUtil::pathconcat(m_rootdir, "/boot",
                                  "vmlinuz-" + m_crashrelease);
    Debug::debug()->dbg("Trying %s", binary.c_str());
    if (FileUtil::exists(binary) && IdentifyKernel::isElfFile(binary.c_str()))
        return binary;

    throw KError("No kernel image found in " +
        FileUtil::pathconcat(m_rootdir, "/boot"));
}

// -----------------------------------------------------------------------------
string SaveDump::findMapfile()
    throw (KError)
{
    Debug::debug()->trace("SaveDump::findMapfile()");

    string map = FileUtil::pathconcat(m_rootdir, "/boot",
                                      "System.map-" + m_crashrelease);
    Debug::debug()->dbg("Trying %s", map.c_str());
    if (FileUtil::exists(map))
        return map;

    throw KError("No System.map found in " +
        FileUtil::pathconcat(m_rootdir, "/boot"));
}

// -----------------------------------------------------------------------------
void SaveDump::checkAndDelete()
    throw (KError)
{
    Debug::debug()->trace("SaveDump::checkAndDelete()");


    // only do that check for local files
    if (m_urlParser.getProtocol() != URLParser::PROT_FILE) {
        Debug::debug()->dbg("Not file protocol. Don't delete.");
        return;
    }

    Configuration *config = Configuration::config();
    string path(FileUtil::pathconcat(m_rootdir, m_urlParser.getPath()));
    unsigned long long freeSize = FileUtil::freeDiskSize(path);
    unsigned long long targetDiskSize = (unsigned long long)config->getFreeDiskSize();

    Debug::debug()->dbg("Free MB: %lld, Configuration: %lld",
            bytes_to_megabytes(freeSize), targetDiskSize);

    if (bytes_to_megabytes(freeSize) < targetDiskSize) {
        FileUtil::rmdir(path, true);
        cout << "Dump too large. Aborting. Check KDUMP_FREE_DISK_SIZE." << endl;
    }
}

// -----------------------------------------------------------------------------
void SaveDump::sendNotification(bool failure, const string &savedir)
    throw ()
{
    Debug::debug()->trace("SaveDump::sendNotification");

#if !HAVE_LIBESMTP
    Debug::debug()->dbg("Email support is not compiled-in.");
#else

    if (m_nomail) {
        Debug::debug()->dbg("--nomail has been specified.");
        return;
    }

    try {

        Configuration *config = Configuration::config();
        if (config->getSmtpServer().size() == 0)
            throw KError("KDUMP_SMTP_SERVER not set.");
        if (config->getNotificationTo().size() == 0)
            throw KError("No recipients specified in KDUMP_NOTIFICATION_TO.");

        if (m_hostname.size() == 0)
            m_hostname = Util::getHostDomain();

        Email email("root@" + m_hostname);
        email.setTo(config->getNotificationTo());

        if (config->getNotificationCc().size() != 0) {
            stringstream split;
            string cc;

            split << config->getNotificationCc();
            while (split >> cc) {
                Debug::debug()->dbg("Adding Cc: %s", cc.c_str());
                email.addCc(cc);
            }
        }

        stringstream ss;
        email.setSubject("kdump: " + m_hostname + " crashed");

        ss << "Your machine " + m_hostname + " crashed." << endl;

        if (failure)
            ss << "Copying dump failed." << endl;
        else
            ss << "Dump has been copied to " + savedir << "." << endl;

        email.setBody(ss.str());

        email.send();
    } catch (const KError &err) {
        Debug::debug()->info("Email failed: %s", err.what());
    }
#endif // HAVE_LIBESMTP
}

//}}}

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:
