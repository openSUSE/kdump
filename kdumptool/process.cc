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
#include <cerrno>
#include <unistd.h>
#include <poll.h>

#include "process.h"
#include "global.h"
#include "stringutil.h"
#include "util.h"
#include "debug.h"

using std::string;
using std::istream;
using std::ostream;
using std::min;
using std::max;

// -----------------------------------------------------------------------------
Process::Process()
    throw ()
    : m_stdin(NULL), m_stdout(NULL), m_stderr(NULL)
{}

// -----------------------------------------------------------------------------
void Process::setStdin(istream *stream)
    throw ()
{
    m_stdin = stream;
}

// -----------------------------------------------------------------------------
void Process::setStdout(ostream *stream)
    throw ()
{
    m_stdout = stream;
}

// -----------------------------------------------------------------------------
void Process::setStderr(ostream *stream)
    throw ()
{
    m_stderr = stream;
}

// -----------------------------------------------------------------------------
uint8_t Process::execute(const string &name, const StringVector &args)
    throw (KError)
{
    pid_t child;
    int status = 0;

    Debug::debug()->trace("Process::execute(%s, %s)",
        name.c_str(), Stringutil::vector2string(args, ":").c_str());

    struct pollfd fds[3];
    int active_fds = 0;

    //
    // setup pipes for stdin, stderr, stdout
    //
    int stdin_pipe[2];
    if (m_stdin) {
        if (pipe(stdin_pipe) < 0)
            throw KSystemError("Could not create stdin pipe.", errno);
        fds[0].fd = stdin_pipe[1];
        fds[0].events = POLLOUT;
        ++active_fds;
    } else
        stdin_pipe[0] = stdin_pipe[1] = fds[0].fd = -1;

    int stdout_pipe[2];
    if (m_stdout) {
        if (pipe(stdout_pipe) < 0)
            throw KSystemError("Could not create stdout pipe.", errno);
        fds[1].fd = stdout_pipe[0];
        fds[1].events = POLLIN;
        ++active_fds;
    } else
        stdin_pipe[0] = stdin_pipe[1] = fds[1].fd = -1;

    int stderr_pipe[2];
    if (m_stderr) {
        if (pipe(stderr_pipe) < 0)
            throw KSystemError("Could not create stderr pipe.", errno);
        fds[2].fd = stderr_pipe[0];
        fds[2].events = POLLIN;
        ++active_fds;
    } else
        stderr_pipe[0] = stderr_pipe[1] = fds[2].fd = -1;

    //
    // execute the child
    //

    child = fork();
    if (child > 0) { // parent code, just wait until the child has exited
        int ret;

        // close pipe ends intended for the child
        if (stdin_pipe[0] >= 0)
            close(stdin_pipe[0]);  // read end of stdin
        if (stdout_pipe[1] >= 0)
            close(stdout_pipe[1]); // write end of stdout
        if (stderr_pipe[1] >= 0)
            close(stderr_pipe[1]); // write end of stderr

        char inbuf[BUFSIZ], *inbufptr = NULL, *inbufend = NULL;
        char outbuf[BUFSIZ];
        ssize_t cnt;

        while (active_fds > 0) {
            int retval = poll(fds, 3, -1);

            if (retval < 0 && errno != EINTR)
                    throw KSystemError("select() failed", errno);

            // Handle stdin
            if (fds[0].revents & POLLOUT) {
                // Buffer underflow
                if (inbufptr >= inbufend) {
                    m_stdin->clear();
                    m_stdin->read(inbufptr = inbuf, BUFSIZ);
                    if (m_stdin->bad())
                        throw KSystemError("Cannot get data for stdin pipe",
                                           errno);
                    inbufend = inbufptr + m_stdin->gcount();
                }
                cnt = write(fds[0].fd, inbufptr, inbufend - inbufptr);
                if (cnt < 0)
                    throw KSystemError("Cannot send data to stdin pipe",
                                       errno);
                inbufptr += cnt;

                if (m_stdin->eof()) {
                    close(fds[0].fd);
                    fds[0].fd = -1;
                    --active_fds;
                }
            }

            // Handle stdout
            if (fds[1].revents & (POLLIN | POLLHUP)) {
                if (fds[1].revents & POLLIN) {
                    cnt = read(fds[1].fd, outbuf, sizeof outbuf);
                    if (cnt < 0)
                        throw KSystemError("Cannot get data from stdout pipe",
                                           errno);
                } else
                    cnt = 0;

                if (!cnt) {
                    close(fds[1].fd);
                    fds[1].fd = -1;
                    --active_fds;
                }
                m_stdout->write(outbuf, cnt);
            }

            // Handle stderr
            if (fds[2].revents & (POLLIN | POLLHUP)) {
                if (fds[2].revents & POLLIN) {
                    cnt = read(fds[2].fd, outbuf, sizeof outbuf);
                    if (cnt < 0)
                        throw KSystemError("Cannot get data from stderr pipe",
                                           errno);
                } else
                    cnt = 0;

                if (!cnt) {
                    close(fds[2].fd);
                    fds[2].fd = -1;
                    --active_fds;
                }
                m_stderr->write(outbuf, cnt);
            }
        }

        ret = waitpid(child, &status, 0);
        if (ret != child) {
            throw KSystemError("Process::execute() waits for "
                + Stringutil::number2string(child) + " but "
                + Stringutil::number2string(ret) + " exited.", errno);
        }

        // close pipe ends in the parent
        for (int i = 0; i < 3; ++i)
            if (fds[i].fd >= 0)
                close(fds[i].fd);

    } else { // child code or failure

        // setup stdin, stdout, stderr if executing child
        if (child == 0) {
            dup2(stdin_pipe[0], STDIN_FILENO);   // read end of stdin
            dup2(stdout_pipe[1], STDOUT_FILENO); // write end of stdout
            dup2(stderr_pipe[1], STDERR_FILENO); // write end of stderr
        }

        // close no longer needed pipes
        close(stdin_pipe[0]);
        close(stdin_pipe[1]);
        close(stdout_pipe[0]);
        close(stdout_pipe[1]);
        close(stderr_pipe[0]);
        close(stderr_pipe[1]);

        if (child == 0)         // child code, execute the process
            executeProcess(name, args);
        else                    // parent code failure
            throw KSystemError("fork() failed in Process::execute", errno);
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
