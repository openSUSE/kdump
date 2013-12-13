/*
 * (c) 2013, Petr Tesarik <ptesarik@suse.cz>, SUSE LINUX Products GmbH
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
#ifndef MULTIPATH_H
#define MULTIPATH_H

#include "subcommand.h"

//{{{ MultipathConf -----------------------------------------------------------

// This class implements a parser of multipath.conf
// It is bug-for-bug compatible with the parser from multipath-tools 0.4.9.
// Known quirks
// * Last line is ignored if it is not terminated by CR or LF.
// * White space at the beginning of a quoted string is swallowed.
// * If a quoted string starts with '!' or '#', it is actually treated as
//   a comment (so it is ignored and the rest of the line too).
// * Keywords which are recognized as group-openers don't need an opening
//   '{' character. However, if it is present, it must be on the same line.
// * Closing brace may be followed by any garbage

class MultipathConf {
    public:
        class Handler {
	    public:
	        /**
		 * Process a parsed input line.
		 *
		 * @param[in] raw    raw line data, including the end-of-line
		 *                   marker (CR or LF)
		 * @param[in] tokens parsed tokens (one string per token)
		 */
	        virtual void process(const std::string &raw,
				     StringVector &tokens)
		= 0;
        };

    public:
        MultipathConf(std::istream &input)
        throw ();

        /**
         * Process input with a given handler
         */
        void process(class Handler &handler);

    private:
        std::istream &m_input;
        std::string m_line;

        bool readline(void);
};

//{{{ Multipath ---------------------------------------------------------------

/**
 * Subcommand to modify multipath.conf
 */
class Multipath : public Subcommand {

    public:
        /**
         * Creates a new Multipath object.
         */
        Multipath()
        throw ();

    public:
        /**
         * Returns the name of the subcommand (multipath).
         */
        const char *getName() const
        throw ();

        /**
         * Parses the non-option arguments from the command line.
         */
        virtual void parseArgs(const StringVector &args)
        throw (KError);

        /**
         * That command does not need a config file.
         */
        bool needsConfigfile() const
        throw ();

        /**
         * Executes the function.
         *
         * @throw KError on any error. No exception indicates success.
         */
        void execute()
        throw (KError);

    private:
	/**
	 * Lines to be added to blacklist_exceptions
	 */
	StringVector m_exceptions;

	/**
	 * Parser of multipath.conf
	 */
	MultipathConf m_parser;
};

//}}}

#endif /* MULTIPATH_H */

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:
