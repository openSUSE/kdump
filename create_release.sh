#!/bin/sh
# (c) 2008, Bernhard Walle <bwalle@suse.de>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 2 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program. If not, see <http://www.gnu.org/licenses/>.
#

usage()
{
    echo "usage: create_release.sh [--notest] [comittish]" >&2
}

notest=
commit=
for opt in "$@"; do
    case "$opt" in
        --notest)
            notest=y
            ;;
        -*)
            echo "Invalid option: $opt" >&2
            usage
            exit 1
            ;;
        *)
            if [ -n "$commit" ] ; then
                usage
                exit 1
            fi
            commit="$opt"
            ;;
    esac
done
test -z "$commit" && commit=HEAD

VERSION=$(git describe "$commit")
test $? -eq 0 || exit 1

# remove leading "v" from the tag
VERSION="${VERSION#v}"

TARBALL=kdump-${VERSION}.tar.bz2

#
# Generate tarball
#
git archive --format=tar --prefix="kdump-${VERSION}/" "$commit" | \
    bzip2 -c >$TARBALL

#
# Test build
#
if [ -z "$notest" ] ; then
    dir=$PWD
    TEMPBUILD=$PWD/tempbuild-$$
    mkdir $TEMPBUILD
    cd $TEMPBUILD
    tar xf ../$TARBALL
    mkdir build
    cmake kdump-${VERSION}
    make -j2
    cd $dir
    rm -fr $TEMPBUILD
fi

echo "-------------------------------"
echo "$TARBALL ready."
echo "-------------------------------"

exit 0
