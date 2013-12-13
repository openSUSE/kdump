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
#ifndef OPTIONPARSER_H
#define OPTIONPARSER_H

#include <list>
#include <map>
#include <string>
#include <vector>

#include "option.h"

//{{{ OptionParser -------------------------------------------------------------

typedef std::pair<std::string, const OptionList*> StringOptionListPair;
typedef std::vector<StringOptionListPair> StringOptionListVector;

class OptionParser {
    public:
	/**
	 * Add a global option to the parser.
	 *
	 * @param option the global option to be added
	 */
        void addGlobalOption(Option *option);

        void printHelp(std::ostream &os, const std::string &name) const;
        void parse(int argc, char *argv[]);
        std::vector<std::string> getArgs();

        /**
         * That's only for a pretty help for now. That means:
         *
         *   program --option argument --option2
         *   program argument --option --option2
         *
         * are equivalent. So it's not legal to use the same option both as
         * "global option" and as option for a subcommand.
         */
        void addSubcommand(const std::string &name, const OptionList &options);

    protected:
        int parsePartial(int argc, char *argv[], const OptionList& opts,
            bool rearrange = true);

        void printHelpForOptionList(std::ostream &os, const OptionList& opts,
            const std::string &indent = "") const;

    private:
        std::vector<std::string> m_args;
        StringOptionListVector m_subcommandOptions;
        OptionList m_globalOptions;

};

//}}}

#endif /* OPTIONPARSER_H */

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:
