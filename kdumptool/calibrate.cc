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
#include <unistd.h>

#include "subcommand.h"
#include "debug.h"
#include "calibrate.h"
#include "configuration.h"
#include "util.h"

// All calculations are in KiB

// This macro converts MiB to KiB
#define MB(x)	((x)*1024)

// Shift right by an amount, rounding the result up
#define shr_round_up(n,amt)	(((n) + (1UL << (amt)) - 1) >> (amt))

// Default reservation size depends on architecture
//
// The following macros are defined:
//
//    DEF_RESERVE_KB	default reservation size
//    KERNEL_KB		kernel image text + data + bss
//    INIT_KB		basic initramfs size (unpacked)
//    INIT_NET_KB	initramfs size increment when network is included
//    SIZE_STRUCT_PAGE	sizeof(struct page)
//
#if defined(__x86_64__)
# define DEF_RESERVE_KB		MB(128)
# define KERNEL_KB		MB(16)
# define INIT_KB		MB(28)
# define INIT_NET_KB		MB(3)
# define SIZE_STRUCT_PAGE	56
#elif defined(__i386__)
# define DEF_RESERVE_KB		MB(128)
# define KERNEL_KB		MB(14)
# define INIT_KB		MB(24)
# define INIT_NET_KB		MB(2)
# define SIZE_STRUCT_PAGE	32
#elif defined(__powerpc64__)
# define DEF_RESERVE_KB		MB(256)
# define KERNEL_KB		MB(16)
# define INIT_KB		MB(48)
# define INIT_NET_KB		MB(4)
# define SIZE_STRUCT_PAGE	64
#elif defined(__powerpc__)
# define DEF_RESERVE_KB		MB(128)
# define KERNEL_KB		MB(12)
# define INIT_KB		MB(28)
# define INIT_NET_KB		MB(2)
# define SIZE_STRUCT_PAGE	32
#elif defined(__s390x__)
# define DEF_RESERVE_KB		MB(128)
# define KERNEL_KB		MB(13)
# define INIT_KB		MB(28)
# define INIT_NET_KB		MB(2)
# define SIZE_STRUCT_PAGE	56
#elif defined(__s390__)
# define DEF_RESERVE_KB		MB(128)
# define KERNEL_KB		MB(12)
# define INIT_KB		MB(24)
# define INIT_NET_KB		MB(2)
# define SIZE_STRUCT_PAGE	32
#elif defined(__ia64__)
# define DEF_RESERVE_KB		MB(512)
# define KERNEL_KB		MB(32)
# define INIT_KB		MB(36)
# define INIT_NET_KB		MB(4)
# define SIZE_STRUCT_PAGE	56
#elif defined(__aarch64__)
# define DEF_RESERVE_KB		MB(128)
# define KERNEL_KB		MB(10)
# define INIT_KB		MB(24)
# define INIT_NET_KB		MB(2)
# define SIZE_STRUCT_PAGE	56
#elif defined(__arm__)
# define DEF_RESERVE_KB		MB(128)
# define KERNEL_KB		MB(12)
# define INIT_KB		MB(24)
# define INIT_NET_KB		MB(2)
# define SIZE_STRUCT_PAGE	32
#else
# error "No default crashkernel reservation for your architecture!"
#endif

// (Pessimistic) estimate of the initrd compression ratio (percents)
#define INITRD_COMPRESS	50

// Estimated non-changing dynamic data (sysfs, procfs, etc.)
#define KERNEL_DYNAMIC_KB	MB(4)

// large hashes, default settings:			     -> per MiB
//   PID: sizeof(void*) for each 256 KiB			  4
//   Dentry cache: sizeof(void*) for each  8 KiB		128
//   Inode cache:  sizeof(void*) for each 16 KiB		 64
//   TCP established: 2*sizeof(void*) for each 128 KiB		 16
//   TCP bind: 2*sizeof(void*) for each 128 KiB			 16
//   UDP: 2*sizeof(void*) + 2*sizeof(long) for each 2 MiB	  1 + 1
//   UDP-Lite: 2*sizeof(void*) + 2*sizeof(long) for each 2 MiB	  1 + 1
//								-------
//								230 + 2
// Assuming that sizeof(void*) == sizeof(long):
#define KERNEL_HASH_PER_MB	(232*sizeof(long))

// Estimated buffer metadata and filesystem in KiB per dirty MiB
#define BUF_PER_DIRTY_MB	64

// Default vm dirty ratio is 20%
#define DIRTY_RATIO		20

// Userspace base requirements:
//   bash (PID 1)	 3 M
//   udevd		 2 M
//   kdumptool		 4 M
//   makedumpfile	 1 M
// -------------------------
// TOTAL:		17 M
#define USER_BASE_KB	MB(17)

// Additional requirements when network is configured
//   dhclient		 7 M
#define USER_NET_KB	MB(7)

// Maximum size of the page bitmap
// 32 MiB is 32*1024*1024*8 = 268435456 bits
// makedumpfile uses two bitmaps, so each has 134217728 bits
// with 4-KiB pages this covers 0.5 TiB of RAM in one cycle
#define MAX_BITMAP_KB	MB(32)

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
    Debug::debug()->trace("Calibrate::execute()");

    unsigned long required, prev;
    unsigned long pagesize = sysconf(_SC_PAGESIZE);

    try {
	Configuration *config = Configuration::config();
	bool needsnet = config->needsNetwork();

	// Get total RAM size
	unsigned long memtotal = Util::getMemorySize();
        Debug::debug()->dbg("Expected total RAM: %lu KiB", memtotal);

	// Calculate boot requirements
	unsigned long ramfs = INIT_KB;
	if (needsnet)
	    ramfs += INIT_NET_KB;
	unsigned long initrd = (ramfs * INITRD_COMPRESS) / 100;
	unsigned long bootsize = KERNEL_KB + initrd + ramfs;
        Debug::debug()->dbg("Memory needed at boot: %lu KiB", bootsize);

	// Run-time kernel requirements
	required = KERNEL_KB + ramfs + KERNEL_DYNAMIC_KB;

	// User-space requirements
	unsigned long user = USER_BASE_KB;
	if (needsnet)
	    user += USER_NET_KB;

	if (config->needsMakedumpfile()) {
	    // Estimate bitmap size (1 bit for every RAM page)
	    unsigned long bitmapsz = shr_round_up(memtotal / pagesize, 2);
	    if (bitmapsz > MAX_BITMAP_KB)
		bitmapsz = MAX_BITMAP_KB;
	    Debug::debug()->dbg("Estimated bitmap size: %lu KiB", bitmapsz);
	    user += bitmapsz;

	    // Makedumpfile needs additional 96 B for every 128 MiB of RAM
	    user += 96 * shr_round_up(memtotal, 20 + 7);
	}
        Debug::debug()->dbg("Total userspace: %lu KiB", user);
	required += user;

	// Make room for dirty pages and in-flight I/O:
	//
	//   required = prev + dirty + io
	//      dirty = total * (DIRTY_RATIO / 100)
	//	   io = dirty * (BUF_PER_DIRTY_MB / 1024)
	//
	// solve the above using integer math:
	unsigned long dirty;
	prev = required;
	required = required * MB(100) /
	    (MB(100) - MB(DIRTY_RATIO) - DIRTY_RATIO * BUF_PER_DIRTY_MB);
	dirty = (required - prev) * MB(1) / (MB(1) + BUF_PER_DIRTY_MB);
        Debug::debug()->dbg("Dirty pagecache: %lu KiB", dirty);
        Debug::debug()->dbg("In-flight I/O: %lu KiB", required - prev - dirty);

	// Account for "large hashes"
	prev = required;
	required = required * MB(1024) / (MB(1024) - KERNEL_HASH_PER_MB);
        Debug::debug()->dbg("Large kernel hashes: %lu KiB", required - prev);

	// Add space for memmap
	prev = required;
	required = required * pagesize / (pagesize - SIZE_STRUCT_PAGE);
        Debug::debug()->dbg("Maximum memmap size: %lu KiB", required - prev);

	// Make sure there is enough space at boot
	if (required < bootsize)
	    required = bootsize;

    } catch(KError) {
	required = DEF_RESERVE_KB;
    }

    cout << shr_round_up(required, 10) << endl;
}

//}}}

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:
