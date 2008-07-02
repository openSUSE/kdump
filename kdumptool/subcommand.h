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
#include <map>

#include "optionparser.h"
#include "global.h"

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
         * Creates a new Subcommand object.
         */
        Subcommand(const char *name, OptionParser *optionparser)
        throw ();

    public:
        /**
         * Executes the function.
         *
         * @throw KError on any error. No exception indicates success.
         */
        virtual void execute()
        throw (KError)  = 0;

        /**
         * Returns the name of the subcommand.
         *
         * @return the name (string is static, you must copy it if you want
         *         to modify it)
         */
        const char *getName() const;

    private:
        OptionParser    *m_optionparser;
        const char      *m_name;
};

//}}}
//{{{ SubcommandManager --------------------------------------------------------

/**
 * Singleton class where any Subcommand is registered.
 *
 */
class SubcommandManager {

    public:
        /**
         * Returns the only instance of the class.
         */
        SubcommandManager *instance()
        throw ();

        /**
         * Gets a Subcommand object for the given name.
         *
         * @return a subcommand object if the subcommand exists, or
         *         @c NULL on failure.
         */
        Subcommand *getSubcommand(const char *name) const
        throw ();

    protected:
        /**
         * Creates a new SubcommandManager object. This is protected to
         * implement the singleton.
         */
        SubcommandManager()
        throw ();

        /**
         * Adds a subcommand. This function is called in the constructor (i.e.
         * when the first object is generated.
         */
        void addSubcommand(Subcommand *command)
        throw ();

    private:
        std::map<const char *, Subcommand *> m_subcommandMap;
        static SubcommandManager *m_instance;
};

//}}}

// vim: set sw=4 ts=4 et fdm=marker:
