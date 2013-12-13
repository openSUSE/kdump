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
#include "multipath.h"
#include "configuration.h"

using std::string;
using std::cin;
using std::cout;

//{{{ MultipathConf -----------------------------------------------------------

// -----------------------------------------------------------------------------
MultipathConf::MultipathConf(std::istream &input)
    throw ()
    : m_input(input)
{ }

// -----------------------------------------------------------------------------
void MultipathConf::process(class Handler &handler)
{
    while (readline()) {
	StringVector tokens;
	string::const_iterator it = m_line.begin();
	bool stringmode = false;
	while (1) {

	    // skip whitespace
	    while (it != m_line.end() && *it &&
		   (isspace(*it) || *it < 0))
		++it;

	    // end of line or comment?
	    if (it == m_line.end() || *it == '\0' || *it == '#' || *it == '!')
		break;

	    // get a token
	    if (*it == '"') {
		// double quotes toggle string mode
		stringmode = !stringmode;
		tokens.push_back(string(1, *it));
		++it;
	    } else if (!stringmode && (*it == '{' || *it == '}')) {
		// group start/end
		tokens.push_back(string(1, *it));
		++it;
	    } else {
		string::const_iterator itr(it);
		while (itr != m_line.end() && *itr != '\0' &&
		       // quote terminates a token
		       *itr != '"' &&
		       // any special character is accepted in string mode
		       (stringmode ||
			// outside of string mode, tokens are terminated at:
			// * white space or non-ASCII
			!(isspace(*itr) || *itr < 0 ||
			  // * comment
			  *itr == '#' || *itr == '!' ||
			  // * group start/end
			  *itr == '{' || *itr == '}')))
		    ++itr;
		tokens.push_back(string(it, itr));
		it = itr;
	    }
	}

	handler.process(m_line, tokens);
    }

    StringVector empty;
    handler.process(m_line, empty);
}

// -----------------------------------------------------------------------------
bool MultipathConf::readline(void)
{
    char c;
    m_line.clear();
    do {
	if (!m_input.get(c))
	    break;
	m_line += c;
    } while (c != '\n' && c != '\r');
    return !m_input.fail();
}

//{{{ AddBlacklistHandler -----------------------------------------------------

/**
 * This class implements a multipath.conf handler that copies the config file
 * to an output stream, adding blacklist and blacklist_exceptions sections.
 *
 * If the respective sections are already found in the input, then the
 * appropriate keywords are added there, because multipath doesn't accept
 * multiple root sections.
 *
 * If the sections are not found, they are added to the beginning of the file.
 * It is not added to the end of the file, because closing braces are optional
 * at the end of the file, and opening braces are optional after a keyword
 * which is known (to multipath!) to start a subsection. As a result, the
 * state of the multipath parser at the end of the file cannot be reliably
 * known. Unlike the state at the beginning of the file.
 */
class AddBlacklistHandler : public MultipathConf::Handler
{
    public:
        AddBlacklistHandler(std::ostream &output,
			    const StringVector &exceptions)
        throw ();
        ~AddBlacklistHandler();

        virtual void process(const std::string &raw, StringVector &tokens);

    protected:
	std::ostream &m_output;
        const StringVector &m_exceptions;

    private:
        void addLine(const std::string &raw);
        void outputLine(const std::string &raw);
        typedef void (AddBlacklistHandler::*lineHandler)(const std::string&);
        void doBlacklist(lineHandler handler);
        void doExceptions(lineHandler handler);

        /**
	 * Raw lines read from the input config file
	 */
        StringVector m_lines;

        bool m_blacklist_done;
        bool m_exceptions_done;
};

// -----------------------------------------------------------------------------
AddBlacklistHandler::AddBlacklistHandler(std::ostream &output,
                                         const StringVector &exceptions)
    throw ()
    : m_output(output), m_exceptions(exceptions),
      m_blacklist_done(false), m_exceptions_done(false)
{ }

// -----------------------------------------------------------------------------
AddBlacklistHandler::~AddBlacklistHandler()
{
    StringVector::const_iterator it;

    if (!m_blacklist_done) {
        m_output << "blacklist {\n";
        doBlacklist(&AddBlacklistHandler::outputLine);
        m_output << "}\n";
    }
    if (!m_exceptions_done) {
        m_output << "blacklist_exceptions {\n";
        doExceptions(&AddBlacklistHandler::outputLine);
        m_output << "}\n";
    }

    for (it = m_lines.begin(); it != m_lines.end(); ++it)
        m_output << *it;
}

// -----------------------------------------------------------------------------
void AddBlacklistHandler::process(const string &raw, StringVector &tokens)
{
    m_lines.push_back(raw);
    if (tokens.size() > 0) {
        if (tokens[0] == "blacklist")
            doBlacklist(&AddBlacklistHandler::addLine);
        else if (tokens[0] == "blacklist_exceptions")
            doExceptions(&AddBlacklistHandler::addLine);
    }
}

// -----------------------------------------------------------------------------
void AddBlacklistHandler::addLine(const string &raw)
{
    m_lines.push_back(raw);
}

// -----------------------------------------------------------------------------
void AddBlacklistHandler::outputLine(const string &raw)
{
    m_output << raw;
}

// -----------------------------------------------------------------------------
void AddBlacklistHandler::doBlacklist(lineHandler handler)
{
    (this->*handler)("\twwid \"*\"\n");
    m_blacklist_done = true;
}

void AddBlacklistHandler::doExceptions(lineHandler handler)
{
    StringVector::const_iterator it;
    for (it = m_exceptions.begin(); it != m_exceptions.end(); ++it)
	(this->*handler)("\t" + *it + "\n");
    m_exceptions_done = true;
}

//{{{ Multipath ---------------------------------------------------------------

/**
 * Global instance, automatically registered in the global subcommand list.
 */
static Multipath globalInstance;

// -----------------------------------------------------------------------------
Multipath::Multipath()
    throw ()
    : m_parser(cin)
{ }

// -----------------------------------------------------------------------------
const char *Multipath::getName() const
    throw ()
{
    return "multipath";
}

// -----------------------------------------------------------------------------
bool Multipath::needsConfigfile() const
    throw ()
{
    return false;
}

// -----------------------------------------------------------------------------
void Multipath::parseCommandline(OptionParser *optionparser)
    throw (KError)
{
    Debug::debug()->trace("Multipath::parseCommandline(%p)", optionparser);

    m_exceptions = optionparser->getArgs();
    m_exceptions.erase(m_exceptions.begin());
}

// -----------------------------------------------------------------------------
void Multipath::execute()
    throw (KError)
{
    Debug::debug()->trace("Multipath::execute()");
    AddBlacklistHandler handler(cout, m_exceptions);
    m_parser.process(handler);
}

//}}}

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:
