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
#include <cstring>
#include <cstdlib>
#include <limits>
#include <string>
#include <cstdarg>
#include <list>
#include <map>
#include <sstream>

#include <dirent.h>
#include <fcntl.h>
#include <unistd.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <getopt.h>


bool needsNetwork;
bool needsMakedumpfile;
long long KDUMP_CPUS, KDUMP_LUKS_MEMORY;
bool m_shrink = false;
bool debug = false;

void read_str(std::string &str, const char* path);

void DEBUG(const char *msg, ...)
{
    va_list valist;
	if (!debug)
		return;
    va_start(valist, msg);
    vfprintf(stderr, msg, valist);
	fprintf(stderr, "\n");
    va_end(valist);
}

// All calculations are in KiB

// This macro converts MiB to KiB
#define MB(x)	((x)*1024)

// Shift right by an amount, rounding the result up
#define shr_round_up(n,amt)	(((n) + (1UL << (amt)) - 1) >> (amt))

// The following macros are defined:
//
//    DEF_RESERVE_KB	default reservation size
//    CAN_REDUCE_CPUS   non-zero if the architecture can reduce kernel
//                      memory requirements with nr_cpus=
//

#if defined(__x86_64__)
# define DEF_RESERVE_KB		MB(192)
# define CAN_REDUCE_CPUS	1

#elif defined(__i386__)
# define DEF_RESERVE_KB		MB(192)
# define CAN_REDUCE_CPUS	1

#elif defined(__powerpc64__)
# define DEF_RESERVE_KB		MB(384)
# define CAN_REDUCE_CPUS	0

#elif defined(__powerpc__)
# define DEF_RESERVE_KB		MB(192)
# define CAN_REDUCE_CPUS	0

#elif defined(__s390x__)
# define DEF_RESERVE_KB		MB(192)
# define CAN_REDUCE_CPUS	1

# define align_memmap		s390x_align_memmap

#elif defined(__s390__)
# define DEF_RESERVE_KB		MB(192)
# define CAN_REDUCE_CPUS	1

# define align_memmap		s390_align_memmap

#elif defined(__aarch64__)
# define DEF_RESERVE_KB		MB(192)
# define CAN_REDUCE_CPUS	1

#elif defined(__arm__)
# define DEF_RESERVE_KB		MB(192)
# define CAN_REDUCE_CPUS	1

#elif defined(__riscv)
# define DEF_RESERVE_KB		MB(192)
# define CAN_REDUCE_CPUS	1

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

// Default framebuffer size: 1024x768 @ 32bpp, except on mainframe
#if defined(__s390__) || defined(__s390x__)
# define DEF_FRAMEBUFFER_KB	0
#else
# define DEF_FRAMEBUFFER_KB	(768UL*4)
#endif

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

// Reserve this much percent above the calculated value
#define ADD_RESERVE_PCT		30

// Reserve this much additional KiB above the calculated value
#define ADD_RESERVE_KB		MB(64)


// Maximum size of the page bitmap
// 32 MiB is 32*1024*1024*8 = 268435456 bits
// makedumpfile uses two bitmaps, so each has 134217728 bits
// with 4-KiB pages this covers 0.5 TiB of RAM in one cycle
#define MAX_BITMAP_KB	MB(32)

// Minimum lowmem allocation. This is 64M for swiotlb and 8M
// for overflow, DMA buffers, etc.
#define MINLOW_KB	MB(64 + 8)

using std::cerr;
using std::cout;
using std::endl;
using std::ifstream;
using std::string;

class SizeConstants {
    protected:
        unsigned long m_kernel_base;
        unsigned long m_kernel_init;
        unsigned long m_kernel_init_net;
        unsigned long m_init_cached;
        unsigned long m_init_cached_net;
        unsigned long m_percpu;
        unsigned long m_pagesize;
        unsigned long m_sizeof_page;
        unsigned long m_user_base;
        unsigned long m_user_net;

    public:
        SizeConstants(void);

        /** Get kernel base requirements.
         *
         * This number covers all memory that can is unavailable to user
         * space, even if it is not allocated by the kernel itself, e.g.
         * if it is reserved by the firmware before the kernel starts.
         *
         * It also includes non-changing run-time allocations caused by
         * PID 1 initialisation (sysfs, procfs, etc.).
         *
         * @returns kernel base allocation [KiB]
         */
        unsigned long kernel_base_kb(void) const
        { return m_kernel_base; }

        /** Get additional kernel requirements at boot.
         *
         * This is memory which is required at boot time but gets freed
         * later. Most importantly, it includes the compressed initramfs
         * image.
         *
         * @returns boot-time requirements [KiB]
         */
        unsigned long kernel_init_kb(void) const
        { return m_kernel_init; }

        /** Get additional boot-time kernel requirements for network.
         *
         * @returns additional boot-time requirements for network [KiB]
         */
        unsigned long kernel_init_net_kb(void) const
        { return m_kernel_init_net; }

        /** Get size of the unpacked initramfs.
         *
         * This is the size of the base image, without network.
         *
         * @returns initramfs memory requirements [KiB]
         */
        unsigned long initramfs_kb(void) const
        { return m_init_cached; }

        /** Get the increase in unpacked intramfs size with network.
         *
         * @returns additional network initramfs memory requirements [KiB]
         */
        unsigned long initramfs_net_kb(void) const
        { return m_init_cached_net; }

        /** Get additional memory requirements per CPU.
         *
         * @returns kernel per-cpu allocation size [KiB]
         */
        unsigned long percpu_kb(void) const
        { return m_percpu; }

        /** Get target page size.
         *
         * @returns page size in BYTES
         */
        unsigned long pagesize(void) const
        { return m_pagesize; }

        /** Get the size of struct page.
         *
         * @returns sizeof(struct page) in BYTES
         */
        unsigned long sizeof_page(void) const
        { return m_sizeof_page; }

        /** Get base user-space requirements.
         *
         * @returns user-space base requirements [KiB]
         */
        unsigned long user_base_kb(void) const
        { return m_user_base; }

        /** Get the increas in user-space requirements with network.
         *
         * @returns additional network user-space requirements [KiB]
         */
        unsigned long user_net_kb(void) const
        { return m_user_net; }
};

// -----------------------------------------------------------------------------
SizeConstants::SizeConstants(void)
{
    static const struct {
        const char *const name;
        unsigned long SizeConstants::*const var;
    } vars[] = {
        { "KERNEL_BASE", &SizeConstants::m_kernel_base },
        { "KERNEL_INIT", &SizeConstants::m_kernel_init },
        { "INIT_NET", &SizeConstants::m_kernel_init_net },
        { "INIT_CACHED", &SizeConstants::m_init_cached },
        { "INIT_CACHED_NET", &SizeConstants::m_init_cached_net },
        { "PERCPU", &SizeConstants::m_percpu },
        { "PAGESIZE", &SizeConstants::m_pagesize },
        { "SIZEOFPAGE", &SizeConstants::m_sizeof_page },
        { "USER_BASE", &SizeConstants::m_user_base },
        { "USER_NET", &SizeConstants::m_user_net },
        { nullptr, nullptr }
    };
    for (auto p = &vars[0]; p->name; ++p) {
		char *val = std::getenv(p->name);
		char *end;
        if (!val || !*val)
            throw std::runtime_error(std::string("No value configured for ") + p->name);
        this->*p->var = strtoll(val, &end, 10);
		if (*end)
            throw std::runtime_error(std::string("Invalid value configured for ") + p->name);
    }
}

class HyperInfo {

    public:
        HyperInfo();

    protected:
        std::string m_type, m_guest_type, m_guest_variant;

    public:
        /**
         * Get hypervisor type.
         */
        const std::string& type(void) const
        { return m_type; }

        /**
         * Get hypervisor guest type.
         */
        const std::string& guest_type(void) const
        { return m_guest_type; }

        /**
         * Get hypervisor guest variant (Dom0 or DomU).
         */
        const std::string& guest_variant(void) const
        { return m_guest_variant; }
};

// -----------------------------------------------------------------------------
HyperInfo::HyperInfo()
{
    read_str(m_type, "/sys/hypervisor/type");
    read_str(m_guest_type, "/sys/hypervisor/guest_type");

    if (m_type == "xen") {
        std::string caps;
        std::string::size_type pos, next, len;

        read_str(caps, "/proc/xen/capabilities");

        m_guest_variant = "DomU";
        pos = 0;
        while (pos != std::string::npos) {
            len = next = caps.find(',', pos);
            if (next != std::string::npos) {
                ++next;
                len -= pos;
            }
            if (caps.compare(pos, len, "control_d") == 0) {
                m_guest_variant = "Dom0";
                break;
            }
            pos = next;
        }
    }
}

void read_str(std::string &str, const char* path)
{
    std::ifstream f;

    f.open(path);
    if (!f)
        return;

    getline(f, str);
    f.close();
    if (f.bad())
        throw std::runtime_error(string(path) + ": Read failed");
}

unsigned long Framebuffer_size(string path) 
{
    std::ifstream f;
    unsigned long width, height, stride;
    char sep;
    string fp;

    fp = path + "/virtual_size";
    f.open(fp.c_str());
    if (!f)
	throw std::runtime_error(fp + ": Open failed");
    f >> width >> sep >> height;
    f.close();
    if (f.bad())
	throw std::runtime_error(fp + ": Read failed");
    else if (!f || sep != ',')
	throw std::runtime_error(fp + ": Invalid content!");
    DEBUG("Framebuffer virtual size: %lux%lu", width, height);

    fp = path + "/stride";
    f.open(fp.c_str());
    if (!f)
	throw std::runtime_error(fp + ": Open failed");
    f >> stride;
    f.close();
    if (f.bad())
	throw std::runtime_error(fp + ": Read failed");
    else if (!f || sep != ',')
	throw std::runtime_error(fp + ": Invalid content!");
    DEBUG("Framebuffer stride: %lu bytes", stride);

    return stride * height;
}

unsigned long Framebuffers_size(void)
{
    unsigned long ret = 0UL;

	string fbs = "/sys/class/graphics";
    DIR *dirp = opendir(fbs.c_str());
    if (!dirp)
		throw std::runtime_error("Cannot open directory " + fbs + ". errno=" +  std::to_string(errno));

    try {
		struct dirent *d;

		errno = 0;
		while ( (d = readdir(dirp)) ) {
			char *end;
			if (strncmp(d->d_name, "fb", 2))
				continue;
			strtoul(d->d_name + 2, &end, 10);
			if (*end != '\0' || end == d->d_name + 2)
				continue;
			string fb = fbs + "/" + d->d_name;
			DEBUG("Found framebuffer: %s", fb.c_str());
			ret += Framebuffer_size(fb);
		}
		if (errno)
			throw std::runtime_error("Cannot read directory " + fbs + ". errno=" + std::to_string(errno));
    } catch(...) {
		closedir(dirp);
		throw;
    }
    closedir(dirp);

    DEBUG("Total size of all framebuffers: %lu bytes", ret);
    return ret;
}

class SlabInfo {

    public:
        /**
	 * Initialize a new SlabInfo object.
	 *
	 * @param[in] line Line from /proc/slabinfo
	 */
	SlabInfo(const string &line);

    protected:
	bool m_comment;
	string m_name;
	unsigned long m_active_objs;
	unsigned long m_num_objs;
	unsigned long m_obj_size;
	unsigned long m_obj_per_slab;
	unsigned long m_pages_per_slab;
	unsigned long m_active_slabs;
	unsigned long m_num_slabs;

    public:
	bool isComment(void) const
	{ return m_comment; }

	const string &name(void) const
	{ return m_name; }

	unsigned long activeObjs(void) const
	{ return m_active_objs; }

	unsigned long numObjs(void) const
	{ return m_num_objs; }

	unsigned long objSize(void) const
	{ return m_obj_size; }

	unsigned long objPerSlab(void) const
	{ return m_obj_per_slab; }

	unsigned long pagesPerSlab(void) const
	{ return m_pages_per_slab; }

	unsigned long activeSlabs(void) const
	{ return m_active_slabs; }

	unsigned long numSlabs(void) const
	{ return m_num_slabs; }
};

// -----------------------------------------------------------------------------
SlabInfo::SlabInfo(const string &line)
{
    static const char slabdata_mark[] = " : slabdata ";

    std::istringstream ss(line);
    ss >> m_name;
    if (!ss)
	throw std::runtime_error("Invalid slabinfo line: " + line);

    if (m_name[0] == '#') {
	m_comment = true;
	return;
    }
    m_comment = false;

    ss >> m_active_objs >> m_num_objs >> m_obj_size
       >> m_obj_per_slab >> m_pages_per_slab;
    if (!ss)
	throw std::runtime_error("Invalid slabinfo line: " + line);

    size_t sdpos = line.find(slabdata_mark, ss.tellg());
    if (sdpos == string::npos)
	throw std::runtime_error("Invalid slabinfo line: " + line);

    ss.seekg(sdpos + sizeof(slabdata_mark) - 1, ss.beg);
    ss >> m_active_slabs >> m_num_slabs;
    if (!ss)
	throw std::runtime_error("Invalid slabinfo line: " + line);
}

// Taken from procps:
#define SLABINFO_LINE_LEN	2048
#define SLABINFO_VER_LEN	100

class SlabInfos {

    public:
	typedef std::map<string, const SlabInfo*> Map;

	SlabInfos() {}

	~SlabInfos()
	{ destroyInfo(); }

        /**
	 * SlabInfo for each slab
	 */
	Map m_info;

    private:
	/**
	 * Destroy SlabInfo objects in m_info.
	 */
	void destroyInfo(void);

    public:
        /**
	 * Read the information about each slab.
	 */
	const Map& getInfo(void);
};

// -----------------------------------------------------------------------------
void SlabInfos::destroyInfo(void)
{
    Map::iterator it;
    for (it = m_info.begin(); it != m_info.end(); ++it)
	delete it->second;
    m_info.clear();
}

// -----------------------------------------------------------------------------
const SlabInfos::Map& SlabInfos::getInfo(void)
{
    static const char verhdr[] = "slabinfo - version: ";
    char buf[SLABINFO_VER_LEN], *p, *end;
    unsigned long major, minor;
	string m_path("/proc/slabinfo");

    std::ifstream f(m_path.c_str());
    if (!f)
	throw std::runtime_error(m_path + ": Open failed");
    f.getline(buf, sizeof buf);
    if (f.bad())
	throw std::runtime_error(m_path + ": Read failed");
    else if (!f || strncmp(buf, verhdr, sizeof(verhdr)-1))
	throw std::runtime_error(m_path + ": Invalid version");
    p = buf + sizeof(verhdr) - 1;

    major = strtoul(p, &end, 10);
    if (end == p || *end != '.')
	throw std::runtime_error(m_path + ": Invalid version");
    p = end + 1;
    minor = strtoul(p, &end, 10);
    if (end == p || *end != '\0')
	throw std::runtime_error(m_path + ": Invalid version");
    DEBUG("Found slabinfo version %lu.%lu", major, minor);

    if (major != 2)
	throw std::runtime_error(m_path + ": Unsupported slabinfo version");

    char line[SLABINFO_LINE_LEN];
    while(f.getline(line, SLABINFO_LINE_LEN)) {
	SlabInfo *si = new SlabInfo(line);
	if (si->isComment()) {
	    delete si;
	    continue;
	}
	m_info[si->name()] = si;
    }

    return m_info;
}

class MemRange {

    public:

        typedef unsigned long long Addr;

        /**
	 * Initialize a new MemRange object.
	 *
	 * @param[in] start First address within the range
	 * @param[in] end   Last address within the range
	 */
	MemRange(Addr start, Addr end)
	: m_start(start), m_end(end)
	{}

	/**
	 * Get first address in range.
	 */
	Addr start(void) const
	{ return m_start; }

	/**
	 * Get last address in range.
	 */
	Addr end(void) const
	{ return m_end; }

	/**
	 * Get range length.
	 *
	 * @return number of bytes in the range
	 */
	Addr length() const
	{ return m_end - m_start + 1; }

    protected:

	Addr m_start, m_end;
};

class MemMap {

    public:

        typedef std::list<MemRange> List;

        /**
	 * Initialize a new MemMap object.
	 *
	 * @param[in] procdir Mount point for procfs
	 */
        MemMap(const SizeConstants &sizes, const char *procdir = "/proc");

	/**
	 * Get the total System RAM (in bytes).
	 */
	unsigned long long total(void) const;

	/**
	 * Get the size (in bytes) of the largest block up to
	 * a given limit.
	 *
	 * @param[in] limit  maximum address to be considered
	 */
	unsigned long long largest(unsigned long long limit) const;

	/**
	 * Get the size (in bytes) of the largest block.
	 */
	unsigned long long largest(void) const
	{ return largest(std::numeric_limits<unsigned long long>::max()); }

	/**
	 * Try to allocate a block.
	 */
	unsigned long long find(unsigned long size, unsigned long align) const;

    private:

        const SizeConstants& m_sizes;
	List m_ranges;
        MemRange::Addr m_kstart, m_kend;
};

MemMap::MemMap(const SizeConstants &sizes, const char *procdir)
    : m_sizes(sizes), m_kstart(0), m_kend(0)
{
	string path("/proc/iomem");

    ifstream f(path.c_str());
    if (!f)
	throw std::runtime_error(path + ": Open failed");

    f.setf(std::ios::hex, std::ios::basefield);
    while (f) {
        int firstc = f.peek();
        if (!f.eof()) {
	    MemRange::Addr start, end;

	    if (!(f >> start))
		throw std::runtime_error("Invalid resource start");
	    if (f.get() != '-')
		throw std::runtime_error("Invalid range delimiter");
	    if (!(f >> end))
		throw std::runtime_error("Invalid resource end");

	    int c;
	    while ((c = f.get()) == ' ');
	    if (c != ':')
		throw std::runtime_error("Invalid resource name delimiter");
	    while ((c = f.get()) == ' ');
	    f.unget();

            string name;
	    std::getline(f, name);
            if (firstc != ' ' && name == "System RAM") {
                m_ranges.emplace_back(start, end);
            } else if (firstc == ' ' && !name.compare(0, 7, "Kernel ")) {
                if (!m_kstart)
                    m_kstart = start;
                m_kend = end;
            }
        }
    }

    f.close();
}

// -----------------------------------------------------------------------------
unsigned long long MemMap::total(void) const
{
    unsigned long long ret = 0;

    for (const auto& range : m_ranges)
        ret += range.length();

    return ret;
}

// -----------------------------------------------------------------------------
unsigned long long MemMap::largest(unsigned long long limit) const
{
    unsigned long long ret = 0;

    for (const auto& range : m_ranges) {
	MemRange::Addr start, end, length;

        start = range.start();
	if (start > limit)
	    continue;

        end = range.end();
        if (end > limit)
            end = limit;
        length = end - start + 1;
        if (start <= m_kstart && m_kstart <= end) {
            // Worst case is if the kernel and initrd are spread evenly
            // across this RAM region
            MemRange::Addr ksize =
                (end < m_kend ? end : m_kend) - m_kstart + 1;
            length = (length - ksize - m_sizes.kernel_init_kb()) / 3;
        }
	if (length > ret)
	    ret = length;
    }

    return ret;
}

// -----------------------------------------------------------------------------
unsigned long long MemMap::find(unsigned long size, unsigned long align) const
{
    List::const_reverse_iterator it;

    for (it = m_ranges.rbegin(); it != m_ranges.rend(); ++it) {
        MemRange::Addr base = it->end() + 1;

	if (base < size)
	    continue;

	base -= size;
	base -= base % align;
        if (base >= it->start())
	    return base;
    }

    return ~0ULL;
}

unsigned long SystemCPU_count(const char *name)
{
    unsigned long ret = 0UL;

    ifstream fin(name);
    if (!fin)
	throw std::runtime_error(string("Cannot open ") + name + ", errno=" + std::to_string(errno));

    try {
	unsigned long n1, n2;
	char delim;

	fin.exceptions(ifstream::badbit);
	while (fin >> n1) {
	    fin >> delim;
	    if (fin && delim == '-') {
		if (! (fin >> n2) )
		    throw std::runtime_error(string(name) + ": wrong number format");
		ret += n2 - n1;
		fin >> delim;
	    }
	    if (!fin.eof() && delim != ',')
		throw std::runtime_error(string(name) + ": wrong delimiter: " + delim);
	    ++ret;
	}
	if (!fin.eof())
	    throw std::runtime_error(string(name) + ": wrong number format");
	fin.close();
    } catch (ifstream::failure &e) {
	throw std::runtime_error("Cannot read " + string(name) +", errno=" + std::to_string(errno));
    }

    return ret;
}

static unsigned long runtimeSize(SizeConstants const &sizes,
                                 unsigned long memtotal)
{
    unsigned long required, prev;

    // Run-time kernel requirements
    required = sizes.kernel_base_kb() + sizes.initramfs_kb();

    // Double the size, because fbcon allocates its own framebuffer,
    // and many DRM drivers allocate the hw framebuffer in system RAM
    try {
        required += 2 * Framebuffers_size() / 1024UL;
    } catch(std::runtime_error &e) {
        DEBUG("Cannot get framebuffer size: %s", e.what());
        required += 2 * DEF_FRAMEBUFFER_KB;
    }

    // LUKS Argon2 hash requires a lot of memory
	if (KDUMP_LUKS_MEMORY) {
        required += KDUMP_LUKS_MEMORY;
        DEBUG("Adding %lu KiB for crypto devices", KDUMP_LUKS_MEMORY);
    }

    // Add space for constant slabs
    try {
        SlabInfos slab;
        for (const auto& elem : slab.getInfo()) {
            if (!elem.first.compare(0, 5, "Acpi-")) {
                unsigned long slabsize = elem.second->numSlabs() *
                    elem.second->pagesPerSlab() * sizes.pagesize() / 1024;
                required += slabsize;

                DEBUG("Adding %ld KiB for %s slab cache",
                                    slabsize, elem.second->name().c_str());
            }
        }
    } catch (std::runtime_error &e) {
        DEBUG("Cannot get slab sizes: %s", e.what());
    }

    // Add memory based on CPU count
    unsigned long cpus = 0;
    if (CAN_REDUCE_CPUS) {
	cpus = KDUMP_CPUS;
    }
    if (!cpus) {
        unsigned long online = SystemCPU_count("/sys/devices/system/cpu/online");
        unsigned long offline = SystemCPU_count("/sys/devices/system/cpu/offline");
        DEBUG("CPUs online: %lu, offline: %lu",
                            online, offline);
        cpus = online + offline;
    }
    DEBUG("Total assumed CPUs: %lu", cpus);
    cpus *= sizes.percpu_kb();
    DEBUG("Total per-cpu requirements: %lu KiB", cpus);
    required += cpus;

    // User-space requirements
    unsigned long user = sizes.user_base_kb();
    if (needsNetwork)
        user += sizes.user_net_kb();

    if (needsMakedumpfile) {
        // Estimate bitmap size (1 bit for every RAM page)
        unsigned long bitmapsz = shr_round_up(memtotal / sizes.pagesize(), 2);
        if (bitmapsz > MAX_BITMAP_KB)
            bitmapsz = MAX_BITMAP_KB;
        DEBUG("Estimated bitmap size: %lu KiB", bitmapsz);
        user += bitmapsz;

        // Makedumpfile needs additional 96 B for every 128 MiB of RAM
        user += 96 * shr_round_up(memtotal, 20 + 7);
    }
    DEBUG("Total userspace: %lu KiB", user);
    required += user;

    // Make room for dirty pages and in-flight I/O:
    //
    //   required = prev + dirty + io
    //      dirty = total * (DIRTY_RATIO / 100)
    //         io = dirty * (BUF_PER_DIRTY_MB / 1024)
    //
    // solve the above using integer math:
    unsigned long dirty;
    prev = required;
    required = required * MB(100) /
        (MB(100) - MB(DIRTY_RATIO) - DIRTY_RATIO * BUF_PER_DIRTY_MB);
    dirty = (required - prev) * MB(1) / (MB(1) + BUF_PER_DIRTY_MB);
    DEBUG("Dirty pagecache: %lu KiB", dirty);
    DEBUG("In-flight I/O: %lu KiB", required - prev - dirty);

    // Account for "large hashes"
    prev = required;
    required = required * MB(1024) / (MB(1024) - KERNEL_HASH_PER_MB);
    DEBUG("Large kernel hashes: %lu KiB", required - prev);

    // Add space for memmap
    prev = required;
    required = required * sizes.pagesize() / (sizes.pagesize() - sizes.sizeof_page());
    unsigned long maxpfn = (required - prev) / sizes.sizeof_page();
    required = prev + align_memmap(maxpfn) * sizes.sizeof_page();

    DEBUG("Maximum memmap size: %lu KiB", required - prev);
    DEBUG("Total run-time size: %lu KiB", required);
    return required;
}

// -----------------------------------------------------------------------------
static void shrink_crash_size(unsigned long size)
{
    static const char crash_size_fname[] = "/sys/kernel/kexec_crash_size";

    std::ostringstream ss;
    ss << size;
    const string &numstr = ss.str();

    int fd = open(crash_size_fname, O_WRONLY);
    if (fd < 0)
        throw std::runtime_error(string("Cannot open ") + crash_size_fname +
			", errno=" + std::to_string(errno));
    if (write(fd, numstr.c_str(), numstr.length()) < 0) {
        if (errno == EINVAL)
            DEBUG("New crash kernel size is bigger");
        else if (errno == ENOENT)
            throw std::runtime_error("Crash kernel is currently loaded");
        else
            throw std::runtime_error(string("Cannot write to ") + crash_size_fname +
                               ", errno=" + std::to_string(errno));
    }
    close(fd);
}

// -----------------------------------------------------------------------------
int main(int argc, char* argv[])
{
	int opt;
	static option long_options[] = {
		{"shrink", 0, 0, 's'},
		{0, 0, 0, 0}
	};

	while ((opt = getopt_long(argc, argv, "ds", long_options, NULL)) != -1) {
	       switch (opt) {
			case 's':
				m_shrink = true;
				break;
			case 'd':
				debug = true;
				break;
			default:
				exit(2);
		}
	}

	/* parse environment variables */
	try {
		char *end, *val;
		val = std::getenv("KDUMP_CPUS");
		if (!val || !*val)
			throw std::runtime_error("KDUMP_CPUS not defined");
		KDUMP_CPUS = strtoll(val, &end, 10);
		if (*end)
			throw std::runtime_error("KDUMP_CPUS invalid");

		val = std::getenv("KDUMP_LUKS_MEMORY");
		if (!val || !*val)
			throw std::runtime_error("KDUMP_LUKS_MEMORY not defined");
		KDUMP_LUKS_MEMORY = strtoll(val, &end, 10);
		if (*end)
			throw std::runtime_error("KDUMP_LUKS_MEMORY invalid");

		val = std::getenv("KDUMP_PROTO");
		if (!val || !*val)
			throw std::runtime_error("KDUMP_PROTO not defined");
		needsNetwork = strcmp(val, "file");

		val = std::getenv("KDUMP_DUMPFORMAT");
		if (!val || !*val)
			throw std::runtime_error("KDUMP_DUMPFORMAT not defined");
		needsMakedumpfile = strcmp(val, "none") && strcmp(val, "raw");
	}
	catch(std::runtime_error &e) {
		cerr << "Error parsing config from environment variables: " << e.what() << endl;
		exit(1);
	}

    HyperInfo hyper;
    DEBUG("Hypervisor type: %s", hyper.type().c_str());
    DEBUG("Guest type: %s", hyper.guest_type().c_str());
    DEBUG("Guest variant: %s", hyper.guest_variant().c_str());
    if (hyper.type() == "xen" && hyper.guest_type() == "PV" &&
        hyper.guest_variant() == "DomU") {
        cout << "Total: 0" << endl;
        cout << "Low: 0" << endl;
        cout << "High: 0" << endl;
        cout << "MinLow: 0" << endl;
        cout << "MaxLow: 0" << endl;
        cout << "MinHigh: 0 " << endl;
        cout << "MaxHigh: 0 " << endl;
        return 0;
    }

    SizeConstants sizes;
    MemMap mm(sizes);
    unsigned long required;
    unsigned long memtotal = shr_round_up(mm.total(), 10);

    // Get total RAM size
    DEBUG("Expected total RAM: %lu KiB", memtotal);

    // Calculate boot requirements
    unsigned long bootsize = sizes.kernel_base_kb() +
        sizes.kernel_init_kb() + sizes.initramfs_kb();
    if (needsNetwork)
        bootsize += sizes.kernel_init_net_kb() + sizes.initramfs_net_kb();
    DEBUG("Memory needed at boot: %lu KiB", bootsize);

    try {
        required = runtimeSize(sizes, memtotal);

	// Make sure there is enough space at boot
	if (required < bootsize)
	    required = bootsize;

        // Reserve a fixed percentage on top of the calculation
        required = (required * (100 + ADD_RESERVE_PCT)) / 100 + ADD_RESERVE_KB;

    } catch(std::runtime_error &e) {
	cerr << "Error calculating required reservation, using default: " << e.what() << endl;
	required = DEF_RESERVE_KB;
    }

    unsigned long low, minlow, maxlow;
    unsigned long high, minhigh, maxhigh;

#if defined(__x86_64__)

    unsigned long long base = mm.find(required << 10, 16UL << 20);

    DEBUG("Estimated crash area base: 0x%llx", base);

    // If maxpfn is above 4G, SWIOTLB may be needed
    if ((base + (required << 10)) >= (1ULL<<32)) {
	DEBUG("Adding 64 MiB for SWIOTLB");
	required += MB(64);
    }

    if (base < (1ULL<<32)) {
        low = minlow = 0;
    } else {
        low = minlow = MINLOW_KB;
        required = (required > low ? required - low : 0);
        if (required < bootsize)
            required = bootsize;
    }
    high = required;

    maxlow = mm.largest(1ULL<<32) >> 10;
    minhigh = 0;
    maxhigh = mm.largest() >> 10;

#else  // __x86_64__

    minlow = MINLOW_KB;
    low = required;

# if defined(__i386__)
    maxlow = mm.largest(512ULL<<20) >> 10;
# else
    maxlow = mm.largest() >> 10;
# endif  // __i386__

    high = 0;
    minhigh = 0;
    maxhigh = 0;

#endif  // __x86_64__

    cout << "Total: " << (memtotal >> 10) << endl;
    cout << "Low: " << shr_round_up(low, 10) << endl;
    cout << "High: " << shr_round_up(high, 10) << endl;
    cout << "MinLow: " << shr_round_up(minlow, 10) << endl;
    cout << "MaxLow: " << (maxlow >> 10) << endl;
    cout << "MinHigh: " << shr_round_up(minhigh, 10) << endl;
    cout << "MaxHigh: " << (maxhigh >> 10) << endl;

#if HAVE_FADUMP
    unsigned long fadump, minfadump, maxfadump;

    /* min = 64 MB, max = 50% of total memory, suggested = 5% of total memory */
    maxfadump = memtotal / 2;
    minfadump = MB(64);
    if (maxfadump < minfadump)
        maxfadump = minfadump;
    fadump = memtotal / 20;
    if (fadump < minfadump)
        fadump = minfadump;
    if (fadump > maxfadump)
        fadump = maxfadump;


    cout << "Fadump: "    << shr_round_up(fadump, 10) << endl;
    cout << "MinFadump: " << shr_round_up(minfadump, 10) << endl;
    cout << "MaxFadump: " << shr_round_up(maxfadump, 10) << endl;
#endif

    if (m_shrink)
        shrink_crash_size(required << 10);
}

