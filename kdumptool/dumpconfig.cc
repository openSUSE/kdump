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
    : m_format(FMT_SHELL), m_usage((1 << ConfigOption::USE_MAX) - 1),
      m_nodefault(false)
{
    string formatlist;
    for (size_t i = 0; i < sizeof(format_names)/sizeof(format_names[0]); ++i) {
	if (!formatlist.empty())
	    formatlist += ", ";
	formatlist.push_back('\'');
	formatlist += format_names[i];
	formatlist.push_back('\'');
    }
    m_formatOption = new StringOption("format", 'f', &m_formatString,
        "Set the output format (" + formatlist + ")");
    m_options.push_back(m_formatOption);

    string usagelist;
    for (size_t i = 0; i < sizeof(usage_names)/sizeof(usage_names[0]); ++i) {
	usagelist.push_back('\'');
	usagelist += usage_names[i];
	usagelist += "', ";
    }
    m_usageOption = new StringOption("usage", 'u', &m_usageString,
        "Show only options used at a certain stage\n"
        "\t(" + usagelist + "'all')");
    m_options.push_back(m_usageOption);

    m_options.push_back(new FlagOption("nodefault", 'n', &m_nodefault,
	"Omit variables which have their default values"));
}

// -----------------------------------------------------------------------------
const char *DumpConfig::getName() const
{
    return "dump_config";
}

// -----------------------------------------------------------------------------
void DumpConfig::parseArgs(const StringVector &args)
{
    Debug::debug()->trace(__FUNCTION__);

    if (m_formatOption->isSet()) {
	size_t i;
	for (i = 0; i < sizeof(format_names)/sizeof(format_names[0]); ++i)
	    if (m_formatString == format_names[i]) {
		m_format = (enum Format)i;
		break;
	    }
	if (i >= sizeof(format_names)/sizeof(format_names[0]))
	    throw KError("Unknown value format: " + m_formatString);
    }

    if (m_usageOption->isSet()) {
	m_usage = 0;

	size_t pos = 0;
	while (pos != string::npos) {
	    size_t next = m_usageString.find(',', pos);
	    string token(m_usageString, pos, next - pos);
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

    Debug::debug()->dbg("format: %s, usage: 0x%x",
        format_names[m_format], m_usage);
}

// -----------------------------------------------------------------------------
void DumpConfig::execute()
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
