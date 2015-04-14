#!/bin/bash
#
# (c) 2014, Petr Tesarik <ptesarik@suse.de>, SUSE LINUX Products GmbH
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

# Check that results match expectation
#                                                                            {{{
function check()
{
    local arg="$1"
    local expect="$2"
    local result="$3"
    if [ "$result" != "$expect" ] ; then
	echo "failed packet: $arg"
	echo "Expected:"
	echo "$expect"
	echo "Result:"
	echo "$result"
	errornumber=$(( errornumber + 1 ))
    fi
}
# }}}

#
# Program                                                                    {{{
#

TESTPACKET=$1
HEXDUMP="od -Ax -tx1"

if [ -z "$TESTPACKET" ] ; then
    echo "Usage: $0 testsftppacket"
    exit 1
fi

errornumber=0

# TEST #1: Empty packet

ARG="d"
EXPECT="00 00 00 00"
RESULT=$( "$TESTPACKET" $ARG )
check "$ARG" "$EXPECT" "$RESULT"

# TEST #2: Updated empty packet

ARG="u"
EXPECT="00 00 00 00"
RESULT=$( "$TESTPACKET" $ARG )
check "$ARG" "$EXPECT" "$RESULT"

# TEST #3: Any 8-bit value
ARG=""
EXPECT="00 00 01 00"
i=0
while [ $i -le 255 ]
do
    ARG="$ARG "$(printf "b%02x" $i)
    EXPECT="$EXPECT "$(printf "%02x" $i)
    i=$(( $i + 1 ))
done
RESULT=$( "$TESTPACKET" $ARG u )
check "$ARG" "$EXPECT" "$RESULT"

exit $errornumber

# }}}

# vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:
