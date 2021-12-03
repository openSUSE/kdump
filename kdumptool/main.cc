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
#include <cstdlib>
#include <stdexcept>

#include "global.h"
#include "kdumptool.h"
#include "transfer.h"
#include "dataprovider.h"
#include "progress.h"
#include "stringutil.h"

// Subcommand initialization
#include "deletedumps.h"
#include "dumpconfig.h"
#include "findkernel.h"
#include "identifykernel.h"
#include "ledblink.h"
#include "multipath.h"
#include "print_target.h"
#include "read_ikconfig.h"
#include "read_vmcoreinfo.h"
#include "savedump.h"
#include "calibrate.h"

using std::cerr;
using std::cout;
using std::endl;
using std::list;

// -----------------------------------------------------------------------------
int main(int argc, char *argv[])
{
    KdumpTool kdt;
    bool exception = false;

    try {
        kdt.addSubcommand(new DeleteDumpsCommand);
        kdt.addSubcommand(new DumpConfig);
        kdt.addSubcommand(new FindKernel);
        kdt.addSubcommand(new IdentifyKernel);
        kdt.addSubcommand(new LedBlink);
        kdt.addSubcommand(new Multipath);
        kdt.addSubcommand(new PrintTarget);
        kdt.addSubcommand(new ReadIKConfig);
        kdt.addSubcommand(new ReadVmcoreinfo);
        kdt.addSubcommand(new SaveDump);
        kdt.addSubcommand(new Calibrate);

        kdt.parseCommandline(argc, argv);
        kdt.readConfiguration();
        kdt.execute();
    } catch (const KError &ke) {
        cerr << ke.what() << endl;
        exception = true;
    } catch (const std::exception &ex) {
        cerr << "Fatal exception: " << ex.what() << endl;
        exception = true;
    }

    if (exception && kdt.getErrorCode() == 0)
        return -1;
    else
        return kdt.getErrorCode();
}

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:
