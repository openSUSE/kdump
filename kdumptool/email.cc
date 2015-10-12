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
#include <string>
#include <cstring>

#include "global.h"

#if HAVE_LIBESMTP
#   include <auth-client.h>
#endif // HAVE_LIBESMTP

#include "debug.h"
#include "email.h"
#include "configuration.h"
#include "util.h"
#include "stringutil.h"
#include "routable.h"

using std::string;

#if HAVE_LIBESMTP

// -----------------------------------------------------------------------------
static int authinteract(auth_client_request_t request,
	     char **result, int fields, void *arg)
{
    static char userbuffer[BUFSIZ];
    static char passwordbuffer[BUFSIZ];

    Debug::debug()->trace("authinteract()");
    Configuration *config = Configuration::config();

    strncpy(userbuffer, config->KDUMP_SMTP_USER.value().c_str(), BUFSIZ);
    strncpy(passwordbuffer, config->KDUMP_SMTP_PASSWORD.value().c_str(), BUFSIZ);

    for (int i = 0; i < fields; i++) {
        if (request[i].flags & AUTH_PASS)
            result[i] = userbuffer;
        else if (request[i].flags & AUTH_USER)
            result[i] = passwordbuffer;
        else if (result[i] == NULL)
            return 0;
    }

    return 1;
}

// -----------------------------------------------------------------------------
static const char *smtp_messagecb(void **buf, int *len, void *arg)
{
    string *buffer = (string *)arg;
    static bool first_called = true;
    char *retval = strdup(buffer->c_str());

    Debug::debug()->trace("smtp_messagecb(), first_called=%d",
        int(first_called));

    if (!buf) {
        Debug::debug()->dbg("smtp_messagecb() called with inavlid args.");
        return NULL;
    }

    if (len == NULL) {
        Debug::debug()->dbg("rewind");
        first_called = true;
        return NULL;
    }

    if (first_called) {
        *buf = retval;
        *len = buffer->size();
        first_called = false;

        return retval;
    } else {
        *buf = NULL;

        return NULL;
    }
}

// -----------------------------------------------------------------------------
static void smtp_monitor_cb(const char *buf, int buflen, int writing, void *arg)
{
    // make sure that the buffer is 0-terminated
    char *mybuffer = new char[buflen+1];
    memmove(mybuffer, buf, buflen);
    mybuffer[buflen] = '\0';

    // replace newlines with spaces
    for (int i = 0; i < buflen; i++) {
        if (mybuffer[i] == '\n' || mybuffer[i] == '\r') {
            mybuffer[i] = ' ';
        }
    }

    const char *direction;
    switch (writing) {
        case SMTP_CB_READING:
            direction = "<=";
            break;

        case SMTP_CB_WRITING:
            direction = "=>";
            break;

        default:
            direction = "";
            break;
    }

    Debug::debug()->trace("ESMTP: %s %s", direction, mybuffer);

    delete[] mybuffer;
}


// -----------------------------------------------------------------------------
Email::Email(const string &from)
    throw ()
    : m_from(from)
{
    Debug::debug()->trace("Email::Email(%s)", from.c_str());
}

// -----------------------------------------------------------------------------
void Email::setTo(const string &recipient)
    throw ()
{
    Debug::debug()->trace("Email::setTo(%s)", recipient.c_str());

    m_to = recipient;
}

// -----------------------------------------------------------------------------
void Email::addCc(const string &cc)
    throw ()
{
    Debug::debug()->trace("Email::addCc(%s)", cc.c_str());

    m_cc.push_back(cc);
}

// -----------------------------------------------------------------------------
void Email::setSubject(const string &subject)
    throw ()
{
    Debug::debug()->trace("Email::setSubject(%s)", subject.c_str());

    m_subject = subject;
}

// -----------------------------------------------------------------------------
void Email::setBody(const string &body)
    throw ()
{
    Debug::debug()->trace("Email::setBody()");

    m_body = "\r\n" + body;
}

// -----------------------------------------------------------------------------
void Email::setHostname(const string &hostname)
    throw ()
{
    Debug::debug()->trace("Email::setHostname(%s)", hostname.c_str());

    m_hostname = hostname;

    // only host, not domain
    string::size_type dot = m_hostname.find('.');
    if (dot != string::npos) {
        m_hostname = m_hostname.substr(0, dot);
    }
}

// -----------------------------------------------------------------------------
void Email::send()
    throw (KError)
{
    Debug::debug()->trace("Email::send()");
    int ret;

    smtp_session_t session = smtp_create_session();
    smtp_message_t message = smtp_add_message(session);
    auth_context_t authctx = NULL;

    if (Debug::debug()->isDebugEnabled()) {
        ret = smtp_set_monitorcb(session, smtp_monitor_cb, NULL, false);
        if (ret == 0) {
            throw KSmtpError("Cannot set monitor callback.", smtp_errno());
        }
    }

    Configuration *config = Configuration::config();
    string host = config->KDUMP_SMTP_SERVER.value();
    Debug::debug()->dbg("Host: %s", host.c_str());

    // default to "smtp" (25) port instead of 587
    string hostname;
    size_t colonpos = host.find(':');
    if (colonpos == string::npos) {
        hostname = host;
        host += ":25";
    } else
	hostname = host.substr(0, colonpos);

    Routable rt(hostname);
    if (!rt.check(config->KDUMP_NET_TIMEOUT.value()))
	throw KError("SMTP server not reachable");

    ret = smtp_set_server(session, strdup(host.c_str()));
    if (ret == 0)
        throw KSmtpError("smtp_set_server() failed.", ret);

    // use STARTLS
    // TODO: need to investigate why that does not work
    //smtp_starttls_enable(session, Starttls_ENABLED);

    // need auth?
    if (config->KDUMP_SMTP_USER.value().size() > 0 &&
        config->KDUMP_SMTP_PASSWORD.value().size() > 0) {
        Debug::debug()->dbg("Setting ESMTP Auth callback");
        authctx = auth_create_context();
        auth_set_mechanism_flags(authctx, AUTH_PLUGIN_PLAIN, 0);
        auth_set_interact_cb(authctx, authinteract, NULL);
        smtp_auth_set_context(session, authctx);
    }

    // set hostname
    if (m_hostname.size() > 0) {
        ret = smtp_set_hostname(session, m_hostname.c_str());
        if (ret == 0) {
            throw KSmtpError("smtp_set_hostname failed.", smtp_errno());
        }
    }

    // set headers
    ret = smtp_set_header(message, "From", NULL, m_from.c_str());
    if (ret == 0)
        throw KSmtpError("smtp_set_header(From) failed.", smtp_errno());

    ret = smtp_set_header(message, "To", NULL, strdup(m_to.c_str()));
    if (ret == 0)
        throw KSmtpError("smtp_set_header(To) failed.", smtp_errno());

    ret = smtp_set_header(message, "Subject", m_subject.c_str());
    if (ret == 0)
        throw KSmtpError("smtp_set_header(Subject) failed.", smtp_errno());

    // format Cc
    for (StringVector::const_iterator it = m_cc.begin();
            it != m_cc.end(); ++it) {

        smtp_recipient_t recipient;

        // set header
        ret = smtp_set_header(message, "CC", NULL, (*it).c_str());
        Debug::debug()->dbg("smtp_set_header(%s)", (*it).c_str());
        if (ret == 0)
            throw KSmtpError("smtp_set_header(CC) failed.", smtp_errno());

        // add recipient
        recipient = smtp_add_recipient(message, (*it).c_str());
        if (!recipient)
            throw KSmtpError("smtp_add_recipient() failed.", smtp_errno());
    }

    // add To as recipient
    smtp_recipient_t recipient = smtp_add_recipient(message, strdup(m_to.c_str()));
    if (!recipient)
        throw KSmtpError("smtp_add_recipient() failed.", smtp_errno());

    // set the text callback
    ret = smtp_set_messagecb(message, smtp_messagecb, &m_body);
    if (ret == 0)
        throw KSmtpError("smtp_set_messagecb() failed.", smtp_errno());

    ret = smtp_start_session(session);
    if (ret == 0) {
        smtp_destroy_session(session);
        if (authctx) {
            auth_destroy_context(authctx);
            auth_client_exit();
        }

        throw KSmtpError("smtp_start_session() failed.", smtp_errno());
    }

    const smtp_status_t *status = smtp_message_transfer_status(message);

    if (status && status->code >= 400) {
        KString statustext = SAVE_CHARSTRING(status->text);
        int statuscode = status->code;

        // remove newlines from the status text
        statustext.rtrim("\n \t\r");

        smtp_destroy_session(session);
        if (authctx) {
            auth_destroy_context(authctx);
            auth_client_exit();
        }

        throw KError("Sending mail failed: " + statustext +
            ". (" + Stringutil::number2string(statuscode) + ")");
    }

    smtp_destroy_session(session);
    if (authctx)
        auth_destroy_context(authctx);
}

#endif // HAVE_LIBESMTP

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:
