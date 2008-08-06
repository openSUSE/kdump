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
#include <iostream>
#include <string>
#include <strings.h>
#include <cerrno>
#include <memory>
#include <sstream>

#include "config.h"
#include "email.h"
#include "subcommand.h"
#include "debug.h"
#include "savedump.h"
#include "util.h"
#include "fileutil.h"
#include "transfer.h"
#include "configuration.h"
#include "dataprovider.h"
#include "progress.h"
#include "stringutil.h"
#include "vmcoreinfo.h"
#include "sendnotification.h"
#include "debuglink.h"

using std::string;
using std::cout;
using std::endl;
using std::auto_ptr;
using std::stringstream;

//{{{ SaveDump -----------------------------------------------------------------

// -----------------------------------------------------------------------------
SendNotification::SendNotification()
    throw ()
{
    Debug::debug()->trace("SendNotification::SaveDump()");
}

// -----------------------------------------------------------------------------
SendNotification::~SendNotification()
    throw ()
{
    Debug::debug()->trace("SendNotification::~SendNotification()");
}

// -----------------------------------------------------------------------------
const char *SendNotification::getName() const
    throw ()
{
    return "send_notification";
}

// -----------------------------------------------------------------------------
OptionList SendNotification::getOptions() const
    throw ()
{
    OptionList list;

    Debug::debug()->trace("SendNotification::getOptions()");

    list.push_back(Option("fqdn", 'q', OT_STRING,
        "Use the specified hostname/domainname instead of uname()."));

    return list;

}

// -----------------------------------------------------------------------------
void SendNotification::parseCommandline(OptionParser *optionparser)
    throw (KError)
{
    Debug::debug()->trace("SendNotification::parseCommandline(%p)", optionparser);

    if (optionparser->getValue("fqdn").getType() != OT_INVALID)
        m_hostname = optionparser->getValue("fqdn").getString();

    Debug::debug()->dbg("hostname: %s", m_hostname.c_str());
}

// -----------------------------------------------------------------------------
void SendNotification::execute()
    throw (KError)
{
    Debug::debug()->trace(__FUNCTION__);

#if !HAVE_LIBESMTP
    throw KError("Email support is not compiled-in.");
#else

    Configuration *config = Configuration::config();
    if (config->getSmtpServer().size() == 0)
        throw KError("KDUMP_SMTP_SERVER not set.");
    if (config->getNotificationTo().size() == 0)
        throw KError("No recipients specified in KDUMP_NOTIFICATION_TO.");

    if (m_hostname.size() == 0)
        m_hostname = Util::getHostDomain();

    Email email("root@" + m_hostname);
    email.setTo(config->getNotificationTo());

    if (config->getNotificationCc().size() != 0) {
        stringstream split;
        string cc;

        split << config->getNotificationCc();
        while (split >> cc) {
            Debug::debug()->dbg("Adding Cc: %s", cc.c_str());
            email.addCc(cc);
        }
    }

    stringstream ss;
    email.setSubject("[kdump] Server " + m_hostname + " crashed");

    ss << "Your server " + m_hostname + " crashed." << endl;
    ss << "Dump has been copied to " + config->getSavedir() << "." << endl;

    email.setBody(ss.str());

    email.send();


#endif // HAVE_LIBESMTP
}


//}}}

// vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:
