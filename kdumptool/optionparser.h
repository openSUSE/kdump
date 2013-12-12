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
#ifndef OPTIONPARSER_H
#define OPTIONPARSER_H

#include <list>
#include <map>
#include <string>
#include <vector>

//{{{ Option -------------------------------------------------------------------

/* forward declaration for getoptArgs */
struct option;

class Option {
    public:
        Option(const std::string &name, char letter,
                const std::string &description = "");

        /**
         * Empty (but virtual) constructor.
         */
        virtual ~Option()
        {}

    public:
        const std::string& getLongName() const
            throw ()
            { return m_longName; }
        char getLetter() const
            throw ()
            { return m_letter; }
        const std::string& getDescription() const
            throw ()
            { return m_description; }

        /**
         * Get the parameters for getopt_long().
         *
         * @param[out] opt filled with appropriate data for long options
         * @return option string to be used by getopt (short)
         */
        virtual std::string getoptArgs(struct option *opt)
            = 0;

	/**
	 * Set the option value.
	 *
	 * @param[in] arg option value as a C-style string
	 */
	virtual void setValue(const char *arg)
	    = 0;

        /**
         * Check if a value was set.
         *
         * @return true if the option was specified.
         */
        bool isSet(void)
            throw ()
            { return m_isSet; }

        virtual const char *getPlaceholder(void) const
            throw ()
            { return NULL; }

    private:
        const std::string m_longName;
        const std::string m_description;
        const char        m_letter;

    protected:
        bool        m_isSet;
};

typedef std::list<Option*> OptionList;

//}}}
//{{{ FlagOption ---------------------------------------------------------------

/**
 * Option which serves as a flag toggle.
 */
class FlagOption : public Option {
    public:
        FlagOption(const std::string &name, char letter,
                   bool *value,
                   const std::string &description = "");

        virtual std::string getoptArgs(struct option *opt);

	/**
	 * Set the flag.
	 *
	 * @param[in] arg ignored
	 */
	virtual void setValue(const char *arg);

    private:
        bool *m_value;
};

//}}}
//{{{ StringOption -------------------------------------------------------------

/**
 * Option which takes a string as argument.
 */
class StringOption : public Option {
    public:
        StringOption(const std::string &name, char letter,
                     std::string *value,
                     const std::string &description = "");

        virtual const char *getPlaceholder(void) const
            throw ()
            { return "<STRING>"; }

        virtual std::string getoptArgs(struct option *opt);

	/**
	 * Set the option value.
	 *
	 * @param[in] arg option value as a C-style string
	 */
	virtual void setValue(const char *arg);

    private:
        std::string *m_value;
};

//}}}
//{{{ IntOption ----------------------------------------------------------------

/**
 * Option which takes an integer value as argument.
 */
class IntOption : public Option {
    public:
        IntOption(const std::string &name, char letter,
                  int *value,
                  const std::string &description = "");

        virtual const char *getPlaceholder(void) const
            throw ()
            { return "<NUMBER>"; }

        virtual std::string getoptArgs(struct option *opt);

	/**
	 * Set the option value.
	 *
	 * @param[in] arg option value as a C-style string
	 */
	virtual void setValue(const char *arg);


    private:
        int *m_value;
};

//}}}
//{{{ OptionParser -------------------------------------------------------------

typedef std::pair<std::string, const OptionList*> StringOptionListPair;
typedef std::vector<StringOptionListPair> StringOptionListVector;

class OptionParser {
    public:
	/**
	 * Add a global option to the parser.
	 *
	 * @param option the global option to be added
	 */
        void addGlobalOption(Option *option);

        void printHelp(std::ostream &os, const std::string &name) const;
        void parse(int argc, char *argv[]);
        std::vector<std::string> getArgs();

        /**
         * That's only for a pretty help for now. That means:
         *
         *   program --option argument --option2
         *   program argument --option --option2
         *
         * are equivalent. So it's not legal to use the same option both as
         * "global option" and as option for a subcommand.
         */
        void addSubcommand(const std::string &name, const OptionList &options);

    protected:
        int parsePartial(int argc, char *argv[], const OptionList& opts,
            bool rearrange = true);
        Option &findOption(char letter);

        template <class InputIterator>
        void printHelpForOptionList(std::ostream &os,
            InputIterator begin, InputIterator end,
            const std::string &indent = "") const;

    private:
        std::vector<Option*> m_options;
        std::vector<std::string> m_args;
        StringOptionListVector m_subcommandOptions;
        OptionList m_globalOptions;

};

//}}}

#endif /* OPTIONPARSER_H */

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:
