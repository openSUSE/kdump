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

//{{{ OptionType ---------------------------------------------------------------

enum OptionType {
    OT_INVALID,
    OT_STRING,
    OT_INTEGER,
    OT_FLAG
};

//}}}
//{{{ OptionValue --------------------------------------------------------------

class OptionValue {
    public:
        OptionValue();

    public:
        void setType(OptionType type);
        OptionType getType() const
            throw ()
            { return m_type; }

        void setString(const std::string &string);
        const std::string& getString() const
            throw ()
            { return m_string; }

        void setFlag(bool flag);
        bool getFlag() const
            throw ()
            { return m_flag; }

        void setInteger(int value);
        int getInteger() const
            throw ()
            { return m_integer; }

    private:
        OptionType      m_type;
        int             m_integer;
        std::string     m_string;
        bool            m_flag;
};

//}}}
//{{{ Option -------------------------------------------------------------------

class Option {
    public:
        Option(const std::string &name, char letter,
                OptionType type = OT_FLAG,
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
        OptionType getType() const
            throw ()
            { return m_type; }
        const std::string& getDescription() const
            throw ()
            { return m_description; }

        /* that's set by the OptionParser */
        void setValue(OptionValue value);
        const OptionValue& getValue() const
            throw ()
            { return m_value; }

        virtual const char *getPlaceholder(void) const
            throw ()
            { return NULL; }

    private:
        std::string m_longName;
        std::string m_description;
        char        m_letter;
        OptionType  m_type;
        OptionValue m_value;
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
                   const std::string &description = "");
};

//}}}
//{{{ StringOption -------------------------------------------------------------

/**
 * Option which takes a string as argument.
 */
class StringOption : public Option {
    public:
        StringOption(const std::string &name, char letter,
                     const std::string &description = "");

        virtual const char *getPlaceholder(void) const
            throw ()
            { return "<STRING>"; }
};

//}}}
//{{{ IntOption ----------------------------------------------------------------

/**
 * Option which takes an integer value as argument.
 */
class IntOption : public Option {
    public:
        IntOption(const std::string &name, char letter,
                  const std::string &description = "");

        virtual const char *getPlaceholder(void) const
            throw ()
            { return "<NUMBER>"; }
};

//}}}
//{{{ OptionParser -------------------------------------------------------------

typedef std::pair<std::string, const OptionList*> StringOptionListPair;
typedef std::vector<StringOptionListPair> StringOptionListVector;

class OptionParser {
    public:
        void addOption(Option *option);

        void printHelp(std::ostream &os, const std::string &name) const;
        void parse(int argc, char *argv[]);
        OptionValue getValue(const std::string &name);
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
