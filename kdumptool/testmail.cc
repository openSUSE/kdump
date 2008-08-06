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
#include <string>
#include <iostream>
#include <cstdlib>
#include <stdexcept>

#include "global.h"
#include "configuration.h"
#include "util.h"
#include "stringutil.h"
#include "debug.h"
#include "email.h"

using std::cerr;
using std::cout;
using std::endl;
using std::string;
using std::ifstream;
using std::ofstream;

// -----------------------------------------------------------------------------
int main(int argc, char *argv[])
{
    Debug::debug()->setStderrLevel(Debug::DL_TRACE);
    try {
        Configuration *config = Configuration::config();
        config->readFile("/etc/sysconfig/kdump");

        // don't break the build when libesmtp is not configured
#if HAVE_LIBESMTP

        Email mail("root@suse.de");
        mail.setTo("bwalle@suse.de");
        mail.addCc("bwalle@suse.de");
        mail.addCc("bernhard@bwalle.de");
        mail.setSubject("Bla");
        mail.setBody("x\n\ny");

        mail.send();

#endif // HAVE_LIBESMTP

    } catch (const std::exception &ex) {
        cerr << "Fatal exception: " << ex.what() << endl;
        return EXIT_FAILURE;
    }

    return EXIT_SUCCESS;
}

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:
