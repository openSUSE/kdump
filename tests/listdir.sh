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
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
# 02110-1301, USA.
#

#
# Check if OUTPUT is equal to EXPECT
#									     {{{
function check_output()
{
    if [ "$OUTPUT" != "$EXPECT" ]; then
	echo "Expected:"
	echo "$EXPECT"
	echo
	echo "Actual output:"
	echo "$OUTPUT"
	errors=$(( $errors+1 ))
    fi
}									   # }}}

#
# Program								     {{{
#

LISTDIR=$1
DIR=$2

if [ -z "$LISTDIR" ] || [ -z "$DIR" ] ; then
    echo "Usage: $0 listdir dir"
    exit 1
fi

. "$DIR/testdirs.sh"

setup_testdir "$DIR/tmp-testdirs" || exit 1

errors=0

EXPECT=$( LANG= sort <(
	IFS=$'\n'
	echo "${TESTKDUMP[*]}"
	echo "${TESTDIRS[*]}"
	echo "${TESTFILES[*]}"
    ) )
OUTPUT=$( "$LISTDIR" "$DIR/tmp-testdirs" all || echo "*** testlistdir FAILED!" )
check_output

EXPECT=$( LANG= sort <(
	IFS=$'\n'
	echo "${TESTKDUMP[*]}"
	echo "${TESTDIRS[*]}"
    ) )
OUTPUT=$( "$LISTDIR" "$DIR/tmp-testdirs" dirs  || echo "*** testlistdir FAILED!" )
check_output

EXPECT=$( LANG= sort <(
	IFS=$'\n'
	echo "${TESTKDUMP[*]}"
    ) )
OUTPUT=$( "$LISTDIR" "$DIR/tmp-testdirs" kdumpdirs  || echo "*** testlistdir FAILED!" )
check_output

exit $errors

# }}}

# vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:
