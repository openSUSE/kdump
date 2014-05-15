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

//{{{ SubProcess ---------------------------------------------------------------

// -----------------------------------------------------------------------------
SubProcess::SubProcess()
    : m_pid(-1), m_killSignal(SIGKILL)
{}

// -----------------------------------------------------------------------------
SubProcess::~SubProcess()
{
    _closeParentFDs();
    if (m_pid != -1) {
	kill();
	wait();
    }
}

// -----------------------------------------------------------------------------
void SubProcess::checkSpawned(void)
    throw (KError)
{
    if (m_pid == -1)
	throw KError("SubProcess::checkSpawned(): no subprocess spawned");
}

// -----------------------------------------------------------------------------
void SubProcess::setPipeDirection(int fd, enum PipeDirection dir)
{
    if (dir == None)
	m_pipes.erase(fd);
    else {
	std::pair<int, struct PipeInfo> val;
	std::pair<std::map<int, struct PipeInfo>::iterator, bool> ret;
	val.first = fd;
	val.second.dir = dir;
	val.second.parentfd = -1;
	val.second.childfd = -1;
	ret = m_pipes.insert(val);
	if (ret.second == false)
	    ret.first->second.dir = dir;
    }
}

// -----------------------------------------------------------------------------
enum SubProcess::PipeDirection SubProcess::getPipeDirection(int fd)
{
    std::map<int, struct PipeInfo>::iterator ret;
    ret = m_pipes.find(fd);
    return (ret == m_pipes.end())
	? None
	: ret->second.dir;
}

// -----------------------------------------------------------------------------
int SubProcess::getPipeFD(int fd)
    throw (std::out_of_range)
{
    std::map<int, struct PipeInfo>::iterator ret;
    ret = m_pipes.find(fd);
    if (ret == m_pipes.end())
	throw std::out_of_range("SubProcess::getPipeFD(): Unknown fd "
				+ Stringutil::number2string(fd));
    return ret->second.parentfd;
}

// -----------------------------------------------------------------------------
void SubProcess::_closeParentFDs(void)
{
    std::map<int, struct PipeInfo>::iterator it;
    for (it = m_pipes.begin(); it != m_pipes.end(); ++it) {
	if (it->second.parentfd >= 0) {
	    close(it->second.parentfd);
	    it->second.parentfd = -1;
	}
    }
}

// -----------------------------------------------------------------------------
void SubProcess::_closeChildFDs(void)
{
    std::map<int, struct PipeInfo>::iterator it;
    for (it = m_pipes.begin(); it != m_pipes.end(); ++it) {
	if (it->second.childfd >= 0) {
	    close(it->second.childfd);
	    it->second.childfd = -1;
	}
    }
}

// -----------------------------------------------------------------------------
void SubProcess::spawn(const string &name, const StringVector &args)
{
    Debug::debug()->trace("SubProcess::spawn(%s, %s)",
        name.c_str(), Stringutil::vector2string(args, ":").c_str());

    //
    // setup pipes
    //
    try {
	std::map<int, struct PipeInfo>::iterator it;
	for (it = m_pipes.begin(); it != m_pipes.end(); ++it) {
	    int pipefd[2];

	    if (it->second.dir != ParentToChild &&
		it->second.dir != ChildToParent)
		throw KError("SubProcess::spawn(): "
			     "invalid pipe direction for fd "
			     + Stringutil::number2string(it->first) + ": "
			     + Stringutil::number2string(it->second.dir));

	    if (pipe(pipefd) < 0)
		throw KSystemError("SubProcess::spawn(): "
				   "cannot create pipe for fd "
				   + Stringutil::number2string(it->first),
				   errno);

	    if(it->second.dir == ParentToChild) {
		it->second.parentfd = pipefd[1];
		it->second.childfd = pipefd[0];
	    } else {		// ChildToParent
		it->second.parentfd = pipefd[0];
		it->second.childfd = pipefd[1];
	    }
	}
    } catch(...) {
	_closeParentFDs();
	_closeChildFDs();
	throw;
    }

    //
    // execute the child
    //

    pid_t child = fork();
    if (child > 0) {		// parent code
	m_pid = child;

	_closeChildFDs();

    } else {
	_closeParentFDs();

        if (child == 0) {	// child code
	    std::map<int, struct PipeInfo>::iterator it;
	    for (it = m_pipes.begin(); it != m_pipes.end(); ++it)
		dup2(it->second.childfd, it->first);
        }

	_closeChildFDs();

        if (child != 0)         // parent code failure
            throw KSystemError("fork() failed in ProcessFilter::execute", errno);
	// child code, execute the process
	StringVector fullV = args;
	fullV.insert(fullV.begin(), name);
	char **vector = Stringutil::stringv2charv(fullV);

	int ret = execvp(name.c_str(), vector);
	Util::freev(vector);

	if (ret < 0)
	    throw KSystemError("Execution of '" + name + "' failed.", errno);
    }
}

// -----------------------------------------------------------------------------
void SubProcess::kill(int sig)
    throw (KError)
{
    checkSpawned();

    if (::kill(m_pid, sig))
	throw KSystemError("SubProcess::kill(): cannot send signal"
			   + Stringutil::number2string(sig), errno);
}

// -----------------------------------------------------------------------------
int SubProcess::wait(void)
    throw (KError)
{
    checkSpawned();

    int status;
    pid_t ret = ::waitpid(m_pid, &status, 0);

    if (ret == -1)
	throw KSystemError("SubProcess::wait(): cannot get state of PID "
			   + Stringutil::number2string(m_pid), errno);

    if (ret != m_pid)
	throw KError("SubProcess::wait(): spawned PID "
		     + Stringutil::number2string(m_pid) + " but PID "
		     + Stringutil::number2string(ret) + " exited.");

    m_pid = -1;
    return status;
}

//}}}
//{{{ ProcessFilter ------------------------------------------------------------

// -----------------------------------------------------------------------------
ProcessFilter::ProcessFilter()
    throw ()
    : m_stdin(NULL), m_stdout(NULL), m_stderr(NULL)
{}

// -----------------------------------------------------------------------------
void ProcessFilter::setStdin(istream *stream)
    throw ()
{
    m_stdin = stream;
}

// -----------------------------------------------------------------------------
void ProcessFilter::setStdout(ostream *stream)
    throw ()
{
    m_stdout = stream;
}

// -----------------------------------------------------------------------------
void ProcessFilter::setStderr(ostream *stream)
    throw ()
{
    m_stderr = stream;
}

// -----------------------------------------------------------------------------
void ProcessFilter::spawn(const string &name, const StringVector &args)
{
    Debug::debug()->trace("ProcessFilter::spawn(%s, %s)",
        name.c_str(), Stringutil::vector2string(args, ":").c_str());

    //
    // setup pipes for stdin, stderr, stdout
    //
    int stdin_pipe[2];
    if (m_stdin) {
        if (pipe(stdin_pipe) < 0)
            throw KSystemError("Could not create stdin pipe.", errno);
        m_pipefds[0] = stdin_pipe[1];
    } else
	m_pipefds[0] = -1;

    int stdout_pipe[2];
    if (m_stdout) {
        if (pipe(stdout_pipe) < 0)
            throw KSystemError("Could not create stdout pipe.", errno);
        m_pipefds[1] = stdout_pipe[0];
    } else
	m_pipefds[1] = -1;

    int stderr_pipe[2];
    if (m_stderr) {
        if (pipe(stderr_pipe) < 0)
            throw KSystemError("Could not create stderr pipe.", errno);
	m_pipefds[2] = stderr_pipe[0];
    } else
	m_pipefds[2] = -1;

    //
    // execute the child
    //

    pid_t child = fork();
    if (child > 0) {		// parent code
	m_pid = child;

        // close pipe ends intended for the child
        if (m_stdin)
            close(stdin_pipe[0]);  // read end of stdin
        if (m_stdout)
            close(stdout_pipe[1]); // write end of stdout
        if (m_stderr)
            close(stderr_pipe[1]); // write end of stderr

    } else {
        if (child == 0) {	// child code
            if (m_stdin)
                dup2(stdin_pipe[0], STDIN_FILENO);   // read end of stdin
            if (m_stdout)
                dup2(stdout_pipe[1], STDOUT_FILENO); // write end of stdout
            if (m_stderr)
                dup2(stderr_pipe[1], STDERR_FILENO); // write end of stderr
        }

	// close no longer needed pipes
	if (m_stdin) {
	    close(stdin_pipe[0]);
	    close(stdin_pipe[1]);
	}
	if (m_stdout) {
	    close(stdout_pipe[0]);
	    close(stdout_pipe[1]);
	}
	if (m_stderr) {
	    close(stderr_pipe[0]);
	    close(stderr_pipe[1]);
	}

        if (child == 0)         // child code, execute the process
            executeProcess(name, args);
        else                    // parent code failure
            throw KSystemError("SubProcess::spawn(): fork failed", errno);
    }
}

// -----------------------------------------------------------------------------
uint8_t ProcessFilter::execute(const string &name, const StringVector &args)
    throw (KError)
{
    Debug::debug()->trace("ProcessFilter::execute(%s, %s)",
        name.c_str(), Stringutil::vector2string(args, ":").c_str());

    spawn(name, args);

    // initialize fds
    struct pollfd fds[3];
    int active_fds = 0;
    for (int i = 0; i < 3; ++i) {
	fds[i].fd = m_pipefds[i];
	if (fds[i].fd >= 0)
	    ++active_fds;
    }
    fds[0].events = POLLOUT;
    fds[1].events = fds[2].events = POLLIN;

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

    int status;
    pid_t ret = waitpid(m_pid, &status, 0);
    if (ret != m_pid) {
	throw KSystemError("ProcessFilter::execute() waits for "
			   + Stringutil::number2string(m_pid) + " but "
			   + Stringutil::number2string(ret) + " exited.",
			   errno);
    }
    m_pid = -1;

    return WEXITSTATUS(status);
}

// -----------------------------------------------------------------------------
void ProcessFilter::executeProcess(const string &name, const StringVector &args)
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

//}}}

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:
