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
# Define kdump directories here						     {{{
TESTKDUMP=(	"2014-03-27-10:37"
		"2013-12-31-23:59"
		"2014-03-05-12:15"
)									   # }}}

#
# Define non-kdump directories here					     {{{
TESTDIRS=(	"2014-03-26-18:15"
		"unrelated"
		"app-dump"
)									   # }}}

#
# Define test files here						     {{{
TESTFILES=(	"beta"
		"99-last"
		"gamma"
		"UpperCase"
		"123-digit"
)									   # }}}

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

rm -rf "$DIR/tmp"
mkdir "$DIR/tmp"

for tdir in "${TESTKDUMP[@]}" "${TESTDIRS[@]}"; do
    mkdir -p "$DIR/tmp/$tdir" || exit 1
done

for tdir in "${TESTKDUMP[@]}"; do
    touch "$DIR/tmp/$tdir/vmcore" || exit 1
done

for f in "${TESTFILES[@]}"; do
    touch "$DIR/tmp/$f" || exit 1
done

errors=0

EXPECT=$( LANG= sort <(
	IFS=$'\n'
	echo "${TESTKDUMP[*]}"
	echo "${TESTDIRS[*]}"
	echo "${TESTFILES[*]}"
    ) )
OUTPUT=$( "$LISTDIR" "$DIR/tmp" all || echo "*** testlistdir FAILED!" )
check_output

EXPECT=$( LANG= sort <(
	IFS=$'\n'
	echo "${TESTKDUMP[*]}"
	echo "${TESTDIRS[*]}"
    ) )
OUTPUT=$( "$LISTDIR" "$DIR/tmp" dirs  || echo "*** testlistdir FAILED!" )
check_output

EXPECT=$( LANG= sort <(
	IFS=$'\n'
	echo "${TESTKDUMP[*]}"
    ) )
OUTPUT=$( "$LISTDIR" "$DIR/tmp" kdumpdirs  || echo "*** testlistdir FAILED!" )
check_output

exit $errors

# }}}

# vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:
