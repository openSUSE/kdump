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

#include "global.h"
#include "optionparser.h"
#include "subcommand.h"

//{{{ Process ------------------------------------------------------------------

/**
 * Simple to use process API.
 */
class Process {

    public:

        /**
         * Creates a new process.
         */
        Process()
        throw ();

        /**
         * Desctructor.
         */
        virtual ~Process () {}

        /**
         * Executes a process.
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
         * Sets a file name for stdin which will be used for the process
         * executed in Process::execute(). Calling Process::setStdinFd()
         * overwrites Process::setStdinFile() and vice versa.
         *
         * @param[in] filename the filename of the file. This file will be
         *            opened readonly.
         * @throw KError if opening the specified @p filename failed
         */
        void setStdinFile(const char *filename)
        throw (KError);

        /**
         * Sets a file name for stdout which will be used for the process
         * executed in Process::executable(). Calling Process::setStdoutFd()
         * overwrites Process::setStdoutFile() and vice versa.
         *
         * @param[in] filename the filename of the file. This file will be
         *            opened writeonly.
         * @throw KError if opening the specified @p filename failed
         */
        void setStdoutFile(const char *filename)
        throw (KError);

        /**
         * Sets a file name for stderr which will be used for the process
         * executed in Process::executable(). Calling Process::setStderrFd()
         * overwrites Process::setStderrFile() and vice versa.
         *
         * @param[in] filename the filename of the file. This file will be
         *            opened writeonly.
         * @throw KError if opening the specified @p filename failed
         */
        void setStderrFile(const char *filename)
        throw (KError);

        /**
         * Sets the buffer where the bytes from stdin are read from to
         * @p buffer. This function overwrites the Process::setStdinFd()
         * and Process::setStdinFile().
         *
         * @param[in] buffer a byte vector
         */
        void setStdinBuffer(ByteVector *buffer)
        throw ();

        /**
         * Sets the buffer where the bytes from stdout are written to.
         * This function overwrites the Process::setStdoutFd() and
         * Process::setStdoutFile().
         *
         * @param[in] buffer a byte vector
         */
        void setStdoutBuffer(ByteVector *buffer)
        throw ();

        /**
         * Sets the buffer where the bytes from stdout are written to.
         * This function overwrites the Process::setStderrFd() and
         * Process::setStderrFile().
         *
         * @param[in] buffer a byte vector
         */
        void setStderrBuffer(ByteVector *buffer)
        throw ();

    protected:
        void executeProcess(const std::string &name, const StringVector &args)
        throw (KError);

        void closeFds()
        throw ();

    private:
        int m_stdinFd;
        int m_stdoutFd;
        int m_stderrFd;
        bool m_needCloseStdin;
        bool m_needCloseStdout;
        bool m_needCloseStderr;
        ByteVector *m_stdinBuffer;
        ByteVector *m_stdoutBuffer;
        ByteVector *m_stderrBuffer;
};

//}}}

#endif /* PROCESS_H */

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:
