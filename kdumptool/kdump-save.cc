/*
 * Copyright (c) 2021 SUSE LLC
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
 * along with this program; if not, see <https://www.gnu.org/licenses>.
 */

#include <cerrno>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>

#include <signal.h>
#include <sys/resource.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

#include "config.h"
#include "global.h"
#include "configuration.h"
#include "deletedumps.h"
#include "fileutil.h"
#include "ledblink.h"
#include "mounts.h"
#include "process.h"
#include "savedump.h"

using std::cerr;
using std::cout;
using std::endl;
using std::ifstream;
using std::istringstream;
using std::ofstream;
using std::ostringstream;
using std::string;
using std::stringstream;

#define ARRAY_SIZE(arr) (sizeof(arr) / sizeof((arr)[0]))

static const char BIN_SH[] = "/bin/sh";

static const char KDUMP_DIR[] = "/kdump";
static const char MNT_DIR[] = "/mnt";

static const char CONFIG_FILE[] = "/etc/sysconfig/kdump";
static const char KERNEL_CMDLINE[] = "/proc/cmdline";

static const char CORE_PATTERN[] = "/proc/sys/kernel/core_pattern";

static const char HOSTNAME[] = "/etc/hostname.kdump";

static int runCommand(const string &command)
{
    cout << "Running " << command << endl;

    ProcessFilter pf;
    StringVector args { "-c", command };
    return pf.execute(BIN_SH, args);
}

static void runShell()
{
    cout << "Type 'reboot -f' to reboot the system or 'exit' to" << endl
         << "resume the boot process." << endl;
        ProcessFilter().execute(BIN_SH, StringVector());
}

static void maybeReboot()
{
    Configuration *config = Configuration::config();
    if (config->KDUMP_IMMEDIATE_REBOOT.value()) {
        ProcessFilter().execute("umount", StringVector { "-a" });
        ProcessFilter().execute("reboot", StringVector { "-f" });
    }
}

#if HAVE_FADUMP

static const char FADUMP_ENABLED[] = "/sys/kernel/fadump_enabled";
static const char FADUMP_RELEASE_MEM[] = "/sys/kernel/fadump_release_mem";

/**
 * Check if fadump is enabled.
 *
 * @return @c true if fadump is enabled,
 *         @c false if fadump is not availabled or disabled.
 */
static bool fadumpEnabled()
{
    FilePath fp(FADUMP_ENABLED);
    if (!fp.exists())
        return false;

    ifstream fin;
    if (!fin)
        throw KSystemError(fp, errno);

    int enabled;
    fin >> enabled;
    return enabled;
}

static bool handleExitFadump()
{
    if (!fadumpEnabled())
        return false;

    Configuration *config = Configuration::config();

    // release memory if possible
    FilePath fp(FADUMP_RELEASE_MEM);
    if (fp.exists() && !config->KDUMP_IMMEDIATE_REBOOT.value()) {
        ofstream fout(fp);
        fout << "1" << endl;
        fout.close();
    }

    if (config->KDUMP_FADUMP_SHELL.value()) {
        cout << endl
             << "Dump saving completed." << endl;
        runShell();
    }

    maybeReboot();

    // unmount kdump directories
    StringVector dirs;
    KernelMountTable tbl;
    MountTable::iterator it(tbl, MNT_ITER_FORWARD);
    while (++it) {
        FilePath fp(it->target());
        if (!fp.startsWith(KDUMP_DIR))
            continue;
        dirs.push_back(fp);
    }
    ProcessFilter().execute("umount", dirs);

    return true;
}

#else

static inline bool handleExitFadump()
{
    return false;
}

#endif

// If KDUMP_IMMEDIATE_REBOOT is false, then open a shell. If it's true, then
// reboot.
static void handleExit()
{
    if (!handleExitFadump()) {
        maybeReboot();

        cout << endl << "Dump saving completed." << endl;
        runShell();
    }
}

static void handleError(const string& message)
{
    Configuration *config = Configuration::config();
    if (!config->KDUMP_CONTINUE_ON_ERROR.value())
        throw KError(message);

    cerr << message << "." << endl;
}

class BlinkProcess
{
    private:
        pid_t m_pid;

    public:
        BlinkProcess();
        ~BlinkProcess();
};

BlinkProcess::BlinkProcess()
{
    pid_t pid = fork();
    if (pid < 0)
        throw KSystemError("Fork failed", errno);

    if (pid == 0) {
        try {
            LedBlink blink;
            blink.execute();
        } catch(std::exception &e) {
            cerr << "Cannot blink LEDs: " << e.what() << "." << endl;
            _exit(1);
        }
        _exit(0);
    }

    m_pid = pid;
}

BlinkProcess::~BlinkProcess()
{
    if (m_pid > 0) {
        // intentionally ignore errors
        kill(m_pid, SIGTERM);
        waitpid(m_pid, NULL, 0);
    }
}

class CorePatternOverride
{
    public:
        CorePatternOverride(const string &pattern);
        ~CorePatternOverride();

    private:
        stringstream m_orig;
};

CorePatternOverride::CorePatternOverride(const string &pattern)
{
    ifstream fin(CORE_PATTERN);
    if (!fin)
        throw KSystemError(CORE_PATTERN, errno);
    m_orig << fin.rdbuf();

    ofstream fout(CORE_PATTERN);
    if (!fout)
        throw KSystemError(CORE_PATTERN, errno);
    fout << pattern;
}

CorePatternOverride::~CorePatternOverride()
{
    ofstream fout(CORE_PATTERN);
    fout << m_orig.rdbuf();
}

class CoreLimitOverride
{
    public:
        CoreLimitOverride(rlim_t limit);
        ~CoreLimitOverride();

    private:
        struct rlimit m_orig;
};

CoreLimitOverride::CoreLimitOverride(rlim_t limit)
{
    if (getrlimit(RLIMIT_CORE, &m_orig))
        throw KSystemError("Cannot get core limit", errno);

    struct rlimit core_rlimit;
    core_rlimit.rlim_cur = limit;
    core_rlimit.rlim_max = limit;
    if (setrlimit(RLIMIT_CORE, &core_rlimit))
        throw KSystemError("Cannot set new core limit", errno);
}

CoreLimitOverride::~CoreLimitOverride()
{
    setrlimit(RLIMIT_CORE, &m_orig);
}

class EnvironmentOverride
{
    public:
        EnvironmentOverride(const char *name, const char *value);
        ~EnvironmentOverride();

    private:
        const string m_name;
        string m_orig;
        bool m_orig_set;

        int set(const char *value);
};

EnvironmentOverride::EnvironmentOverride(const char *name, const char *value)
    : m_name(name), m_orig_set(false)
{
    const char *orig = getenv(m_name.c_str());
    if (orig) {
        m_orig.assign(orig);
        m_orig_set = true;
    }

    if (set(value))
        throw KSystemError("Cannot override environment variable " + m_name,
                           errno);
}

EnvironmentOverride::~EnvironmentOverride()
{
    set(m_orig.c_str());
}

int EnvironmentOverride::set(const char *value)
{
    return value
        ? setenv(m_name.c_str(), value, 1)
        : unsetenv(m_name.c_str());
}

static void deleteDumps()
{
    try {
        DeleteDumps deleter;
        deleter.rootDir(KDUMP_DIR);
        deleter.deleteAll();
    } catch (KError &err) {
        handleError(string("Cannot delete old dumps: ") + err.what());
    }
}

static void saveDump()
{
    string hostname;
    ifstream fin(HOSTNAME);
    if (!fin)
        throw KSystemError(HOSTNAME, errno);
    fin >> hostname;
    fin.close();

    // set HOME to find the public/private key
    EnvironmentOverride HomeEnv("HOME", KDUMP_DIR);

    // set TMPDIR for makedumpfile temporary bitmap
    FilePath tmpdir(KDUMP_DIR);
    tmpdir.appendPath("tmp");
    EnvironmentOverride TmpdirEnv("TMPDIR", tmpdir.c_str());

    try {
        SaveDump saver;
        saver.rootDir(KDUMP_DIR);
        saver.hostName(hostname);
        saver.create();
    } catch (KError &err) {
        Configuration *config = Configuration::config();
        if (!config->KDUMP_CONTINUE_ON_ERROR.value())
            throw KError(string("Cannot save dump: ") + err.what());
    }
}

static void execute()
{
    Configuration *config = Configuration::config();

    // start LED blinking
    BlinkProcess blinker;

    // create mountpoint for NFS/CIFS
    FilePath mnt(MNT_DIR);
    if (!mnt.exists())
        mnt.mkdir(false);

    const string &transfer = config->KDUMP_TRANSFER.value();
    if (!transfer.empty()) {
        int code = runCommand(transfer);
        if (code)
            cerr << "Transfer exit code is " << code << endl;
    } else {
        // pre-script
        const string &prescript = config->KDUMP_PRESCRIPT.value();
        if (!prescript.empty()) {
            int code = runCommand(prescript);
            if (code != 0 && !config->KDUMP_CONTINUE_ON_ERROR.value()) {
                ostringstream msg;
                msg << "Pre-script failed (" << code << ")";
                throw KError(msg.str());
            }
        }

        // delete old dumps
        deleteDumps();

        // save the dump
        saveDump();

        // post-script
        const string &postscript = config->KDUMP_POSTSCRIPT.value();
        if (!postscript.empty()) {
            int code = runCommand(postscript);
            if (code != 0 && !config->KDUMP_CONTINUE_ON_ERROR.value()) {
                ostringstream msg;
                msg << "Post-script failed (" << code << ")";
                throw KError(msg.str());
            }
        }
    }

    handleExit();
}

int main(int argc, char **argv)
{
    try {
        // sanity check
        if (!FilePath(DEFAULT_DUMP).exists())
            throw KError("Kdump initrd booted in non-kdump kernel");

        // override core file pattern and limit (for debugging kdump itself)
        FilePath core_pattern(KDUMP_DIR);
        core_pattern.appendPath("tmp/core.kdumptool");
        CorePatternOverride pattern_override(core_pattern);
        CoreLimitOverride limit_override(RLIM_INFINITY);

        Configuration *config = Configuration::config();
        config->readFile(CONFIG_FILE);

        execute();
    } catch(std::exception &e) {
        cerr << "Cannot save dump!" << endl
             << endl
             << "  " << e.what() << "." << endl
             << endl
             << "Something failed. You can try to debug it here." << endl;
        runShell();
        return 1;
    }

    return 0;
}
