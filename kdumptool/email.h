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
#ifndef EMAIL_H
#define EMAIL_H

#if HAVE_LIBESMTP
#   include <libesmtp.h>
#endif // HAVE_LIBESMTP

#include "global.h"

#if HAVE_LIBESMTP

//{{{ KSmtpErrorCode -----------------------------------------------------------

/**
 * Class for libesmtp errors.
 */
class KSmtpErrorCode : public KErrorCode {
    public:
        KSmtpErrorCode(int code)
	    : KErrorCode(code)
        {}

        virtual std::string message(void) const;
};

typedef KCodeError<KSmtpErrorCode> KSmtpError;

//}}}
//{{{ Email --------------------------------------------------------------------


/**
 * Send an email.
 */
class Email {

    public:
        /**
         * Creates a new Email object.
         *
         * @param[in] from the sender in a RFC-compilant form, e.g.
         *                 <tt>"Bla Blub" &lt;bla@blub.org&gt;</tt>
         */
        Email(const std::string &from);

        /**
         * Destroyes a Email object.
         */
        virtual ~Email()
        { }

    public:
        /**
         * Sets the recipient.
         *
         * @param[in] recipient the recipient
         */
        void setTo(const std::string &recipient);

        /**
         * Adds an address to Cc.
         *
         * @param[in] cc an address in a RFC-compilant form, e.g.
         *               <tt>"Bla Blub" &lt;bla@blub.org&gt;</tt>
         */
        void addCc(const std::string &cc);

        /**
         * Sets the subject of the mail.
         *
         * @param[in] subject the subject string, must not contain umlauts,
         *                    i.e. only ASCII characters
         */
        void setSubject(const std::string &subject);

        /**
         * Sets the body of the mail.
         *
         * @param[in] body the body string
         */
        void setBody(const std::string &body);

        /**
         * Sets the hostname of localhost. If that function is not called,
         * libesmtp uses uname.
         *
         * @param[in] hostname the host name to use
         */
        void setHostname(const std::string &hostname);

        /**
         * Sends the specified email.
         *
         * @exception ESmtpError the exception that is thrown when an error
         *            occurred
         */
        void send();


     private:
         std::string m_to;
         StringVector m_cc;
         std::string m_subject;
         std::string m_from;
         std::string m_hostname;
         std::string m_body;
};

//}}}

#endif // HAVE_LIBESMTP
#endif // EMAIL_H

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:
