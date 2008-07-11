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
#include <errno.h>
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
#include "ledblink.h"
#include "util.h"

using std::string;
using std::cout;
using std::cerr;
using std::endl;

//{{{ LedBlink -----------------------------------------------------------------

// -----------------------------------------------------------------------------
void led_exit_handler(int signo, void *data)
{
    int fd = (unsigned long)data;

    // reset the LEDs to show the state of the keyboard
    int counter = 0;
    ioctl(fd, KDGKBLED, &counter);
    ioctl(fd, KDSETLED, counter);

    close(fd);
}

// -----------------------------------------------------------------------------
LedBlink::LedBlink()
    throw ()
    : m_interval(500)
{}

// -----------------------------------------------------------------------------
const char *LedBlink::getName() const
    throw ()
{
    return "ledblink";
}

// -----------------------------------------------------------------------------
OptionList LedBlink::getOptions() const
    throw ()
{
    OptionList list;

    list.push_back(Option("interval", 'i', OT_INTEGER,
        "Blink interval in ms (default is 500 ms)"));

    return list;
}

// -----------------------------------------------------------------------------
void LedBlink::parseCommandline(OptionParser *optionparser)
    throw (KError)
{
    Debug::debug()->trace(__FUNCTION__);

    if (optionparser->getValue("interval").getType() != OT_INVALID) {
        int value = optionparser->getValue("interval").getInteger();
        m_interval = value;
    }

    Debug::debug()->dbg("interval=%d", m_interval);
}

// -----------------------------------------------------------------------------
void LedBlink::execute()
    throw (KError)
{
    int ret, console = -1;
    struct timespec wait_period;

    /* set the interval */
    wait_period.tv_sec = m_interval / 1000;
    wait_period.tv_nsec = (m_interval % 1000) * 1000 * 1000;

    console = open("/dev/console", O_NOCTTY);
    if (console < 0)
        throw KSystemError("Could not open console.", errno);

    // register the exit handler
    ret = on_exit(led_exit_handler, (void *)console);
    if (ret != 0)
        throw KSystemError("Registering led_exit_handler failed.", errno);

    for (long counter = 0; ; counter = (counter + 1) % 8) {

        ret = ioctl(console, KDSETLED, counter);
        if (ret < 0)
            throw KSystemError("Unable to ioctl(KDSETLED) "
                    "-- are you not on the console?", errno);

        nanosleep(&wait_period, NULL);
    }
}

//}}}

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:
