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
#include <typeinfo>

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
using std::make_shared;
using std::shared_ptr;

//{{{ SubProcess ---------------------------------------------------------------

// -----------------------------------------------------------------------------
SubProcess::SubProcess()
    : m_pid(-1), m_killSignal(SIGKILL)
{}

// -----------------------------------------------------------------------------
SubProcess::~SubProcess()
{
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
void SubProcess::setChildFD(int fd, shared_ptr<SubProcessFD> setup)
{
    m_fdmap.erase(fd);
    if (setup)
        m_fdmap.emplace(fd, setup);
}

// -----------------------------------------------------------------------------
shared_ptr<SubProcessFD> SubProcess::getChildFD(int fd)
{
    auto it = m_fdmap.find(fd);
    return it != m_fdmap.end()
        ? it->second
        : shared_ptr<SubProcessFD>(nullptr);
}

// -----------------------------------------------------------------------------
void SubProcess::setPipeDirection(int fd, enum PipeDirection dir)
{
    if (dir == ParentToChild)
        setChildFD(fd, make_shared<ParentToChildPipe>());
    else if (dir == ChildToParent)
        setChildFD(fd, make_shared<ChildToParentPipe>());
    else
        setChildFD(fd, nullptr);
}

// -----------------------------------------------------------------------------
enum SubProcess::PipeDirection SubProcess::getPipeDirection(int fd)
{
    enum SubProcess::PipeDirection ret = None;
    auto ptr = getChildFD(fd);
    if (ptr) {
        if (typeid(*ptr) == typeid(ParentToChildPipe))
            ret = ParentToChild;
        else if (typeid(*ptr) == typeid(ChildToParentPipe))
            ret = ChildToParent;
    }
    return ret;
}

// -----------------------------------------------------------------------------
int SubProcess::getPipeFD(int fd)
{
    auto it = m_fdmap.find(fd);
    if (it == m_fdmap.end())
	throw std::out_of_range("SubProcess::getPipeFD(): Unknown fd "
				+ StringUtil::number2string(fd));
    return it->second->parentFD();
}

// -----------------------------------------------------------------------------
void SubProcess::setRedirection(int fd, int srcfd)
{
    if (srcfd >= 0)
        setChildFD(fd, make_shared<SubProcessRedirect>(srcfd));
    else
        setChildFD(fd, nullptr);
}

// -----------------------------------------------------------------------------
void SubProcess::spawn(const string &name, const StringVector &args)
{
    Debug::debug()->trace("SubProcess::spawn(%s, %s)",
        name.c_str(), args.join(':').c_str());

    //
    // setup pipes
    //
    for (auto &elem : m_fdmap)
        elem.second->prepare();

    //
    // execute the child
    //

    pid_t child = fork();
    if (child > 0) {		// parent code
	m_pid = child;

        for (auto &elem : m_fdmap)
            elem.second->finalizeParent();

    } else if (child == 0) {	// child code
        for (auto &elem : m_fdmap)
            elem.second->finalizeChild(elem.first);

    	// execute the process
	CharV fullV = args;
	fullV.insert(fullV.begin(), name);
	char **vector = fullV.data();

	int ret = execvp(name.c_str(), vector);
	if (ret < 0)
	    throw KSystemError("Execution of '" + name + "' failed.", errno);

    } else {                    // parent code failure
        throw KSystemError("SubProcess::spawn(): fork failed", errno);
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
//{{{ SubProcessFD -------------------------------------------------------------

// -----------------------------------------------------------------------------
SubProcessFD::~SubProcessFD()
{
}

// -----------------------------------------------------------------------------
void SubProcessFD::_move_fd(int& oldfd, int newfd)
{
    if (dup2(oldfd, newfd) < 0)
        throw KSystemError("Cannot duplicate fd "
                           + StringUtil::number2string(oldfd)
                           + " to fd " + StringUtil::number2string(newfd),
                           errno);
    close(oldfd);
    oldfd = -1;
}

//}}}
//{{{ SubProcessRedirect -------------------------------------------------------

// -----------------------------------------------------------------------------
void SubProcessRedirect::prepare()
{
}

// -----------------------------------------------------------------------------
void SubProcessRedirect::finalizeParent()
{
}

// -----------------------------------------------------------------------------
void SubProcessRedirect::finalizeChild(int fd)
{
    _move_fd(m_fd, fd);
}

// -----------------------------------------------------------------------------
int SubProcessRedirect::parentFD()
{
    return -1;
}

//}}}
//{{{ SubProcessPipe -----------------------------------------------------------

// -----------------------------------------------------------------------------
SubProcessPipe::~SubProcessPipe()
{
    close();
}

// -----------------------------------------------------------------------------
void SubProcessPipe::prepare()
{
    close();
    if (pipe2(m_pipefd, O_CLOEXEC) < 0)
        throw KSystemError("Cannot create subprocess pipe", errno);
}

// -----------------------------------------------------------------------------
void SubProcessPipe::close()
{
    if (m_pipefd[0] >= 0)
        ::close(m_pipefd[0]);
    if (m_pipefd[1] >= 0)
        ::close(m_pipefd[1]);
    m_pipefd[0] = m_pipefd[1] = -1;
}

//}}}
//{{{ ParentToChildPipe --------------------------------------------------------

// -----------------------------------------------------------------------------
void ParentToChildPipe::finalizeParent()
{
    ::close(m_pipefd[0]);
    m_pipefd[0] = -1;
}

// -----------------------------------------------------------------------------
void ParentToChildPipe::finalizeChild(int fd)
{
    ::close(m_pipefd[1]);
    m_pipefd[1] = -1;
    _move_fd(m_pipefd[0], fd);
}

// -----------------------------------------------------------------------------
int ParentToChildPipe::parentFD()
{
    return m_pipefd[1];
}

//}}}
//{{{ ChildToParentPipe --------------------------------------------------------

// -----------------------------------------------------------------------------
void ChildToParentPipe::finalizeParent()
{
    ::close(m_pipefd[1]);
    m_pipefd[1] = -1;
}

// -----------------------------------------------------------------------------
void ChildToParentPipe::finalizeChild(int fd)
{
    ::close(m_pipefd[0]);
    m_pipefd[0] = -1;
    _move_fd(m_pipefd[1], fd);
}

// -----------------------------------------------------------------------------
int ChildToParentPipe::parentFD()
{
    return m_pipefd[0];
}

//}}}
//{{{ Input --------------------------------------------------------------------
class Input : public ProcessFilter::IO {
    public:

	Input(int fd)
            : ProcessFilter::IO(fd, make_shared<ParentToChildPipe>())
	{ }
};

//}}}
//{{{ IStream ------------------------------------------------------------------
class IStream : public Input {
    public:
	IStream(int fd, std::istream *input)
	: Input(fd), m_input(input), bufptr(NULL), bufend(NULL)
	{ }

        virtual void setupIO(MultiplexIO &io);
        virtual void handleEvents(MultiplexIO &io);

    private:
	std::istream *m_input;
	int pollidx;
	char buf[BUFSIZ], *bufptr, *bufend;
};

// -----------------------------------------------------------------------------
void IStream::setupIO(MultiplexIO &io)
{
    pollidx = io.add(m_pipe->parentFD(), POLLOUT);
}

// -----------------------------------------------------------------------------
void IStream::handleEvents(MultiplexIO &io)
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
            m_pipe->close();
	    io.deactivate(pollidx);
	}
    }
}

//}}}
//{{{ Output -------------------------------------------------------------------

class Output : public ProcessFilter::IO {
    public:
	Output(int fd)
            : ProcessFilter::IO(fd, make_shared<ChildToParentPipe>())
	{ }
};

//}}}
//{{{ OStream ------------------------------------------------------------------

class OStream : public Output {
    public:
	OStream(int fd, std::ostream *output)
	: Output(fd), m_output(output)
	{ }

        virtual void setupIO(MultiplexIO &io);
        virtual void handleEvents(MultiplexIO &io);

    private:
	std::ostream *m_output;
	int pollidx;
	char buf[BUFSIZ];
};

// -----------------------------------------------------------------------------
void OStream::setupIO(MultiplexIO &io)
{
    pollidx = io.add(m_pipe->parentFD(), POLLIN);
}

// -----------------------------------------------------------------------------
void OStream::handleEvents(MultiplexIO &io)
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
            m_pipe->close();
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
        it->second->setupIO(io);

    while (io.active() > 0) {
	io.monitor();
	for (it = m_iomap.begin(); it != m_iomap.end(); ++it)
            it->second->handleEvents(io);
    }

    int status = p.wait();
    return WEXITSTATUS(status);
}

//}}}

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:
