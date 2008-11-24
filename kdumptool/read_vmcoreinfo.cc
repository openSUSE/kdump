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
#include <iostream>
#include <string>
#include <cerrno>
#include <fcntl.h>

#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <sys/ioctl.h>
#include <linux/kd.h>
#include <stdlib.h>
#include <stdbool.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <getopt.h>
#include <fcntl.h>


#include "subcommand.h"
#include "debug.h"
#include "read_vmcoreinfo.h"
#include "vmcoreinfo.h"
#include "util.h"

using std::string;
using std::cout;
using std::cerr;
using std::endl;

//{{{ ReadVmcoreinfo -----------------------------------------------------------------

// -----------------------------------------------------------------------------
ReadVmcoreinfo::ReadVmcoreinfo()
    throw ()
    : m_file(DEFAULT_DUMP)
{}

// -----------------------------------------------------------------------------
const char *ReadVmcoreinfo::getName() const
    throw ()
{
    return "read_vmcoreinfo";
}

// -----------------------------------------------------------------------------
OptionList ReadVmcoreinfo::getOptions() const
    throw ()
{
    OptionList list;

    list.push_back(Option("dump", 'u', OT_STRING,
        "Use the specified dump instead of " DEFAULT_DUMP "."));

    return list;
}

// -----------------------------------------------------------------------------
void ReadVmcoreinfo::parseCommandline(OptionParser *optionparser)
    throw (KError)
{
    Debug::debug()->trace("ReadVmcoreinfo::parseCommandline(%p)", optionparser);

    if (optionparser->getValue("dump").getType() != OT_INVALID)
        m_file = optionparser->getValue("dump").getString();

    if (optionparser->getArgs().size() == 2)
        m_option = optionparser->getArgs()[1];

    Debug::debug()->dbg("file=%s, option=%s", m_file.c_str(), m_option.c_str());
}

// -----------------------------------------------------------------------------
void ReadVmcoreinfo::execute()
    throw (KError)
{
    Vmcoreinfo vm;
    vm.readFromELF(m_file.c_str());

    if (vm.isXenVmcoreinfo()) {
        cerr << "VMCOREINFO_XEN:" << endl;
    } else {
        cerr << "VMCOREINFO:" << endl;
    }

    if (m_option.size() > 0) {
        Debug::debug()->dbg("Printing value of %s", m_option.c_str());
        cout << vm.getStringValue(m_option.c_str()) << endl;
    } else {
        StringList keys = vm.getKeys();
        for (StringList::const_iterator it = keys.begin();
                it != keys.end(); ++it)
            cout << *it << "=" << vm.getStringValue((*it).c_str()) << endl;
    }
}

//}}}

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:
