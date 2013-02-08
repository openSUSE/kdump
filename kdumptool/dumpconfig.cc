/*
 * (c) 2013, Petr Tesarik <ptesarik@suse.cz>, SUSE LINUX Products GmbH
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

#include "subcommand.h"
#include "debug.h"
#include "dumpconfig.h"
#include "configuration.h"
#include "quotedstring.h"

using std::string;
using std::cout;

const char *DumpConfig::format_names[] = {
    "shell",			// DumpConfig::FMT_SHELL
    "kernel",			// DumpConfig::FMT_KERNEL
};

const char *DumpConfig::usage_names[] = {
    "mkinitrd",			// ConfigOption::USE_MKINITRD
    "kexec",			// ConfigOption::USE_KEXEC
    "dump",			// ConfigOption::USE_DUMP
};

//{{{ DumpConfig ---------------------------------------------------------------

// -----------------------------------------------------------------------------
DumpConfig::DumpConfig()
    throw ()
    : m_format(FMT_SHELL), m_usage((1 << ConfigOption::USE_MAX) - 1),
      m_nodefault(false)
{}

// -----------------------------------------------------------------------------
const char *DumpConfig::getName() const
    throw ()
{
    return "dump_config";
}

// -----------------------------------------------------------------------------
OptionList DumpConfig::getOptions() const
    throw ()
{
    OptionList list;

    Debug::debug()->trace("DumpConfig::getOptions()");

    string formatlist;
    for (size_t i = 0; i < sizeof(format_names)/sizeof(format_names[0]); ++i) {
	if (!formatlist.empty())
	    formatlist += ", ";
	formatlist.push_back('\'');
	formatlist += format_names[i];
	formatlist.push_back('\'');
    }
    list.push_back(Option("format", 'f', OT_STRING,
	"Set the output format (" + formatlist + ")"));

    string usagelist;
    for (size_t i = 0; i < sizeof(usage_names)/sizeof(usage_names[0]); ++i) {
	usagelist.push_back('\'');
	usagelist += usage_names[i];
	usagelist += "', ";
    }
    list.push_back(Option("usage", 'u', OT_STRING,
	"Show only options used at a certain stage\n"
	"\t(" + usagelist + "'all')"));

    list.push_back(Option("nodefault", 'n', OT_FLAG,
	"Omit variables which have their default values"));

    return list;

}

// -----------------------------------------------------------------------------
void DumpConfig::parseCommandline(OptionParser *optionparser)
    throw (KError)
{
    Debug::debug()->trace("DumpConfig::parseCommandline(%p)", optionparser);

    if (optionparser->getValue("format").getType() != OT_INVALID) {
	string format = optionparser->getValue("format").getString();
	size_t i;
	for (i = 0; i < sizeof(format_names)/sizeof(format_names[0]); ++i)
	    if (format == format_names[i]) {
		m_format = (enum Format)i;
		break;
	    }
	if (i >= sizeof(format_names)/sizeof(format_names[0]))
	    throw KError("Unknown value format: " + format);
    }

    if (optionparser->getValue("usage").getType() != OT_INVALID) {
	string usage = optionparser->getValue("usage").getString();
	m_usage = 0;

	size_t pos = 0;
	while (pos != string::npos) {
	    size_t next = usage.find(',', pos);
	    string token(usage, pos, next - pos);
	    pos = next;
	    if (pos != string::npos)
		++pos;

	    size_t i;
	    for (i = 0; i < sizeof(usage_names)/sizeof(usage_names[0]); ++i)
		if (token == usage_names[i]) {
		    m_usage |= 1 << i;
		    break;
		}
	    if (i >= sizeof(usage_names)/sizeof(usage_names[0])) {
		if (token == "all")
		    m_usage = (1 << ConfigOption::USE_MAX) - 1;
		else
		    throw KError("Unknown usage string: " + token);
	    }
	}
    }

    if (optionparser->getValue("nodefault").getFlag())
        m_nodefault = true;

    Debug::debug()->dbg("format: %s, usage: 0x%x",
        format_names[m_format], m_usage);
}

// -----------------------------------------------------------------------------
void DumpConfig::execute()
    throw (KError)
{
    Debug::debug()->trace("DumpConfig::execute()");

    Configuration *config = Configuration::config();

    ConfigOptionIterator it,
	begin = config->optionsBegin(),
	end = config->optionsEnd();

    QuotedString *qs;
    char delim;
    switch (m_format) {
    case FMT_SHELL:
        qs = new ShellQuotedString();
        delim = '\n';
        break;
    case FMT_KERNEL:
        qs = new KernelQuotedString();
        delim = ' ';
        break;
    default:
        throw KError("Invalid format: " + (int)m_format);
    }

    for (it = begin; it != end; ++it) {
	if (((*it)->usage() & m_usage) == 0)
	    continue;
	if (m_nodefault && (*it)->isDefault())
	    continue;

        qs->assign((*it)->valueAsString());
        cout << (*it)->name() << "=" << qs->quoted() << delim;
    }

    delete qs;
}

//}}}

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:
