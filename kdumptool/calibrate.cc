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
#include <fstream>
#include <cerrno>
#include <unistd.h>

#include "subcommand.h"
#include "debug.h"
#include "calibrate.h"
#include "configuration.h"
#include "util.h"
#include "fileutil.h"

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
//    KDUMP_PHYS_LOAD   assumed physical load address of the kdump kernel;
//                      if pages between 0 and the load address are not
//                      counted into total memory, set this to ZERO
//    PERCPU_KB         additional kernel memory for each online CPU
//    CAN_REDUCE_CPUS   non-zero if the architecture can reduce kernel
//                      memory requirements with nr_cpus=
//

#if defined(__x86_64__)
# define DEF_RESERVE_KB		MB(128)
# define KERNEL_KB		MB(16)
# define INIT_KB		MB(28)
# define INIT_NET_KB		MB(3)
# define SIZE_STRUCT_PAGE	56
# define KDUMP_PHYS_LOAD	0
# define CAN_REDUCE_CPUS	1
# define PERCPU_KB		108

#elif defined(__i386__)
# define DEF_RESERVE_KB		MB(128)
# define KERNEL_KB		MB(14)
# define INIT_KB		MB(24)
# define INIT_NET_KB		MB(2)
# define SIZE_STRUCT_PAGE	32
# define KDUMP_PHYS_LOAD	0
# define CAN_REDUCE_CPUS	1
# define PERCPU_KB		56

#elif defined(__powerpc64__)
# define DEF_RESERVE_KB		MB(256)
# define KERNEL_KB		MB(16)
# define INIT_KB		MB(48)
# define INIT_NET_KB		MB(4)
# define SIZE_STRUCT_PAGE	64
# define KDUMP_PHYS_LOAD	MB(128)
# define CAN_REDUCE_CPUS	0
# define PERCPU_KB		172	// FIXME: is it non-linear?

#elif defined(__powerpc__)
# define DEF_RESERVE_KB		MB(128)
# define KERNEL_KB		MB(12)
# define INIT_KB		MB(28)
# define INIT_NET_KB		MB(2)
# define SIZE_STRUCT_PAGE	32
# define KDUMP_PHYS_LOAD	MB(128)
# define CAN_REDUCE_CPUS	0
# define PERCPU_KB		0	// TODO !!!

#elif defined(__s390x__)
# define DEF_RESERVE_KB		MB(128)
# define KERNEL_KB		MB(13)
# define INIT_KB		MB(28)
# define INIT_NET_KB		MB(2)
# define SIZE_STRUCT_PAGE	56
# define KDUMP_PHYS_LOAD	0
# define CAN_REDUCE_CPUS	1
# define PERCPU_KB		0	// TODO !!!

# define align_memmap		s390x_align_memmap

#elif defined(__s390__)
# define DEF_RESERVE_KB		MB(128)
# define KERNEL_KB		MB(12)
# define INIT_KB		MB(24)
# define INIT_NET_KB		MB(2)
# define SIZE_STRUCT_PAGE	32
# define KDUMP_PHYS_LOAD	0
# define CAN_REDUCE_CPUS	1
# define PERCPU_KB		0	// TODO !!!

# define align_memmap		s390_align_memmap

#elif defined(__ia64__)
# define DEF_RESERVE_KB		MB(512)
# define KERNEL_KB		MB(32)
# define INIT_KB		MB(36)
# define INIT_NET_KB		MB(4)
# define SIZE_STRUCT_PAGE	56
# define KDUMP_PHYS_LOAD	0
# define CAN_REDUCE_CPUS	1
# define PERCPU_KB		0	// TODO !!!

#elif defined(__aarch64__)
# define DEF_RESERVE_KB		MB(128)
# define KERNEL_KB		MB(10)
# define INIT_KB		MB(24)
# define INIT_NET_KB		MB(2)
# define SIZE_STRUCT_PAGE	56
# define KDUMP_PHYS_LOAD	0
# define CAN_REDUCE_CPUS	1
# define PERCPU_KB		0	// TODO !!!

#elif defined(__arm__)
# define DEF_RESERVE_KB		MB(128)
# define KERNEL_KB		MB(12)
# define INIT_KB		MB(24)
# define INIT_NET_KB		MB(2)
# define SIZE_STRUCT_PAGE	32
# define KDUMP_PHYS_LOAD	0
# define CAN_REDUCE_CPUS	1
# define PERCPU_KB		0	// TODO !!!

#else
# error "No default crashkernel reservation for your architecture!"
#endif

#ifndef align_memmap
# define align_memmap(maxpfn)	((unsigned long) (maxpfn))
#endif

static inline unsigned long s390_align_memmap(unsigned long maxpfn)
{
    // SECTION_SIZE_BITS: 25, PAGE_SHIFT: 12, KiB: 10
    return ((maxpfn - 1) | ((1UL << (25 - 12 - 10)) - 1)) + 1;
}

static inline unsigned long s390x_align_memmap(unsigned long maxpfn)
{
    // SECTION_SIZE_BITS: 28, PAGE_SHIFT: 12, KiB: 10
    return ((maxpfn - 1) | ((1UL << (28 - 12 - 10)) - 1)) + 1;
}

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
//   10 * udevd		12 M
//   kdumptool		 4 M
//   makedumpfile	 1 M
// -------------------------
// TOTAL:		20 M
#define USER_BASE_KB	MB(20)

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
using std::ifstream;

//{{{ SystemCPU ----------------------------------------------------------------

class SystemCPU {

    public:
        /**
	 * Initialize a new SystemCPU object.
	 *
	 * @param[in] sysdir Mount point for sysfs
	 */
	SystemCPU(const char *sysdir = "/sys")
	throw ()
	: m_cpudir(FilePath(sysdir).appendPath("devices/system/cpu"))
	{}

    protected:
        /**
	 * Path to the cpu system devices base directory
	 */
	const FilePath m_cpudir;

        /**
	 * Count the number of CPUs in a cpuset
	 *
	 * @param[in] name Name of the cpuset ("possible", "present", "online")
	 *
	 * @exception KError if the file cannot be opened or parsed
	 */
	unsigned long count(const char *name);

    public:
        /**
	 * Count the number of online CPUs
	 *
	 * @exception KError see @c count()
	 */
	unsigned long numOnline(void)
	{ return count("online"); }

        /**
	 * Count the number of offline CPUs
	 *
	 * @exception KError see @c count()
	 */
	unsigned long numOffline(void)
	{ return count("offline"); }

};

// -----------------------------------------------------------------------------
unsigned long SystemCPU::count(const char *name)
{
    FilePath path(m_cpudir);
    path.appendPath(name);
    unsigned long ret = 0UL;

    ifstream fin(path.c_str());
    if (!fin)
	throw KSystemError("Cannot open " + path, errno);

    try {
	unsigned long n1, n2;
	char delim;

	fin.exceptions(ifstream::badbit);
	while (fin >> n1) {
	    fin >> delim;
	    if (fin && delim == '-') {
		if (! (fin >> n2) )
		    throw KError(path + ": wrong number format");
		ret += n2 - n1;
		fin >> delim;
	    }
	    if (!fin.eof() && delim != ',')
		throw KError(path + ": wrong delimiter: " + delim);
	    ++ret;
	}
	if (!fin.eof())
	    throw KError(path + ": wrong number format");
	fin.close();
    } catch (ifstream::failure e) {
	throw KSystemError("Cannot read " + path, errno);
    }

    return ret;
}

//}}}
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

	// Add memory based on CPU count
	unsigned long cpus;
	if (CAN_REDUCE_CPUS) {
	    cpus = config->KDUMP_CPUS.value();
	} else {
	    SystemCPU syscpu;
	    unsigned long online = syscpu.numOnline();
	    unsigned long offline = syscpu.numOffline();
	    Debug::debug()->dbg("CPUs online: %lu, offline: %lu",
				online, offline);
	    cpus = online + offline;
	}
	Debug::debug()->dbg("Total assumed CPUs: %lu", cpus);

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

	// Account for memory between 0 and KDUMP_PHYS_LOAD
	required += KDUMP_PHYS_LOAD;
	Debug::debug()->dbg("Assumed load offset: %lu KiB", KDUMP_PHYS_LOAD);

	// Account for "large hashes"
	prev = required;
	required = required * MB(1024) / (MB(1024) - KERNEL_HASH_PER_MB);
        Debug::debug()->dbg("Large kernel hashes: %lu KiB", required - prev);

	// Add space for memmap
	prev = required;
	required = required * pagesize / (pagesize - SIZE_STRUCT_PAGE);
	unsigned long maxpfn = (required - prev) / SIZE_STRUCT_PAGE;
	required = prev + align_memmap(maxpfn) * SIZE_STRUCT_PAGE;
        Debug::debug()->dbg("Maximum memmap size: %lu KiB", required - prev);

	// Make sure there is enough space at boot
	Debug::debug()->dbg("Total run-time size: %lu KiB", required);
	if (required < bootsize)
	    required = bootsize;

    } catch(KError e) {
	Debug::debug()->info(e.what());
	required = DEF_RESERVE_KB;
    }

    cout << shr_round_up(required, 10) << endl;
}

//}}}

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:
