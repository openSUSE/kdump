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

#include <libelf.h>
#include <gelf.h>

#include "subcommand.h"
#include "debug.h"
#include "savedump.h"
#include "util.h"
#include "fileutil.h"
#include "transfer.h"
#include "configuration.h"
#include "dataprovider.h"
#include "progress.h"
#include "vmcoreinfo.h"

using std::string;

//{{{ Vmcoreinfo ---------------------------------------------------------------

// -----------------------------------------------------------------------------
void Vmcoreinfo::readFromELF(const char *elf_file)
    throw (KError)
{
}

// -----------------------------------------------------------------------------
ByteVector Vmcoreinfo::readElfNote(const char *file)
    throw (KError)
{


}
//}}}

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:
