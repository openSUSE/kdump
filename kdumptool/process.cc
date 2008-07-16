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
#include <algorithm>
#include <sys/types.h>
#include <sys/stat.h>
#include <sys/wait.h>
#include <fcntl.h>
#include <errno.h>
#include <unistd.h>
#include <errno.h>

#include "process.h"
#include "global.h"
#include "stringutil.h"
#include "util.h"
#include "debug.h"

using std::string;
using std::min;
using std::max;

// -----------------------------------------------------------------------------
Process::Process()
    throw ()
    : m_stdinFd(-1), m_stdoutFd(-1), m_stderrFd(-1),
      m_needCloseStdin(false), m_needCloseStdout(false),
      m_needCloseStderr(false),
      m_stdinBuffer(NULL), m_stdoutBuffer(NULL), m_stderrBuffer(NULL)
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
void Process::setStdinBuffer(ByteVector *buffer)
    throw ()
{
    m_stdinBuffer = buffer;
    m_needCloseStdin = false;
    m_stdinFd = -1;
}

// -----------------------------------------------------------------------------
void Process::setStdoutBuffer(ByteVector *buffer)
    throw ()
{
    m_stdoutBuffer = buffer;
    m_needCloseStdout = false;
    m_stdoutFd = -1;
}

// -----------------------------------------------------------------------------
void Process::setStderrBuffer(ByteVector *buffer)
    throw ()
{
    m_stderrBuffer = buffer;
    m_needCloseStderr = false;
    m_stderrFd = -1;
}

// -----------------------------------------------------------------------------
uint8_t Process::execute(const string &name, const StringVector &args)
    throw (KError)
{
    pid_t child;
    int status = 0;

    //
    // setup pipes for stdin, stderr, stdout if we use buffers
    //
    int stdin_pipes[2] = { 0, 0 };
    int stdout_pipes[2] = { 0, 0 };
    int stderr_pipes[2] = { 0, 0 };
    if (m_stdinBuffer) {
        int ret = pipe(stdin_pipes);
        if (ret < 0)
            throw KSystemError("Could not create stdin pipes.", errno);
    }
    if (m_stdoutBuffer) {
        int ret = pipe(stdout_pipes);
        if (ret < 0)
            throw KSystemError("Could not create stdout pipes.", errno);
    }
    if (m_stderrBuffer) {
        int ret = pipe(stderr_pipes);
        if (ret < 0)
            throw KSystemError("Could not create stderr pipes.", errno);
    }

    //
    // execute the child
    //

    child = fork();
    if (child > 0) { // parent code, just wait until the child has exited
        int ret;

        // close the ends of the pipes that are not necessary
        if (stdin_pipes[0]) // read end of stdin
            close(stdin_pipes[0]);
        if (stdout_pipes[1]) // write end of stdout
            close(stdout_pipes[1]);
        if (stderr_pipes[1]) // write end of stderr
            close(stderr_pipes[1]);

        if (stdin_pipes[1] || stdout_pipes[0] || stderr_pipes[0]) {

            loff_t write_pos = 0;

            while (true) {
                int maxfd = -1;
                fd_set read_fds;
                fd_set write_fds;
                struct timeval tv = { 1, 0 };
                int retval;

                FD_ZERO(&read_fds);
                FD_ZERO(&write_fds);

                if (stdin_pipes[1]) {
                    // check if fd is still valid
                    if (dup2(stdin_pipes[1], stdin_pipes[1]) < 0) {
                        close(stdin_pipes[1]);
                        stdin_pipes[1] = 0;
                    } else {
                        FD_SET(stdin_pipes[1], &write_fds);
                        maxfd = max(maxfd, stdin_pipes[1]);
                    }
                }
                if (stdout_pipes[0]) {
                    // check if fd is still valid
                    if (dup2(stdout_pipes[0], stdout_pipes[0]) < 0) {
                        close(stdout_pipes[0]);
                        stdout_pipes[0] = 0;
                    } else {
                        FD_SET(stdout_pipes[0], &read_fds);
                        maxfd = max(maxfd, stdout_pipes[0]);
                    }
                }
                if (stderr_pipes[0]) {
                    // check if fd is still valid
                    if (dup2(stderr_pipes[0], stderr_pipes[0]) < 0) {
                        close(stderr_pipes[0]);
                        stderr_pipes[0] = 0;
                    } else {
                        FD_SET(stderr_pipes[0], &read_fds);
                        maxfd = max(maxfd, stderr_pipes[0]);
                    }
                }

                // one more
                maxfd++;

                // if we don't have valid FDs, process the read()/write() once
                if (maxfd == 0)
                    retval = 0;
                else
                    retval = select(maxfd, &read_fds, &write_fds, NULL, &tv);

                if (retval < 0 && errno != EBADF) {
                    closeFds();
                    throw KSystemError("select() failed", errno);
                } else if (retval > 0) {

                    char buffer[BUFSIZ];

                    if (FD_ISSET(stdin_pipes[1], &write_fds)) {

                        loff_t end_pos = min((loff_t)m_stdinBuffer->size(),
                                             write_pos+BUFSIZ);

                        copy(m_stdinBuffer->begin() + write_pos,
                             m_stdinBuffer->begin() + end_pos,
                             buffer);

                        retval = write(stdin_pipes[1], buffer, end_pos - write_pos);
                        if (retval < 0) {
                            closeFds();
                            throw KSystemError("write() failed", errno);
                        }

                        write_pos += retval;

                        if (write_pos >= (loff_t)m_stdinBuffer->size())
                            close(stdin_pipes[1]);

                    }
                    if (FD_ISSET(stdout_pipes[0], &read_fds)) {

                        retval = read(stdout_pipes[0], buffer, BUFSIZ);
                        if (retval < 0) {
                            closeFds();
                            throw KSystemError("read() failed", errno);
                        }

                        if (retval == 0) {
                            close(stdout_pipes[0]);
                            stdout_pipes[0] = 0;
                        } else {
                            m_stdoutBuffer->insert(m_stdoutBuffer->end(),
                                                   buffer, buffer+retval);
                        }
                    }
                    if (FD_ISSET(stderr_pipes[0], &read_fds)) {

                        retval = read(stderr_pipes[0], buffer, BUFSIZ);
                        if (retval < 0) {
                            closeFds();
                            throw KSystemError("read() failed", errno);
                        }

                        if (retval == 0) {
                            close(stderr_pipes[0]);
                            stderr_pipes[0] = 0;
                        } else {
                            m_stderrBuffer->insert(m_stderrBuffer->end(),
                                                   buffer, buffer+retval);
                        }
                    }
                }

                if (maxfd == 0) {
                    // check if the process is still running
                    ret = waitpid(child, &status, 0);
                    if (ret != child) {
                        throw KSystemError("Process::execute() waits for "
                        + Stringutil::number2string(child) + " but "
                        + Stringutil::number2string(ret) + " exited.", errno);
                    }
                    break;
                }
            }

        } else {
            ret = waitpid(child, &status, 0);
            if (ret != child) {
                throw KSystemError("Process::execute() waits for "
                    + Stringutil::number2string(child) + " but "
                    + Stringutil::number2string(ret) + " exited.", errno);
            }
        }
    } else if (child == 0) { // child code, execute the process

        // stdin
        if (stdin_pipes[1]) {
            close(stdin_pipes[1]); // write end of stdin
            dup2(stdin_pipes[0], STDIN_FILENO); // read end of stdin
        } else if (m_stdinFd > 0) {
            close(STDIN_FILENO);
            dup2(m_stdinFd, STDIN_FILENO);
        }

        // stdout
        if (stdout_pipes[0]) {
            close(stdout_pipes[0]); // read end of stdout
            dup2(stdout_pipes[1], STDOUT_FILENO); // write end of stdout
        } else if (m_stdoutFd > 0) {
            close(STDOUT_FILENO);
            dup2(m_stdoutFd, STDOUT_FILENO);
        }

        // stderr
        if (stderr_pipes[0]) {
            close(stderr_pipes[0]); // read end of stderr
            dup2(stderr_pipes[1], STDERR_FILENO); // write end of stderr
        } else if (m_stderrFd > 0) {
            close(STDERR_FILENO);
            dup2(m_stderrFd, STDERR_FILENO);
        }

        executeProcess(name, args);

    } else { // error in fork()
        closeFds();
        throw KSystemError("fork() failed in Process::execute", errno);
    }

    closeFds();

    return WEXITSTATUS(status);
}

// -----------------------------------------------------------------------------
void Process::closeFds()
    throw ()
{
    Debug::debug()->trace("Process::closeFds()");

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
}

// -----------------------------------------------------------------------------
void Process::executeProcess(const string &name, const StringVector &args)
    throw (KError)
{
    StringVector fullV = args;
    fullV.insert(fullV.begin(), name);
    char **vector = Stringutil::stringv2charv(fullV);

    string s;
    for (StringVector::const_iterator it = args.begin();
            it != args.end(); ++it)
        s += *it + " ";

    Debug::debug()->dbg("Executing %s %s", name.c_str(), s.c_str());

    int ret = execvp(name.c_str(), vector);
    Util::freev(vector);

    if (ret < 0)
        throw KSystemError("Execution of '" + name + "' failed.", errno);
}

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:
