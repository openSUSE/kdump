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
#include <list>
#include <strings.h>
#include <cerrno>
#include <memory>
#include <sstream>
#include <fstream>

#include "subcommand.h"
#include "debug.h"
#include "savedump.h"
#include "util.h"
#include "fileutil.h"
#include "rootdirurl.h"
#include "transfer.h"
#include "sshtransfer.h"
#include "configuration.h"
#include "dataprovider.h"
#include "progress.h"
#include "stringutil.h"
#include "vmcoreinfo.h"
#include "identifykernel.h"
#include "email.h"
#include "routable.h"
#include "calibrate.h"

using std::string;
using std::list;
using std::cout;
using std::cerr;
using std::endl;
using std::auto_ptr;
using std::istringstream;
using std::ostringstream;
using std::ifstream;

#define KERNELCOMMANDLINE "/proc/cmdline"

//{{{ SaveDump -----------------------------------------------------------------

// -----------------------------------------------------------------------------
SaveDump::SaveDump()
    throw ()
    : m_dump(DEFAULT_DUMP), m_transfer(NULL), m_usedDirectSave(false),
      m_useMakedumpfile(false), m_split(0), m_threads(0), m_nomail(false)
{
    Debug::debug()->trace("SaveDump::SaveDump()");

    m_options.push_back(new StringOption("dump", 'u', &m_dump,
        "Use the specified dump instead of " DEFAULT_DUMP));
    m_options.push_back(new StringOption("root", 'R', &m_rootdir,
        "Use the specified root directory instead of /"));
    m_options.push_back(new StringOption("kernelversion", 'k', &m_crashrelease,
        "Use the specified kernel version instead of reading VMCOREINFO"));
    m_options.push_back(new StringOption("hostname", 'H', &m_hostname,
        "Use the specified hostname instead of uname()"));
    m_options.push_back(new FlagOption("nomail", 'M', &m_nomail,
        "Don't send notification email even if email has been configured"));
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
void SaveDump::execute()
    throw (KError)
{
    Debug::debug()->trace(__FUNCTION__);
    Debug::debug()->dbg("dump: %s, root: %s, crashrelease: %s, "
        "hostname: %s, nomail: %d",
        m_dump.c_str(), m_rootdir.c_str(), m_crashrelease.c_str(),
        m_hostname.c_str(), int(m_nomail));

    Configuration *config = Configuration::config();

    // check if the dump file actually exists
    if (!m_dump.exists())
        throw KError("The dump file " + m_dump + " does not exist.");

    // build the transfer object
    // prepend a time stamp to the save dir
    string subdir = Stringutil::formatCurrentTime(ISO_DATETIME);
    RootDirURLVector urlv;
    std::istringstream iss(config->KDUMP_SAVEDIR.value());
    FilePath elem;
    while (iss >> elem) {
        RootDirURL url(elem, m_rootdir);
        if (url.getProtocol() != URLParser::PROT_FILE) {
            Routable rt(url.getHostname());
            if (!rt.check(config->KDUMP_NET_TIMEOUT.value())) {
                cerr << "WARNING: Dump target not reachable" << endl;
                elem.appendPath(string("unknown-") + subdir);
            } else
                elem.appendPath(rt.prefsrc() + '-' + subdir);
        } else
            elem.appendPath(subdir);
        urlv.push_back(RootDirURL(elem, m_rootdir));
    }

    m_transfer = getTransfer(urlv);

    // save the dump
    try {
        saveDump(urlv);
    } catch (const KError &error) {
        setErrorCode(1);

        sendNotification(true, urlv);

        // run checkAndDelete() in any case
        try {
            checkAndDelete(urlv);
        } catch (const KError &error) {
            cout << error.what() << endl;
        }

        if (config->KDUMP_CONTINUE_ON_ERROR.value())
            cout << error.what() << endl;
        else
            throw;
    }

    // send the email afterwards
    sendNotification(false, urlv);

    // because we don't know the file size in advance, check
    // afterwards if the disk space is not sufficient and delete
    // the dump again
    try {
        checkAndDelete(urlv);
    } catch (const KError &error) {
        setErrorCode(1);
        if (config->KDUMP_CONTINUE_ON_ERROR.value())
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
        if (config->KDUMP_CONTINUE_ON_ERROR.value())
            cout << error.what() << endl;
        else
            throw;
    }

    try {
        fillVmcoreinfo();
    } catch (const KError &error) {
        Debug::debug()->dbg("Error when reading VMCOREINFO: %s", error.what());
    }

    // if we have no VMCOREINFO, then try to command line to get the
    // kernel version
    if (m_crashrelease.size() == 0) {
        try {
            m_crashrelease = getKernelReleaseCommandline();
        } catch (const KError &error) {
            Debug::debug()->dbg("Unable to retrieve kernel version: %s",
                    error.what());
        }
    }

    // generate the README file
    try {
        generateInfo();
    } catch (const KError &error) {
        setErrorCode(1);
        if (config->KDUMP_CONTINUE_ON_ERROR.value())
            cout << error.what() << endl;
        else
            throw;
    }

    // copy kernel
    if (m_crashrelease.size() > 0) {
        try {
            if (config->KDUMP_COPY_KERNEL.value())
                copyKernel();
        } catch (const KError &error) {
            setErrorCode(1);
            if (config->KDUMP_CONTINUE_ON_ERROR.value())
                cout << error.what() << endl;
            else
                throw;
        }
    } else {
        Debug::debug()->info("Don't copy the kernel and System.map because of missing "
            "crash kernel release.");
    }
}

// -----------------------------------------------------------------------------
void SaveDump::saveDump(const RootDirURLVector &urlv)
    throw (KError)
{
    Configuration *config = Configuration::config();

    // build the data provider object
    int dumplevel = config->KDUMP_DUMPLEVEL.value();
    if (dumplevel < 0 || dumplevel > 31) {
        Debug::debug()->info("Dumplevel %d is invalid. Using 0.", dumplevel);
        dumplevel = 0;
    }

    // check if the vmcore file does exist and has a size of > 0
    if (!m_dump.exists()) {
        throw KError("Core file " + m_dump + " does not exist.");
    }
    if (m_dump.fileSize() == 0) {
        throw KError("Zero size vmcore (" + m_dump + ").");
    }

    Terminal terminal;

    // Save a copy of dmesg
    try {
        string directCmdline = "makedumpfile --dump-dmesg " + m_dump;
	string pipeCmdline = "makedumpfile --dump-dmesg -F " + m_dump;
	ProcessDataProvider logProvider(
	    pipeCmdline.c_str(), directCmdline.c_str());

	cout << "Extracting dmesg" << endl;
	terminal.printLine();
	TerminalProgress logProgress("Saving dmesg");
        if (config->KDUMP_VERBOSE.value()
	    & Configuration::VERB_PROGRESS)
            logProvider.setProgress(&logProgress);
        else
            cout << "Saving dmesg ..." << endl;
        m_transfer->perform(&logProvider, "dmesg.txt", NULL);
	terminal.printLine();
    } catch (const KError &error) {
	cout << error.what() << endl;
    } catch (...) {
	cout << "Extracting failed." << endl;
    }

    // dump format
    const string &dumpformat = config->KDUMP_DUMPFORMAT.value();
    DataProvider *provider;

    bool noDump = strcasecmp(dumpformat.c_str(), "none") == 0;
    bool useElf = strcasecmp(dumpformat.c_str(), "elf") == 0;
    bool useCompressed = strcasecmp(dumpformat.c_str(), "compressed") == 0;
    bool useLZO = strcasecmp(dumpformat.c_str(), "lzo") == 0;
    bool useSnappy = strcasecmp(dumpformat.c_str(), "snappy") == 0;

    if (noDump)
	return;			// nothing to be done

    unsigned long cpus = config->KDUMP_CPUS.value();
    unsigned long online_cpus = 0;
    if (cpus) {
        SystemCPU syscpu;
        online_cpus = syscpu.numOnline();
        /* Limit kdump cpus to online cpus if (KDUMP_CPUS > online cpus) */
        if (cpus > online_cpus)
            cpus = online_cpus;
    }
    if (!config->kdumptoolContainsFlag("SINGLE") &&
        cpus > 1) {

        /* The check for NOSPLIT is for backward compatibility */
        if (config->kdumptoolContainsFlag("SPLIT") &&
            !config->kdumptoolContainsFlag("NOSPLIT")) {
            if (!useElf)
                m_split = cpus;
            else
                cerr << "Splitting ELF dumps is not supported." << endl;
        } else {
            if (!useElf)
                m_threads = cpus - 1;
            else
                cerr << "Multithreading is unavailable for ELF dumps" << endl;
        }
    }

    bool excludeDomU = false;
    if (!config->kdumptoolContainsFlag("XENALLDOMAINS") &&
	Util::isXenCoreDump(m_dump.c_str()))
      excludeDomU = true;

    if (useElf && dumplevel == 0 && !excludeDomU) {
        // use file source?
        provider = new FileDataProvider(m_dump.c_str());
        m_useMakedumpfile = false;
    } else {
        // use makedumpfile
        ostringstream cmdline;
        cmdline << "makedumpfile ";
	if (m_split)
	    cmdline << "--split ";
        if (m_threads) {
	    SystemCPU syscpu;
            cmdline << "--num-threads " << m_threads << " ";
        }
        cmdline << config->MAKEDUMPFILE_OPTIONS.value() << " ";
        cmdline << "-d " << config->KDUMP_DUMPLEVEL.value() << " ";
	if (excludeDomU)
	    cmdline << "-X ";
        if (useElf)
            cmdline << "-E ";
        if (useCompressed)
            cmdline << "-c ";
        if (useLZO)
            cmdline << "-l ";
	if (useSnappy)
	    cmdline << "-p ";
        cmdline << m_dump;

        string directCmdline = cmdline.str();
        string pipeCmdline = cmdline.str() + " -F"; // flattened format

        provider = new ProcessDataProvider(pipeCmdline.c_str(),
            directCmdline.c_str());
        m_useMakedumpfile = true;
    }

    try {
        if (m_useMakedumpfile) {
            cout << "Saving dump using makedumpfile" << endl;
            terminal.printLine();
        }
        TerminalProgress progress("Saving dump");
        if (config->KDUMP_VERBOSE.value()
	    & Configuration::VERB_PROGRESS)
            provider->setProgress(&progress);
        else
            cout << "Saving dump ..." << endl;
	if (m_split) {
	    StringVector targets;
	    for (unsigned long i = 1; i <= m_split; ++i) {
		ostringstream ss;
		ss << "vmcore" << i;
		targets.push_back(ss.str());
	    }
	    m_transfer->perform(provider, targets, &m_usedDirectSave);
	} else {
	    m_transfer->perform(provider, "vmcore", &m_usedDirectSave);
	}
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
    list<FilePath> paths;
    Configuration *config = Configuration::config();

    paths.push_back("/bin/makedumpfile-R.pl");
    paths.push_back("/sbin/makedumpfile-R.pl");
    paths.push_back("/usr/bin/makedumpfile-R.pl");
    paths.push_back("/usr/sbin/makedumpfile-R.pl");
    paths.push_back("/usr/local/bin/makedumpfile-R.pl");
    paths.push_back("/usr/local/sbin/makedumpfile-R.pl");
    paths.push_back("/root/bin/makedumpfile-R.pl");

    string makedumpfile_binary;

    for (list<FilePath>::const_iterator it = paths.begin();
            it != paths.end(); ++it) {
        if (it->exists()) {
            makedumpfile_binary = *it;
            break;
        }
    }

    if (makedumpfile_binary.size() == 0)
        throw KError("makedumpfile-R.pl not found.");

    FileDataProvider provider(makedumpfile_binary.c_str());
    TerminalProgress progress("Saving makedumpfile-R.pl");
    if (config->KDUMP_VERBOSE.value()
	& Configuration::VERB_PROGRESS)
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
    ostringstream ss;

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
    if (config->KDUMP_VERBOSE.value()
	& Configuration::VERB_PROGRESS)
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

    ostringstream ss;

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
       << Stringutil::number2string(config->KDUMP_DUMPLEVEL.value()) << endl;
    ss << "Dump format    : " << config->KDUMP_DUMPFORMAT.value() << endl;
    if (m_split && m_usedDirectSave)
        ss << "Split parts    : " << m_split << endl;
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
    if (config->KDUMP_VERBOSE.value()
	& Configuration::VERB_PROGRESS)
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

    FilePath mapfile = findMapfile();
    FilePath kernel = findKernel();
    FilePath fp;

    // mapfile
    TerminalProgress mapProgress("Copying System.map");
    (fp = m_rootdir).appendPath(mapfile);
    FileDataProvider mapProvider(fp.c_str());
    if (config->KDUMP_VERBOSE.value()
	& Configuration::VERB_PROGRESS)
        mapProvider.setProgress(&mapProgress);
    else
        cout << "Copying System.map" << endl;
    m_transfer->perform(&mapProvider, mapfile.baseName().c_str());

    TerminalProgress kernelProgress("Copying kernel");
    (fp = m_rootdir).appendPath(kernel);
    FileDataProvider kernelProvider(fp.c_str());
    if (config->KDUMP_VERBOSE.value()
	& Configuration::VERB_PROGRESS)
        kernelProvider.setProgress(&kernelProgress);
    else
        cout << "Copying kernel" << endl;
    m_transfer->perform(&kernelProvider, kernel.baseName().c_str());
}

// -----------------------------------------------------------------------------
string SaveDump::findKernel()
    throw (KError)
{
    Debug::debug()->trace("SaveDump::findKernel()");

    // find the kernel binary
    FilePath binary, binaryroot;

    // 1: vmlinux
    (binary = "/boot").appendPath("vmlinux-" + m_crashrelease);
    (binaryroot = m_rootdir).appendPath(binary);
    Debug::debug()->dbg("Trying %s", binaryroot.c_str());
    if (binaryroot.exists())
        return binary;

    // 2: vmlinux.gz
    binary += ".gz";
    binaryroot += ".gz";
    Debug::debug()->dbg("Trying %s", binaryroot.c_str());
    if (binaryroot.exists())
        return binary;

    // 3: vmlinuz (check if ELF file)
    (binary = "/boot").appendPath("vmlinuz-" + m_crashrelease);
    (binaryroot = m_rootdir).appendPath(binary);
    Debug::debug()->dbg("Trying %s", binaryroot.c_str());
    if (binaryroot.exists() &&
            Util::isElfFile(binaryroot.c_str())) {
        return binary;
    }

    // 4: image
    (binary = "/boot").appendPath("image-" + m_crashrelease);
    (binaryroot = m_rootdir).appendPath(binary);
    Debug::debug()->dbg("Trying %s", binaryroot.c_str());
    if (binaryroot.exists())
        return binary;

    // 5: Image
    (binary = "/boot").appendPath("Image-" + m_crashrelease);
    (binaryroot = m_rootdir).appendPath(binary);
    Debug::debug()->dbg("Trying %s", binaryroot.c_str());
    if (binaryroot.exists())
        return binary;

    FilePath fp = m_rootdir;
    fp.appendPath("/boot");
    throw KError("No kernel image found in " + fp);
}

// -----------------------------------------------------------------------------
string SaveDump::findMapfile()
    throw (KError)
{
    Debug::debug()->trace("SaveDump::findMapfile()");

    FilePath map, maproot;
    (map = "/boot").appendPath("System.map-" + m_crashrelease);
    (maproot = m_rootdir).appendPath(map);
    Debug::debug()->dbg("Trying %s", maproot.c_str());
    if (maproot.exists())
        return map;

    FilePath fp = m_rootdir;
    fp.appendPath("/boot");
    throw KError("No System.map found in " + fp);
}

// -----------------------------------------------------------------------------
void SaveDump::checkAndDelete(const RootDirURLVector &urlv)
    throw (KError)
{
    RootDirURLVector::const_iterator it;
    for (it = urlv.begin(); it != urlv.end(); ++it)
	check_one(*it);
}

// -----------------------------------------------------------------------------
void SaveDump::check_one(const RootDirURL &parser)
    throw (KError)
{
    Debug::debug()->trace("SaveDump::check_one(\"%s\")",
			  parser.getURL().c_str());

    // only do that check for local files
    if (parser.getProtocol() != URLParser::PROT_FILE) {
        Debug::debug()->dbg("Not file protocol. Don't delete.");
        return;
    }

    FilePath path = parser.getRealPath();
    Configuration *config = Configuration::config();

    unsigned long long freeSize = path.freeDiskSize();
    unsigned long long targetDiskSize = config->KDUMP_FREE_DISK_SIZE.value();

    Debug::debug()->dbg("Free MB: %lld, Configuration: %lld",
            bytes_to_megabytes(freeSize), targetDiskSize);

    if (bytes_to_megabytes(freeSize) < targetDiskSize) {
        path.rmdir(true);
        throw KError("Dump too large. Aborting. Check KDUMP_FREE_DISK_SIZE.");
    }
}

// -----------------------------------------------------------------------------
void SaveDump::sendNotification(bool failure, const RootDirURLVector &urlv)
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

    Configuration *config = Configuration::config();
    const std::string &SmtpServer = config->KDUMP_SMTP_SERVER.value();
    const std::string &NotificationTo = config->KDUMP_NOTIFICATION_TO.value();

    // Email not configured
    if (SmtpServer.size() == 0 && NotificationTo.size() == 0) {
        Debug::debug()->dbg("Email not configured.");
        return;
    }

    try {
        if (SmtpServer.size() == 0)
            throw KError("KDUMP_SMTP_SERVER not set.");
        if (NotificationTo.size() == 0)
            throw KError("No recipients specified in KDUMP_NOTIFICATION_TO.");

        if (m_hostname.size() == 0)
            m_hostname = Util::getHostDomain();

        Email email("root@" + m_hostname);
        email.setHostname(m_hostname);
        email.setTo(NotificationTo);

	const std::string &NotificationCc =
            config->KDUMP_NOTIFICATION_CC.value();
        if (NotificationCc.size() != 0) {
            istringstream split(NotificationCc);
            string cc;

            while (split >> cc) {
                Debug::debug()->dbg("Adding Cc: %s", cc.c_str());
                email.addCc(cc);
            }
        }

        ostringstream ss;
        email.setSubject("kdump: " + m_hostname + " crashed");

        ss << "Your machine " + m_hostname + " crashed." << endl;

        if (failure)
            ss << "Copying dump failed." << endl;
        else {
	    ss << "Dump has been copied to" << endl;
	    RootDirURLVector::const_iterator it;
            for (it = urlv.begin(); it != urlv.end(); ++it)
                ss << it->getURL() << endl;
	}

        email.setBody(ss.str());

        email.send();
    } catch (const KError &err) {
        Debug::debug()->info("Email failed: %s", err.what());
    }
#endif // HAVE_LIBESMTP
}

// -----------------------------------------------------------------------------
string SaveDump::getKernelReleaseCommandline()
    throw (KError)
{
    Debug::debug()->trace("SaveDump::getKernelReleaseCommandline()");

    ifstream fin(KERNELCOMMANDLINE);
    if (!fin) {
        throw KError("Unable to open " + string(KERNELCOMMANDLINE) + ".");
    }

    KString s;
    KString version;
    while (fin >> s) {
        Debug::debug()->trace("Token: %s", s.c_str());
        const string pfx = "kernelversion=";
        if (s.startsWith(pfx))
            version = s.substr(pfx.size());
    }

    if (version.size() == 0) {
        throw KError("'kernelversion=' command line missing");
    }

    fin.close();
    return version;
}

// -----------------------------------------------------------------------------
Transfer *SaveDump::getTransfer(const RootDirURLVector &urlv)
    throw (KError)
{
    Debug::debug()->trace("SaveDump::getTransfer(%p)",
			  &urlv);

    if (urlv.size() == 0)
	throw KError("No target specified!");

    switch (urlv.begin()->getProtocol()) {
        case URLParser::PROT_FILE:
            Debug::debug()->dbg("Returning FileTransfer");
            return new FileTransfer(urlv);

        case URLParser::PROT_FTP:
            Debug::debug()->dbg("Returning FTPTransfer");
            return new FTPTransfer(urlv);

        case URLParser::PROT_SFTP:
            Debug::debug()->dbg("Returning SFTPTransfer");
            return new SFTPTransfer(urlv);

        case URLParser::PROT_SSH:
	    Debug::debug()->dbg("Returning SSHTransfer");
	    return new SSHTransfer(urlv);

        case URLParser::PROT_NFS:
            Debug::debug()->dbg("Returning NFSTransfer");
            return new NFSTransfer(urlv);

        case URLParser::PROT_CIFS:
            Debug::debug()->dbg("Returning CIFSTransfer");
            return new CIFSTransfer(urlv);

        default:
            throw KError("Unknown protocol.");
    }
}

//}}}

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:
