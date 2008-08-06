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

    strncpy(userbuffer, config->getSmtpUser().c_str(), BUFSIZ);
    strncpy(passwordbuffer, config->getSmtpPassword().c_str(), BUFSIZ);

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
void Email::send()
    throw (KError)
{
    Debug::debug()->trace("Email::send()");

    smtp_session_t session = smtp_create_session();
    smtp_message_t message = smtp_add_message(session);
    auth_context_t authctx = NULL;

    Configuration *config = Configuration::config();
    string host = config->getSmtpServer();
    Debug::debug()->dbg("Host: %s", host.c_str());

    // default to "smtp" (25) port instead of 587
    if (host.find(':') == string::npos)
        host += ":25";

    int ret = smtp_set_server(session, strdup(host.c_str()));
    if (ret == 0)
        throw KSmtpError("smtp_set_server() failed.", ret);

    // use STARTLS
    // TODO: need to investigate why that does not work
    //smtp_starttls_enable(session, Starttls_ENABLED);

    // need auth?
    if (config->getSmtpUser().size() > 0 &&
                config->getSmtpPassword().size() > 0) {
        Debug::debug()->dbg("Setting ESMTP Auth callback");
        authctx = auth_create_context();
        auth_set_mechanism_flags(authctx, AUTH_PLUGIN_PLAIN, 0);
        auth_set_interact_cb(authctx, authinteract, NULL);
        smtp_auth_set_context(session, authctx);
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
          smtp_destroy_session(session);
          if (authctx) {
              auth_destroy_context(authctx);
              auth_client_exit();
          }

          throw KError("Sending mail failed: " +
              string(status->text) + ". (" +
              Stringutil::number2string(status->code) + ")");
    }

    smtp_destroy_session(session);
    if (authctx)
        auth_destroy_context(authctx);
}

#endif // HAVE_LIBESMTP

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:
