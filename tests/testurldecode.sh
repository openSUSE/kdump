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
	echo "failed string: $arg"
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

# number of tested characters for random tests
NTEST=100

URLDECODE=$1
HEXDUMP="od -Ax -tx1"

if [ -z "$URLDECODE" ] ; then
    echo "Usage: $0 urldecode"
    exit 1
fi

errornumber=0

# TEST #1: Random uppercase %-sequences

ARG=
EXPECT=
i=0
while [ $i -lt $NTEST ]
do
    ch=$(( $RANDOM % 256 ))
    hex=$( printf "%02X" $ch )
    ARG="$ARG%$hex"
    EXPECT="$EXPECT\\x$hex"
    i=$(( i + 1 ))
done
EXPECT=$( echo -e "$EXPECT" | $HEXDUMP )
RESULT=$( "$URLDECODE" "$ARG" | $HEXDUMP )
check "$ARG" "$EXPECT" "$RESULT"

# TEST #2: Random lowercase %-sequences

ARG=
EXPECT=
i=0
while [ $i -lt $NTEST ]
do
    ch=$(( $RANDOM % 256 ))
    hex=$( printf "%02x" $ch )
    ARG="$ARG%$hex"
    EXPECT="$EXPECT\\x$hex"
    i=$(( i + 1 ))
done
EXPECT=$( echo -e "$EXPECT" | $HEXDUMP )
RESULT=$( "$URLDECODE" "$ARG" | $HEXDUMP )
check "$ARG" "$EXPECT" "$RESULT"

# TEST #3: Decoding '+'

ARG="   + +begin hello+world end+"
EXPECT="+ +begin hello+world end+"
RESULT=$( "$URLDECODE" $ARG )
check "$ARG" "$EXPECT" "$RESULT"

# now using form decoding
EXPECT="   begin hello world end "
RESULT=$( "$URLDECODE" -f $ARG )
check "$ARG" "$EXPECT" "$RESULT"

# TEST #4: Transitions quoted <-> unquoted

ARG=
EXPECT=
i=0
while [ $i -lt $NTEST ]
do
    ch=$(( $RANDOM % 256 ))
    hex=$( printf "%02x" $ch )
    case $(( $ch % 3 )) in
	0)
	    ARG="$ARG $hex%$hex"
	    EXPECT="$EXPECT $hex\\x$hex"
	    ;;

	1)
	    ARG="$ARG %$hex$hex"
	    EXPECT="$EXPECT \\x$hex$hex"
	    ;;

	2)
	    ARG="$ARG $hex%$hex$hex"
	    EXPECT="$EXPECT $hex\\x$hex$hex"
	    ;;

	3)
	    ARG="$ARG +%$hex+$hex"
	    EXPECT="$EXPECT  \\x$hex $hex"
	    ;;
    esac
    i=$(( i + 1 ))
done
EXPECT=$( echo -e "$EXPECT" | $HEXDUMP )
RESULT=$( "$URLDECODE" "$ARG" | $HEXDUMP )
check "$ARG" "$EXPECT" "$RESULT"

# TEST #5: Test invalid sequence

ARG="   %invalid %0invalid %ainvalid %x0invalid %+invalid %0+invalid"
EXPECT="%invalid %0invalid %ainvalid %x0invalid % invalid %0 invalid"
RESULT=$( "$URLDECODE" -f $ARG )
check "$ARG" "$EXPECT" "$RESULT"

# TEST #6: More than two hex digits

ARG="%4567 %456789abcdef0123"
EXPECT="E67 E6789abcdef0123"
RESULT=$( "$URLDECODE" $ARG )
check "$ARG" "$EXPECT" "$RESULT"

# TEST #7: NUL character

ARG="NUL character here: '%00'"
EXPECT=$( echo -e "NUL character here: '\000'" | $HEXDUMP )
RESULT=$( "$URLDECODE" $ARG | $HEXDUMP )
check "$ARG" "$EXPECT" "$RESULT"

exit $errornumber

# }}}

# vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:
