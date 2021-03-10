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
#include "charv.h"
#include "util.h"
#include "debug.h"

using std::string;
using std::istream;
using std::ostream;
using std::min;
using std::max;

//{{{ SubProcess ---------------------------------------------------------------

// -----------------------------------------------------------------------------
void SubProcess::PipeInfo::close(void)
{
    closeParent();
    closeChild();
}

// -----------------------------------------------------------------------------
void SubProcess::PipeInfo::closeParent(void)
{
    if (parentfd >= 0) {
	::close(parentfd);
	parentfd = -1;
    }
}

// -----------------------------------------------------------------------------
void SubProcess::PipeInfo::closeChild(void)
{
    if (childfd >= 0) {
	::close(childfd);
	childfd = -1;
    }
}

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
{
    if (m_pid == -1)
	throw KError("SubProcess::checkSpawned(): no subprocess spawned");
}

// -----------------------------------------------------------------------------
void SubProcess::setPipeDirection(int fd, enum PipeDirection dir)
{
    m_redirs.erase(fd);

    if (dir == None)
	m_pipes.erase(fd);
    else {
	std::pair<int, struct PipeInfo> val(fd, PipeInfo(dir));
	std::pair<std::map<int, struct PipeInfo>::iterator, bool> ret;
	ret = m_pipes.insert(val);
	if (ret.second == false) {
	    ret.first->second.close();
	    ret.first->second.dir = dir;
	}
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
{
    std::map<int, struct PipeInfo>::iterator ret;
    ret = m_pipes.find(fd);
    if (ret == m_pipes.end())
	throw std::out_of_range("SubProcess::getPipeFD(): Unknown fd "
				+ StringUtil::number2string(fd));
    return ret->second.parentfd;
}

// -----------------------------------------------------------------------------
void SubProcess::setRedirection(int fd, int srcfd)
{
    m_pipes.erase(fd);

    if (srcfd < 0)
	m_redirs.erase(fd);
    else
	m_redirs[fd] = srcfd;
}

// -----------------------------------------------------------------------------
void SubProcess::_closeParentFDs(void)
{
    std::map<int, struct PipeInfo>::iterator it;
    for (it = m_pipes.begin(); it != m_pipes.end(); ++it)
	it->second.closeParent();
}

// -----------------------------------------------------------------------------
void SubProcess::_closeChildFDs(void)
{
    std::map<int, struct PipeInfo>::iterator it;
    for (it = m_pipes.begin(); it != m_pipes.end(); ++it)
	it->second.closeChild();
}

// -----------------------------------------------------------------------------
void SubProcess::spawn(const string &name, const StringVector &args)
{
    Debug::debug()->trace("SubProcess::spawn(%s, %s)",
        name.c_str(), args.join(':').c_str());

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
			     + StringUtil::number2string(it->first) + ": "
			     + StringUtil::number2string(it->second.dir));

	    if (pipe2(pipefd, O_CLOEXEC) < 0)
		throw KSystemError("SubProcess::spawn(): "
				   "cannot create pipe for fd "
				   + StringUtil::number2string(it->first),
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

	    std::map<int, int>::iterator redir;
	    for (redir = m_redirs.begin(); redir != m_redirs.end(); ++redir)
		dup2(redir->second, redir->first);
        }

        if (child != 0) {	// parent code failure
	    _closeChildFDs();
            throw KSystemError("SubProcess::spawn(): fork failed", errno);
	}

	// child code, execute the process
	CharV fullV = args;
	fullV.insert(fullV.begin(), name);
	char **vector = fullV.data();

	int ret = execvp(name.c_str(), vector);
	if (ret < 0)
	    throw KSystemError("Execution of '" + name + "' failed.", errno);
    }

    Debug::debug()->dbg("Forked child PID %d", m_pid);
}

// -----------------------------------------------------------------------------
void SubProcess::kill(int sig)
{
    Debug::debug()->trace("SubProcess::kill(%d)", sig);

    checkSpawned();

    if (::kill(m_pid, sig))
	throw KSystemError("SubProcess::kill(): cannot send signal"
			   + StringUtil::number2string(sig), errno);
}

// -----------------------------------------------------------------------------
int SubProcess::wait(void)
{
    Debug::debug()->trace("SubProcess::wait() on %d", m_pid);

    checkSpawned();

    int status;
    pid_t ret = ::waitpid(m_pid, &status, 0);

    if (ret == -1)
	throw KSystemError("SubProcess::wait(): cannot get state of PID "
			   + StringUtil::number2string(m_pid), errno);

    if (ret != m_pid)
	throw KError("SubProcess::wait(): spawned PID "
		     + StringUtil::number2string(m_pid) + " but PID "
		     + StringUtil::number2string(ret) + " exited.");

    Debug::debug()->dbg("PID %d exited with status 0x%04x", m_pid, status);

    m_pid = -1;
    return status;
}

//}}}
//{{{ Input --------------------------------------------------------------------
class Input : public ProcessFilter::IO {
    public:

	Input(int fd)
	: ProcessFilter::IO(fd)
	{ }

	virtual void setupSubProcess(SubProcess &p);
};

// -----------------------------------------------------------------------------
void Input::setupSubProcess(SubProcess &p)
{
    p.setPipeDirection(m_fd, SubProcess::ParentToChild);
}

//}}}
//{{{ IStream ------------------------------------------------------------------
class IStream : public Input {
    public:
	IStream(int fd, std::istream *input)
	: Input(fd), m_input(input), bufptr(NULL), bufend(NULL)
	{ }

	virtual void setupIO(MultiplexIO &io, SubProcess &p);
	virtual void handleEvents(MultiplexIO &io, SubProcess &p);

    private:
	std::istream *m_input;
	int pollidx;
	char buf[BUFSIZ], *bufptr, *bufend;
};

// -----------------------------------------------------------------------------
void IStream::setupIO(MultiplexIO &io, SubProcess &p)
{
    pollidx = io.add(p.getPipeFD(m_fd), POLLOUT);
}

// -----------------------------------------------------------------------------
void IStream::handleEvents(MultiplexIO &io, SubProcess &p)
{
    ssize_t cnt;
    const struct pollfd *poll = &io[pollidx];

    if (poll->revents & POLLOUT) {
	// Buffer underflow
	if (bufptr >= bufend) {
	    m_input->clear();
	    m_input->read(bufptr = buf, sizeof buf);
	    if (m_input->bad())
		throw KSystemError("Cannot get data for input pipe", errno);
	    bufend = bufptr + m_input->gcount();
	}
	cnt = write(poll->fd, bufptr, bufend - bufptr);
	if (cnt < 0)
	    throw KSystemError("Cannot send data to input pipe", errno);
	bufptr += cnt;

	if (m_input->eof()) {
	    p.setPipeDirection(m_fd, SubProcess::None);
	    io.deactivate(pollidx);
	}
    }
}

//}}}
//{{{ Output -------------------------------------------------------------------

class Output : public ProcessFilter::IO {
    public:
	Output(int fd)
	: ProcessFilter::IO(fd)
	{ }

	virtual void setupSubProcess(SubProcess &p);
};

// -----------------------------------------------------------------------------
void Output::setupSubProcess(SubProcess &p)
{
    p.setPipeDirection(m_fd, SubProcess::ChildToParent);
}

//}}}
//{{{ OStream ------------------------------------------------------------------

class OStream : public Output {
    public:
	OStream(int fd, std::ostream *output)
	: Output(fd), m_output(output)
	{ }

	virtual void setupIO(MultiplexIO &io, SubProcess &p);
	virtual void handleEvents(MultiplexIO &io, SubProcess &p);

    private:
	std::ostream *m_output;
	int pollidx;
	char buf[BUFSIZ];
};

// -----------------------------------------------------------------------------
void OStream::setupIO(MultiplexIO &io, SubProcess &p)
{
    pollidx = io.add(p.getPipeFD(m_fd), POLLIN);
}

// -----------------------------------------------------------------------------
void OStream::handleEvents(MultiplexIO &io, SubProcess &p)
{
    ssize_t cnt;
    const struct pollfd *poll = &io[pollidx];

    if (poll->revents & (POLLIN | POLLHUP)) {
	if (poll->revents & POLLIN) {
	    cnt = read(poll->fd, buf, sizeof buf);
	    if (cnt < 0)
		throw KSystemError("Cannot get data from output pipe", errno);
	} else
	    cnt = 0;

	if (!cnt) {
	    p.setPipeDirection(m_fd, SubProcess::None);
	    io.deactivate(pollidx);
	}
	m_output->write(buf, cnt);
    }
}

//}}}
//{{{ ProcessFilter ------------------------------------------------------------

// -----------------------------------------------------------------------------
ProcessFilter::~ProcessFilter()
{
    std::map<int, IO*>::const_iterator it;
    for (it = m_iomap.begin(); it != m_iomap.end(); ++it)
	delete it->second;
}

void ProcessFilter::setIO(IO *io)
{
    std::pair<std::map<int, IO*>::iterator, bool> ret;
    ret = m_iomap.insert(std::make_pair(io->getFD(), io));
    if (ret.second == false) {
	delete ret.first->second;
	ret.first->second = io;
    }
}

// -----------------------------------------------------------------------------
void ProcessFilter::setInput(int fd, istream *stream)
{
    setIO(new IStream(fd, stream));
}

// -----------------------------------------------------------------------------
void ProcessFilter::setOutput(int fd, ostream *stream)
{
    setIO(new OStream(fd, stream));
}

// -----------------------------------------------------------------------------
void ProcessFilter::setStdin(std::istream *stream)
{
    setInput(STDIN_FILENO, stream);
}

// -----------------------------------------------------------------------------
void ProcessFilter::setStdout(std::ostream *stream)
{
    setOutput(STDOUT_FILENO, stream);
}

// -----------------------------------------------------------------------------
void ProcessFilter::setStderr(std::ostream *stream)
{
    setOutput(STDERR_FILENO, stream);
}

// -----------------------------------------------------------------------------
uint8_t ProcessFilter::execute(const string &name, const StringVector &args)
{
    Debug::debug()->trace("ProcessFilter::execute(%s, %s)",
        name.c_str(), args.join(':').c_str());

    std::map<int, IO*>::const_iterator it;

    SubProcess p;
    for (it = m_iomap.begin(); it != m_iomap.end(); ++it)
	it->second->setupSubProcess(p);
    p.spawn(name, args);

    // initialize multiplex IO
    MultiplexIO io;
    for (it = m_iomap.begin(); it != m_iomap.end(); ++it)
	it->second->setupIO(io, p);

    while (io.active() > 0) {
	io.monitor();
	for (it = m_iomap.begin(); it != m_iomap.end(); ++it)
	    it->second->handleEvents(io, p);
    }

    int status = p.wait();
    return WEXITSTATUS(status);
}

//}}}

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:
