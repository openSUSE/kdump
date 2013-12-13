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
#ifndef SUBCOMMAND_H
#define SUBCOMMAND_H

#include <list>

#include "global.h"
#include "option.h"

//{{{ Subcommand ---------------------------------------------------------------

/**
 * In kdumptool, the main function mainly executes the sub command of the
 * binary: So if the command is called with
 *
 *     kdumptool myfunction --myoption
 *
 * then it executes the MyfunctionSubcommand. Each subcommand has a method
 * Subcommand::execute() that gets executed. Error handling is done via
 * exceptions, so if the function does not throw an exception, everything was
 * successful.
 *
 */
class Subcommand {
    public:

        /**
         * List of Subcommand pointers.
         */
        typedef std::list<Subcommand *> List;

        /**
         * Default list where new Subcommands are registered.
         */
        static List GlobalList;

        /**
         * Creates a new subcommand. This is for initialisation.
         */
        Subcommand(List &list = GlobalList)
        throw ();

        /**
         * Empty destructor.
         */
        virtual ~Subcommand();

        /**
         * Returns the name of the subcommand.
         *
         * @return the name (string is static, you must copy it if you want
         *         to modify it)
         */
        virtual const char *getName() const
        throw () = 0;

        /**
         * Returns a list of options that command understands. This is for
         * the global option parsing function. The default implementation
         * returns an empty OptionList.
         */
        const OptionList& getOptions() const
        throw ()
        { return m_options; }

        /**
         * Checks if that subcommand needs the configuration file to be read.
         * Default implementation is true.
         *
         * @return @c true if the configuration file must be read,
         *         @c false otherwise.
         */
        virtual bool needsConfigfile() const
        throw ();

        /**
         * Parses the non-option arguments from the command line. This
         * method gets called before Subcommand::execute().
         *
         * The default implementation just does nothing.
         */
        virtual void parseArgs(const StringVector &args)
        throw (KError);

        /**
         * Executes the function.
         *
         * @throw KError on any error. No exception indicates success.
         */
        virtual void execute()
        throw (KError)  = 0;

        /**
         * Sets an error code. As default, -1 (255) is used when an exception
         * is thrown, and 0 is used on success.
         *
         * @param errorcode the error code that should be used
         */
        void setErrorCode(int errorcode)
        throw ();

        /**
         * Returns the error code.
         *
         * @return the error code. When no error code has been set, the function
         *         returns 0. This means that the caller has to check if an
         *         exception has occurred, and if that case, transform the
         *         0 to 255.
         */
        int getErrorCode() const
        throw ();

    protected:
        /**
         * List of options for this command. Child classes are expected to
         * add their options in the constructor.
         * A const reference to this list is returned by getOptions().
         */
        OptionList m_options;

    private:
        int m_errorcode;
};

//}}}

#endif // SUBCOMMAND_H

// vim: set sw=4 ts=4 et fdm=marker: :collapseFolds=1:
