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
#ifndef KDUMPTOOL_H
#define KDUMPTOOL_H

#include "global.h"
#include "optionparser.h"
#include "subcommand.h"

//{{{ KdumpTool ----------------------------------------------------------------

/**
 * Main class of the program.
 */
class KdumpTool {

    public:
        /**
         * Creates a new KdumpTool object. Mainly for initialisation.
         */
        KdumpTool()
        throw ();

    public:
        /**
         * Parses the command line. This method must be called before the
         * execute() method is called.
         */
        void parseCommandline(int argc, char *argv[])
        throw (KError);

        /**
         * Executes the main program.
         */
        void execute()
        throw (KError);

        /**
         * Returns the error code. Important: It's optional that a non-zero
         * error value is set when an exception is thrown. The caller has to
         * do this himself.
         *
         * @return the error code
         */
        int getErrorCode() const
        throw ();

    private:
        OptionParser m_optionParser;
        Subcommand *m_subcommand;
        int m_errorcode;
        bool m_background;
};

//}}}

#endif /* KDUMPTOOL_H */

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:
