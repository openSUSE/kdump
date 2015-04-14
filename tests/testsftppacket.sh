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

# Add random byte to ARG and EXPECT
#                                                                            {{{
function add_random_byte()
{
    local ch=$(( $RANDOM % 256 ))
    local hex=$( printf "%02x" $ch )
    ARG="$ARG$hex"
    EXPECT="$EXPECT $hex"
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

# TEST #4: 100 random 32-bit values
ARG=""
EXPECT="00 00 01 90"		# 4*100 in hex
i=0
while [ $i -lt 100 ]
do
    ARG="$ARG w"
    add_random_byte		# bits 0-7
    add_random_byte		# bits 8-15
    add_random_byte		# bits 16-23
    add_random_byte		# bits 24-31

    i=$(( $i + 1 ))
done
RESULT=$( "$TESTPACKET" $ARG u )
check "$ARG" "$EXPECT" "$RESULT"

# TEST #5: 100 random 64-bit values
ARG=""
EXPECT="00 00 03 20"		# 8*100 in hex
i=0
while [ $i -lt 100 ]
do
    ARG="$ARG l"
    add_random_byte		# bits 0-7
    add_random_byte		# bits 8-15
    add_random_byte		# bits 16-23
    add_random_byte		# bits 24-31
    add_random_byte		# bits 32-39
    add_random_byte		# bits 40-47
    add_random_byte		# bits 48-55
    add_random_byte		# bits 56-63

    i=$(( $i + 1 ))
done
RESULT=$( "$TESTPACKET" $ARG u )
check "$ARG" "$EXPECT" "$RESULT"

# TEST #6: String
ARG="sHello, world!"
EXPECT="00 00 00 11 00 00 00 0d 48 65 6c 6c 6f 2c 20 77 6f 72 6c 64 21"
RESULT=$( "$TESTPACKET" "$ARG" u )
check "$ARG" "$EXPECT" "$RESULT"

# TEST #7: Vector
ARG="v0123456789abcdef u"
EXPECT="00 00 00 08 01 23 45 67 89 ab cd ef"
RESULT=$( "$TESTPACKET" $ARG )
check "$ARG" "$EXPECT" "$RESULT"

# TEST #8: Get byte - note that packet length field is still zero!
ARG="v0123456789abcdef b b b b b b b b b b b b"
EXPECT=$(echo -e "00\n00\n00\n00\n01\n23\n45\n67\n89\nab\ncd\nef")
RESULT=$( "$TESTPACKET" $ARG )
check "$ARG" "$EXPECT" "$RESULT"

exit $errornumber

# }}}

# vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:
