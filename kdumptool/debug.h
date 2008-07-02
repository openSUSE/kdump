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
#ifndef DEBUG_H
#define DEBUG_H

#include <cstdio>
#include <cstdarg>

//{{{ Debugging ----------------------------------------------------------------

class Debug {
    public:
        enum Level {
            DL_TRACE    = 0,
            DL_DEBUG    = 10,
            DL_INFO     = 20,
            DL_NONE     = 100
        };

    public:
        static Debug *debug();

        void dbg(const char *msg, ...);
        void info(const char *msg, ...);
        void trace(const char *msg, ...);
        void dbg(const std::string &s);
        void info(const std::string &s);
        void trace(const std::string &s);
        void msg(Debug::Level level, const char *msg, ...);
        void vmsg(Debug::Level level, const char *msg, std::va_list args);

        void setLevel(Debug::Level level);
        Debug::Level getLevel() const;
        bool isDebugEnabled() const;
        void setFileHandle(FILE *handle);
        FILE *getFileHandle() const;

    protected:
        Debug();

    private:
        static Debug *m_instance;

    private:
        Level m_debuglevel;
        FILE *m_handle;
};

//}}}

#endif /* DEBUG_H */

// vim: set sw=4 ts=4 fdm=marker et:
