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
#ifndef PROCESS_H
#define PROCESS_H

#include <stdint.h>
#include <map>
#include <memory>

#include "global.h"
#include "optionparser.h"
#include "subcommand.h"
#include "stringvector.h"
#include "multiplexio.h"

//{{{ SubProcessFD -------------------------------------------------------------

/**
 * Abstract base class for file descriptor setup.
 */
class SubProcessFD {

    public:

        virtual ~SubProcessFD();

        /**
         * Do all necessary preparations before forking.
         */
        virtual void prepare() = 0;

        /**
         * Finalize the file descriptor in parent.
         */
        virtual void finalizeParent() = 0;

        /**
         * Finalize the file descriptor in child.
         *
         * @param[in] fd file descriptor in child
         */
        virtual void finalizeChild(int fd) = 0;

	/**
	 * Get the file descriptor in the parent process.
	 *
	 * @return the file descriptor of a pipe, or -1 if none
	 */
        virtual int parentFD() = 0;

    protected:

        void _move_fd(int& oldfd, int newfd);
};

//}}}
//{{{ SubProcessRedirect -------------------------------------------------------

class SubProcessRedirect : public SubProcessFD {

        int m_fd;

    public:

        SubProcessRedirect(int fd)
            : m_fd(fd)
        { }

        void prepare();
        void finalizeParent();
        void finalizeChild(int fd);
        int parentFD();
};

//}}}
//{{{ ChildPipe ----------------------------------------------------------------

class SubProcessPipe : public SubProcessFD {

    protected:

        int m_pipefd[2];

    public:

        SubProcessPipe()
        { m_pipefd[0] = m_pipefd[1] = -1; }

        ~SubProcessPipe();
        void prepare();

        /**
         * Explicitly close the pipe.
         */
        void close();
};

//}}}
//{{{ ParentToChildPipe --------------------------------------------------------

/**
 * Pipe for data written from parent to child.
 */
class ParentToChildPipe : public SubProcessPipe {

    public:

        void finalizeParent();
        void finalizeChild(int fd);
        int parentFD();
};

//}}}
//{{{ ChildToParentPipe --------------------------------------------------------

/**
 * Pipe for data written from parent to child.
 */
class ChildToParentPipe : public SubProcessPipe {

    public:

        void finalizeParent();
        void finalizeChild(int fd);
        int parentFD();
};

//}}}
//{{{ SubProcess ---------------------------------------------------------------

/**
 * Representation of a subprocess in the parent.
 */
class SubProcess {

    public:

	enum PipeDirection {
		None = -1,
		ParentToChild,
		ChildToParent
	};

	/**
	 * Prepare a new subprocess.
	 */
	SubProcess();

	/**
	 * Destructor. Kills the subprocess if still running.
	 */
	virtual ~SubProcess();

	/**
	 * Set up a file descriptor in the child.
	 *
	 * @param[in] fd file descriptor in the child
	 * @param[in] setup file descriptor setup class
	 */
	void setChildFD(int fd, std::shared_ptr<SubProcessFD> setup);

	/**
	 * Get child file descriptor setup.
	 *
	 * @param[in] fd file descriptor in the child
	 * @return pointer to the setup class, or nullptr
	 */
	std::shared_ptr<SubProcessFD> getChildFD(int fd);

	/**
	 * Specify how a file descriptor should be redirected.
	 *
	 * @param[in] File descriptor in child.
	 * @param[in] Pipe direction.
	 *
	 * Note that pipes and redirections are mutually exclusive, so
	 * if you set up a pipe, any respective redirection for the same
	 * file descriptor will be removed.
	 */
	void setPipeDirection(int fd, enum PipeDirection dir);

	/**
	 * Check how a file descriptor should be redirected.
	 *
	 * @param[in] File descriptor in child.
	 */
	enum PipeDirection getPipeDirection(int fd);

	/**
	 * Get the file descriptor of the other end of a pipe redirected
	 * in the child.
	 *
	 * @param[in] File descriptor in child.
	 * @exception out_of_range if the file descriptor is not piped.
	 */
        int getPipeFD(int fd);

	/**
	 * Set up redirection from an open file descriptor.
	 *
	 * @param[in] fd File descriptor in child.
	 * @param[in] srcfd File descriptor in parent.
	 *            Use -1 to remove a redirection.
	 *
	 * The source file descriptor is not closed in child. You can
	 * use the O_CLOEXEC flag to control which file descriptors are
	 * preserved across exec.
	 *
	 * Note that pipes and redirections are mutually exclusive, so
	 * if you set a redirection, any respective pipe setup for the
	 * same file descriptor will be removed.
	 */
	void setRedirection(int fd, int srcfd);

	/**
	 * Spawns a subprocess.
	 *
         * @param[in] name the executable (PATH is searched) of the process
         *            to execute
         * @param[in] args the arguments for the process (argv[0] is used from
         *            @c name, so it cannot be overwritten here). Pass an empty
         *            list if you don't want to provide arguments.
	 */
	void spawn(const std::string &name, const StringVector &args);

	/**
	 * Get the child process ID.
	 *
	 * @returns system PID, or -1 if none.
	 */
	int getChildPID(void)
	{ return m_pid; }

	/**
	 * Set the default kill signal.
	 *
	 * @param[in] The signal to kill the child.
	 */
	void setKillSignal(int sig)
	{ m_killSignal = sig; }

	/**
	 * Get the default kill signal.
	 */
	int getKillSignal(void)
	{ return m_killSignal; }

	/**
	 * Send a signal to the child process.
	 *
	 * @param[in] The signal to be sent, see kill(2).
	 * @exception KError if kill(2) fails.
	 */
	void kill(int sig);

	/**
	 * Send the default kill signal to the child process.
	 */
	void kill(void)
	{ kill(m_killSignal); }

	/**
	 * Wait for the child to terminate.
	 *
	 * @return Child exit status, see wait(2).
	 */
	int wait(void);

    protected:

	/**
	 * Make sure that a child process has been spawned.
	 *
	 * @exception KError if there is no child process.
	 */
	void checkSpawned(void);

	pid_t m_pid;
	int m_killSignal;

    private:

	std::map<int, std::shared_ptr<SubProcessFD>> m_fdmap;
};

//}}}
//{{{ ProcessFilter ------------------------------------------------------------

/**
 * Simple to use API to process-based filters.
 */
class ProcessFilter {

    public:

	/**
	 * Abstract base class for handling input/output.
	 */
	class IO;

	/**
	 * Destructor.
	 */
	~ProcessFilter();

        /**
         * Runs the process, redirecting input/output.
         *
         * @param[in] name the executable (PATH is searched) of the process
         *            to execute
         * @param[in] args the arguments for the process (argv[0] is used from
         *            @c name, so it cannot be overwritten here). Pass an empty
         *            list if you don't want to provide arguments.
         * @return the numeric return value (between 0 and 255) of the process
         *         that has been executed
         */
        uint8_t execute(const std::string &name, const StringVector &args);

	/**
	 * Set Input/Output handling.
	 *
	 * @param[in] io pointer to a class ProcessFilter::IO instance;
	 *               the instance is deallocated by class ProcessFilter
	 *
	 * Note: The implementation uses io->getFD() to know which file
	 * descriptor is modified.
	 */
	void setIO(IO *io);

        /**
         * Redirect input in the subprocess from a std::istream.
         *
	 * @param[in] fd file descriptor in child
         * @param[in] stream the stream to be used as input
         */
        void setInput(int fd, std::istream *stream);

        /**
	 * Redirect output from the subprocess to a std::ostream.
         *
	 * @param[in] fd file descriptor in child
         * @param[in] stream the stream to be used as output
         */
        void setOutput(int fd, std::ostream *stream);

	/**
	 * setInput/setOutput shortcuts for stdin, stdout and stderr.
	 *
	 * @param[in] stream the stream to be used as input/output
	 */
	void setStdin(std::istream *stream);
	void setStdout(std::ostream *stream);
	void setStderr(std::ostream *stream);

    private:

	std::map<int, IO*> m_iomap;
};

//}}}
//{{{ ProcessFilter::IO --------------------------------------------------------

class ProcessFilter::IO {
    public:
	/**
	 * Prepare a new IO object.
	 *
	 * @param[in] fd desired file descriptor in the child.
	 */
	IO(int fd)
	: m_fd(fd)
	{ }

	/**
	 * Virtual destructor is needed to destroy virtual classes.
	 */
	virtual ~IO()
	{ }

	/**
	 * Get the associated file descriptor.
	 */
	int getFD(void) const
	{ return m_fd; }

	/**
	 * Prepare a SubProcess instance (before spawning a child).
	 */
	virtual void setupSubProcess(SubProcess &p) = 0;

	/**
	 * Set up I/O multiplexing.
	 *
	 * @param[in,out] io IO multiplexer instance
	 * @param[in,out] p SubProcess instance
	 *
	 * This method is called after the subprocess has already started,
	 * so you can get pipe file descriptors, etc.
	 */
	virtual void setupIO(MultiplexIO &io, SubProcess &p) = 0;

	/**
	 * Handle I/O events.
	 *
	 * @param[in,out] io IO multiplexer instance
	 * @param[in,out] p SubProcess instance
	 */
	virtual void handleEvents(MultiplexIO &io, SubProcess &p) = 0;

    protected:
	/**
	 * Desired file descriptor in the child.
	 */
	int m_fd;
};

//}}}

#endif /* PROCESS_H */

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:
