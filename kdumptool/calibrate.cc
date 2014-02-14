/*
 * (c) 2014, Petr Tesarik <ptesarik@suse.de>, SUSE LINUX Products GmbH
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

#include "subcommand.h"
#include "debug.h"
#include "calibrate.h"

// Default reservation size depends on architecture
#if defined(__x86_64__)
# define DEF_RESERVE_MB 128
#elif defined(__i386__)
# define DEF_RESERVE_MB 128
#elif defined(__powerpc64__)
# define DEF_RESERVE_MB 256
#elif defined(__powerpc__)
# define DEF_RESERVE_MB 128
#elif defined(__s390x__)
# define DEF_RESERVE_MB 128
#elif defined(__s390__)
# define DEF_RESERVE_MB 128
#elif defined(__ia64__)
# define DEF_RESERVE_MB 512
#elif defined(__aarch64__)
# define DEF_RESERVE_MB 128
#elif defined(__arm__)
# define DEF_RESERVE_MB 128
#else
# error "No default crashkernel reservation for your architecture!"
#endif

using std::cout;
using std::endl;

//{{{ Calibrate ----------------------------------------------------------------

// -----------------------------------------------------------------------------
Calibrate::Calibrate()
    throw ()
{}

// -----------------------------------------------------------------------------
const char *Calibrate::getName() const
    throw ()
{
    return "calibrate";
}

// -----------------------------------------------------------------------------
void Calibrate::execute()
    throw (KError)
{
    unsigned long required;
    Debug::debug()->trace("Calibrate::execute()");

    required = DEF_RESERVE_MB;
    cout << required << endl;
}

//}}}

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:
