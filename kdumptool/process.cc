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
#include <string>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>

#include "process.h"
#include "global.h"
#include "stringutil.h"
#include "util.h"

using std::string;

// -----------------------------------------------------------------------------
Process::Process()
    throw ()
    : m_stdinFd(-1), m_stdoutFd(-1), m_stderrFd(-1),
      m_needCloseStdout(false), m_needCloseStdin(false),
      m_needCloseStderr(false)
{}

// -----------------------------------------------------------------------------
void Process::setStdinFd(int fd)
    throw ()
{
    m_stdinFd = fd;
    m_needCloseStdin = false;
}

// -----------------------------------------------------------------------------
void Process::setStdoutFd(int fd)
    throw ()
{
    m_stdoutFd = fd;
    m_needCloseStdout = false;
}

// -----------------------------------------------------------------------------
void Process::setStderrFd(int fd)
    throw ()
{
    m_stderrFd = fd;
    m_needCloseStderr = false;
}

// -----------------------------------------------------------------------------
void Process::setStdinFile(const char *filename)
    throw (KError)
{
    m_stdinFd = open(filename, O_RDONLY);
    if (m_stdinFd < 0)
        throw KSystemError("Opening " + string(filename) + " failed.", errno);

    m_needCloseStdin = true;
}

// -----------------------------------------------------------------------------
void Process::setStdoutFile(const char *filename)
    throw (KError)
{
    m_stdoutFd = open(filename, O_WRONLY | O_CREAT, 0644);
    if (m_stdoutFd < 0)
        throw KSystemError("Opening " + string(filename) + " failed.", errno);

    m_needCloseStdout = true;
}

// -----------------------------------------------------------------------------
void Process::setStderrFile(const char *filename)
    throw (KError)
{
    m_stderrFd = open(filename, O_WRONLY | O_CREAT, 0644);
    if (m_stderrFd < 0)
        throw KSystemError("Opening " + string(filename) + " failed.", errno);

    m_needCloseStderr = true;
}

// -----------------------------------------------------------------------------
uint8_t Process::execute(const string &name, const StringVector &args)
    throw (KError)
{
    pid_t child;
    int status = 0;

    child = fork();
    if (child > 0) { // parent code, just wait until the child has exited
        int ret;

        ret = waitpid(child, &status, 0);
        if (ret != child) {
            throw KSystemError("Process::execute() waits for "
                + Stringutil::number2string(child) + " but "
                + Stringutil::number2string(ret) + " exited.", errno);
        }
    } else if (child == 0) { // child code, execute the process

        if (m_stdinFd > 0) {
            close(STDIN_FILENO);
            dup2(m_stdinFd, STDIN_FILENO);
        }
        if (m_stdoutFd > 0) {
            close(STDOUT_FILENO);
            dup2(m_stdoutFd, STDOUT_FILENO);
        }
        if (m_stderrFd > 0) {
            close(STDERR_FILENO);
            dup2(m_stderrFd, STDERR_FILENO);
        }

        executeProcess(name, args);

    } else { // error in fork()
        throw KSystemError("fork() failed in Process::execute", errno);
    }

    // cleanup
    if (m_needCloseStdin) {
        close(m_stdinFd);
        m_stdinFd = -1;
    }
    if (m_needCloseStdout) {
        close(m_stdoutFd);
        m_stdoutFd = -1;
    }
    if (m_needCloseStderr) {
        close(m_stderrFd);
        m_stderrFd = -1;
    }

    return WEXITSTATUS(status);
}

// -----------------------------------------------------------------------------
void Process::executeProcess(const string &name, const StringVector &args)
    throw (KError)
{
    StringVector fullV = args;
    fullV.insert(fullV.begin(), name);
    char **vector = Stringutil::stringv2charv(fullV);
    int ret = execvp(name.c_str(), vector);
    Util::freev(vector);

    if (ret < 0)
        throw KSystemError("Execution of '" + name + "' failed.", errno);
}

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:
