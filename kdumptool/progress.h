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
#ifndef PROGRESS_H
#define PROGRESS_H

#include <iostream>
#include <ctime>

#include "global.h"

//{{{ Terminal -----------------------------------------------------------------

/**
 * Retrieve some information about the terminal.
 */
class Terminal {

    public:
        /**
	 * Initialize a new Terminal object for standard output.
	 */
	Terminal (void)
	throw ();

    public:

	/**
	 * Get the terminal width.
	 */
	int width(void) const
	throw ()
	{ return m_width; }

	/**
	 * Get the terminal height.
	 */
	int height(void) const
	throw ()
	{ return m_height; }

	bool isdumb(void) const
	throw ()
	{ return m_isdumb; }

        /**
         * Prints a horizontal line, as large as the terminal.
         */
        void printLine(std::ostream &os = std::cout) const
        throw ();

    protected:
        /**
         * Represents a terminal size.
         */
	int m_width, m_height;

	/**
	 * True if this is a dumb terminal.
	 */
	bool m_isdumb;
};

//}}}
//{{{ Progress -----------------------------------------------------------------


/**
 * Interface for a progress meter.
 */
class Progress {

    public:
        /**
         * Destroys a Progress object.
         */
        virtual ~Progress() {}

    public:
        /**
         * This method has to be called when the operation starts.
         */
        virtual void start()
        throw () = 0;

        /**
         * This operation has to be called repeadedly. We use 64 bit integers
         * because that function may be used in file access functions that that
         * can be 64 bit large (on 32 bit systems with Large File Support).
         *
         * @param[in] current the current progress value
         * @param[in] max the maximum progress value
         */
        virtual void progressed(unsigned long long current,
                                unsigned long long max)
        throw () = 0;

        /**
         * This method has to be called when the operation is finished.
         * It displays the final 100 %.
         *
         * @param[in] success @c true on success, @c false on failure
         */
        virtual void stop(bool success=true)
        throw () = 0;
};

//}}}
//{{{ TerminalProgress ---------------------------------------------------------

/**
 * Retrieve some information about the terminal.
 */
class TerminalProgress : public Progress {

    public:
        /**
         * Creates a new object of type TerminalProgress.
         *
         * @param[in] name the name of the action that is taking place
         *            here
         */
        TerminalProgress(const std::string &name)
        throw ();

        /**
         * This method has to be called when the operation starts.
         */
        void start()
        throw ();

        /**
         * This operation has to be called repeadedly.
         *
         * @param[in] current the current progress value
         * @param[in] max the maximum progress value
         */
        void progressed(unsigned long long current,
                        unsigned long long max)
        throw ();

        /**
         * This method has to be called when the operation is finished.
         * It displays the final 100 %.
         *
         * @param[in] success @c true on success, @c false on failure
         */
        void stop(bool success=true)
        throw ();

    protected:
        void clearLine()
        throw ();

    private:
        std::string m_name;
        int m_linelen;
        int m_progresslen;
        time_t m_lastUpdate;
};

//}}}

#endif /* PROGRESS_H */

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:
