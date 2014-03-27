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

KDUMPTOOL=$1
DIR=$2

if [ -z "$KDUMPTOOL" ] || [ -z "$DIR" ] ; then
    echo "Usage: $0 kdumptool dir"
    exit 1
fi

. "$DIR/testdirs.sh"

setup_testdir "$DIR/tmp" || exit 1
SORTDUMPS=$( LANG= sort <(
	IFS=$'\n'
	echo "${TESTKDUMP[*]}"
    ) )

errors=0

CONF="$DIR/tmp/kdump.conf"

echo "Disable delete_dumps"
cat <<EOF >"$CONF"
KDUMP_SAVEDIR="file:///$DIR/tmp"
KDUMP_KEEP_OLD_DUMPS=0
EOF

"$KDUMPTOOL" -F "$CONF" delete_dumps || exit 1
for f in "${TESTKDUMP[@]}" "${TESTDIRS[@]}" "${TESTFILES[@]}"; do
    if ! test -e "$DIR/tmp/$f"; then
	echo "$f incorrectly deleted!" >&2
	errors=$(( $errors+1 ))
    fi
done

echo "Keep all existing dumps"
nkeep=${#TESTKDUMP[@]}
cat <<EOF >"$CONF"
KDUMP_SAVEDIR="file:///$DIR/tmp"
KDUMP_KEEP_OLD_DUMPS=$nkeep
EOF

"$KDUMPTOOL" -F "$CONF" delete_dumps || exit 1

for f in "${TESTKDUMP[@]}" "${TESTDIRS[@]}" "${TESTFILES[@]}"; do
    if ! test -e "$DIR/tmp/$f"; then
	echo "$f incorrectly deleted!" >&2
	errors=$(( $errors+1 ))
    fi
done

echo "Keep all but one dump"

nkeep=$(( ${#TESTKDUMP[@]} - 1 ))
cat <<EOF >"$CONF"
KDUMP_SAVEDIR="file:///$DIR/tmp"
KDUMP_KEEP_OLD_DUMPS=$nkeep
EOF

"$KDUMPTOOL" -F "$CONF" delete_dumps || exit 1

save_ifs=IFS
IFS=$'\n'
KEPTDUMP=( $( echo "$SORTDUMPS" | tail -n $nkeep ) )
REMOVEDUMP=( $( echo "$SORTDUMPS" | head -n 1 ) )
IFS=save_ifs

for f in "${KEPTDUMP[@]}" "${TESTDIRS[@]}" "${TESTFILES[@]}"; do
    if ! test -e "$DIR/tmp/$f"; then
	echo "$f incorrectly deleted!" >&2
	errors=$(( $errors+1 ))
    fi
done

for f in "${REMOVEDUMP[@]}"; do
    if test -e "$DIR/tmp/$f"; then
	echo "$f incorrectly kept!" >&2
	errors=$(( $errors+1 ))
    fi
done

echo "Keep only 1 dump"

nkeep=1
cat <<EOF >"$CONF"
KDUMP_SAVEDIR="file:///$DIR/tmp"
KDUMP_KEEP_OLD_DUMPS=1
EOF

"$KDUMPTOOL" -F "$CONF" delete_dumps || exit 1

save_ifs=IFS
IFS=$'\n'
KEPTDUMP=( $( echo "$SORTDUMPS" | tail -n 1 ) )
REMOVEDUMP=( $( echo "$SORTDUMPS" | head -n $(( ${#TESTKDUMP[@]} - 1)) ) )
IFS=save_ifs

for f in "${KEPTDUMP[@]}" "${TESTDIRS[@]}" "${TESTFILES[@]}"; do
    if ! test -e "$DIR/tmp/$f"; then
	echo "$f incorrectly deleted!" >&2
	errors=$(( $errors+1 ))
    fi
done

for f in "${REMOVEDUMP[@]}"; do
    if test -e "$DIR/tmp/$f"; then
	echo "$f incorrectly kept!" >&2
	errors=$(( $errors+1 ))
    fi
done

echo "Delete all dumps"

nkeep=1
cat <<EOF >"$CONF"
KDUMP_SAVEDIR="file:///$DIR/tmp"
KDUMP_KEEP_OLD_DUMPS=-1
EOF

"$KDUMPTOOL" -F "$CONF" delete_dumps || exit 1

for f in "${TESTDIRS[@]}" "${TESTFILES[@]}"; do
    if ! test -e "$DIR/tmp/$f"; then
	echo "$f incorrectly deleted!" >&2
	errors=$(( $errors+1 ))
    fi
done

for f in "${TESTKDUMP[@]}"; do
    if test -e "$DIR/tmp/$f"; then
	echo "$f incorrectly kept!" >&2
	errors=$(( $errors+1 ))
    fi
done

exit $errors

# }}}

# vim: set sw=4 ts=4 fdm=marker et: :collapseFolds=1:
