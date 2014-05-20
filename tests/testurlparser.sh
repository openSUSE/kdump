#!/bin/bash
#
# (c) 2008, Bernhard Walle <bwalle@suse.de>, SUSE LINUX Products GmbH
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of the GNU General Public License
# as published by the Free Software Foundation; either version 2
# of the License, or (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
# 02110-1301, USA.
#

#
# Define the test URLs here                                                  {{{
#

URLS=(  "/var/log/dump"
        "ftp://bwalle@neptunium.suse.de/var/log/dump"
        "ftp://bwalle:dontsay@strauss.suse.de:123/var/log/dump"
        "ftp://192.168.0.70/var/log/dump"
        "nfs://neptunium/var/log/dump"
        "nfs://neptunium.suse.de:/var/log/dump"
        "cifs://neptunium:/var/log/dump"
        "cifs://bwalle:dontsay@neptunium:/var/log/dump"
        "smb://bwalle@192.168.0.70:/var/log"
        "cifs://bwalle:dontsay@neptunium:/var/log/dump"
	"ftp://pt%65sarik:don%27t%20say@fu%6eny+host/var/log/dum%70"
        )

# protocol:host:port:user:pass:path
VALUES=( "file::-1:::/var/log/dump"
         "ftp:neptunium.suse.de:-1:bwalle::/var/log/dump"
         "ftp:strauss.suse.de:123:bwalle:dontsay:/var/log/dump"
         "ftp:192.168.0.70:-1:anonymous::/var/log/dump"
         "nfs:neptunium:-1:::/var/log/dump"
         "nfs:neptunium.suse.de:-1:::/var/log/dump"
         "cifs:neptunium:-1:::/var/log/dump"
         "cifs:neptunium:-1:bwalle:dontsay:/var/log/dump"
         "cifs:192.168.0.70:-1:bwalle::/var/log"
         "cifs:neptunium:-1:bwalle:dontsay:/var/log/dump"
	 "ftp:funny+host:-1:ptesarik:don't say:/var/log/dump"
        )
                                                                           # }}}

#
# Program                                                                    {{{
#

URLPARSER=$1

if [ -z "$URLPARSER" ] ; then
    echo "Usage: $0 urlparser"
    exit 1
fi

i=0
errornumber=0
for url in ${URLS[@]} ; do

    result=$($URLPARSER "$url" 2>/dev/null)

    prot=$(echo "$result" | grep ^Prot | cut -f 2)
    host=$(echo "$result" | grep ^Host | cut -f 2)
    port=$(echo "$result" | grep ^Port | cut -f 2)
    user=$(echo "$result" | grep ^User | cut -f 2)
    pass=$(echo "$result" | grep ^Pass | cut -f 2)
    path=$(echo "$result" | grep ^Path | cut -f 2)

    values=${VALUES[$i]}

    wanted_prot=$(echo "$values" | cut -d ':' -f 1)
    wanted_host=$(echo "$values" | cut -d ':' -f 2)
    wanted_port=$(echo "$values" | cut -d ':' -f 3)
    wanted_user=$(echo "$values" | cut -d ':' -f 4)
    wanted_pass=$(echo "$values" | cut -d ':' -f 5)
    wanted_path=$(echo "$values" | cut -d ':' -f 6)

    if [ "$prot" != "$wanted_prot" ] ; then
        echo "$url: Protocol is $prot but should be $wanted_prot."
        errornumber=$[$errornumber+1]
    fi

    if [ "$host" != "$wanted_host" ] ; then
        echo "$url: Host is $host but should be $wanted_host."
        errornumber=$[$errornumber+1]
    fi

    if [ "$port" != "$wanted_port" ] ; then
        echo "$url: Port is $port but should be $wanted_port."
        errornumber=$[$errornumber+1]
    fi

    if [ "$user" != "$wanted_user" ] ; then
        echo "$url: User is $user but should be $wanted_user."
        errornumber=$[$errornumber+1]
    fi

    if [ "$pass" != "$wanted_pass" ] ; then
        echo "$url: Passwort is $pass but should be $wanted_pass."
        errornumber=$[$errornumber+1]
    fi

    if [ "$path" != "$wanted_path" ] ; then
        echo "$url: Path is $path but should be $wanted_path."
        errornumber=$[$errornumber+1]
    fi

    i=$[$i+1]
done


exit $errornumber

# }}}

# vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:
