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
#ifndef CALIBRATE_H
#define CALIBRATE_H

#include "subcommand.h"
#include "fileutil.h"

//{{{ Calibrate ----------------------------------------------------------------

/**
 * Subcommand to calibrate reserved memory
 */
class Calibrate : public Subcommand {
    protected:
        bool m_shrink;

    public:
        /**
         * Creates a new Calibrate object.
         */
        Calibrate();

    public:
        /**
         * Returns the name of the subcommand (calibrate).
         */
        const char *getName() const;

        /**
         * Executes the function.
         *
         * @throw KError on any error. No exception indicates success.
         */
        void execute();

    private:
};

//}}}
//{{{ SystemCPU ----------------------------------------------------------------

class SystemCPU {

    public:
        /**
	 * Initialize a new SystemCPU object.
	 *
	 * @param[in] sysdir Mount point for sysfs
	 */
	SystemCPU(const char *sysdir = "/sys")
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

//}}}

#endif /* CALIBRATE_H */

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:
