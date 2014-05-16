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

#include "global.h"
#include "optionparser.h"
#include "subcommand.h"

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
	int getPipeFD(int fd)
	throw (std::out_of_range);

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
	throw ()
	{ return m_pid; }

	/**
	 * Set the default kill signal.
	 *
	 * @param[in] The signal to kill the child.
	 */
	void setKillSignal(int sig)
	throw ()
	{ m_killSignal = sig; }

	/**
	 * Get the default kill signal.
	 */
	int getKillSignal(void)
	throw ()
	{ return m_killSignal; }

	/**
	 * Send a signal to the child process.
	 *
	 * @param[in] The signal to be sent, see kill(2).
	 * @exception KError if kill(2) fails.
	 */
	void kill(int sig)
	throw (KError);

	/**
	 * Send the default kill signal to the child process.
	 */
	void kill(void)
	throw(KError)
	{ kill(m_killSignal); }

	/**
	 * Wait for the child to terminate.
	 *
	 * @return Child exit status, see wait(2).
	 */
	int wait(void)
	throw(KError);

    protected:

	/**
	 * Make sure that a child process has been spawned.
	 *
	 * @exception KError if there is no child process.
	 */
	void checkSpawned(void)
	throw(KError);

	pid_t m_pid;
	int m_killSignal;

    private:

	struct PipeInfo {
	    enum PipeDirection dir;
	    int parentfd;	/* fd in parent */
	    int childfd;	/* fd for child (during init) */

	    /**
	     * Initialize the info
	     */
	    PipeInfo(enum PipeDirection adir)
	    throw ()
	    : dir(adir), parentfd(-1), childfd(-1)
	    { }

	    /**
	     * Destructor.
	     */
	    ~PipeInfo()
	    throw ()
	    { close(); }

	    /**
	     * Close all open file descriptors
	     */
	    void close(void)
	    throw ();

	    /**
	     * Close parent file descriptor, if open
	     */
	    void closeParent(void)
	    throw ();

	    /**
	     * Close child file descriptor, if open
	     */
	    void closeChild(void)
	    throw ();
	};
	std::map<int, struct PipeInfo> m_pipes;

	std::map<int, int> m_redirs;

	void _closeParentFDs(void);
	void _closeChildFDs(void);
};

//}}}
//{{{ MultiplexIO --------------------------------------------------------------

struct pollfd;

class MultiplexIO {
    public:
	/**
	 * Trivial constructor.
	 */
	MultiplexIO(void)
	throw ();

	/**
	 * Add a file descriptor to monitor.
	 *
	 * @param[in] fd file descriptor
	 * @param[in] events to be watched, see poll(2)
	 * @returns corresponding index in the poll vector
	 */
	int add(int fd, short events);

	/**
	 * Get a reference to a pollfd.
	 */
	struct pollfd &operator[](int idx)
	throw(std::out_of_range);

	/**
	 * Remove a monitored file descriptor.
	 *
	 * @param[in] idx index in the poll vector
	 */
	void deactivate(int idx);

	/**
	 * Get the number of active file descriptors.
	 */
	int active() const
	throw ()
	{ return m_active; }

	/**
	 * Wait for a monitored event.
	 *
	 * @param[in] timeout max time to wait (in milliseconds)
	 * @returns the number of events
	 */
	int monitor(int timeout = -1)
	throw (KSystemError);

    private:
	std::vector<struct pollfd> m_fds;
	int m_active;
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
	~ProcessFilter()
	throw ();

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
        uint8_t execute(const std::string &name, const StringVector &args)
        throw (KError);

	/**
	 * Set Input/Output handling for the given fd in the subprocess.
	 *
	 * @param[in] fd file descriptor in child
	 * @param[in] io pointer to a class ProcessFilter::IO instance;
	 *               the instance is deallocated by class ProcessFilter
	 */
	void setIO(int fd, IO *io)
	{ m_iomap[fd] = io; }

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
	 * Virtual destructor is needed to destroy virtual classes.
	 */
	virtual ~IO()
	throw ()
	{ }

	/**
	 * Get the pipe direction in the child.
	 */
	virtual SubProcess::PipeDirection pipeDirection() const
	throw () = 0;

	/**
	 * Set up I/O multiplexing.
	 *
	 * @param[in,out] io IO multiplexer instance
	 * @param[in]     fd file descriptor of the parent end of the pipe
	 */
	virtual void setupIO(MultiplexIO &io, int fd) = 0;

	/**
	 * Handle I/O events.
	 *
	 * @param[in,out] io IO multiplexer instance
	 */
	virtual bool handleEvents(MultiplexIO &io) = 0;
};

//}}}

#endif /* PROCESS_H */

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:
