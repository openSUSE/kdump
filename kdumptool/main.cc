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

using std::cerr;
using std::cout;
using std::endl;

// -----------------------------------------------------------------------------
int main(int argc, char *argv[])
{
    KdumpTool kdt;
    bool exception = false;

    try {
        kdt.parseCommandline(argc, argv);
        //kdt.execute();

        TerminalProgress tp("/boot/vmlinuz");
        FileDataProvider dp("/mounts/dist/install/openSUSE-11.0-RC1-Live/openSUSE-11.0-RC1-GNOME-LiveCD-i386.iso");
        dp.setProgress(&tp);
        //SFTPTransfer filetransfer("sftp://root:n0vel!onl4%@localhost/tmp/a/b/c");

        CIFSTransfer filetransfer("cifs://bwalle:com.desoft@localhost/bwalle/df");
        //NFSTransfer filetransfer("nfs://stravinsky/extern/bwalle/test.iso");
        filetransfer.perform(&dp, "vmlinux");
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
