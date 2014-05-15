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

//{{{ ProcessFilter ------------------------------------------------------------

/**
 * Simple to use API to process-based filters.
 */
class ProcessFilter {

    public:

        /**
         * Creates a new process filter.
         */
        ProcessFilter()
        throw ();

        /**
         * Desctructor.
         */
        virtual ~ProcessFilter () {}

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
         * Sets the stream which will be used as stdin for the process
         * executed in ProcessFilter::execute().
         *
         * @param[in] stream the stream to be used as input
         */
        void setStdin(std::istream *stream)
        throw ();

        /**
         * Sets the stream which will be used as stdout for the process
         * executed in ProcessFilter::executable().
         *
         * @param[in] stream the stream to be used as output
         */
        void setStdout(std::ostream *stream)
        throw ();

        /**
         * Sets the stream which will be used as stderr for the process
         * executed in ProcessFilter::executable().
         *
         * @param[in] stream the stream to be used as error output
         */
        void setStderr(std::ostream *stream)
        throw ();

    protected:
        void executeProcess(const std::string &name, const StringVector &args)
        throw (KError);

    private:
        std::istream *m_stdin;
        std::ostream *m_stdout;
        std::ostream *m_stderr;

	pid_t m_pid;
	int m_pipefds[3];
};

//}}}

#endif /* PROCESS_H */

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:
