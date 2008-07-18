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

ADD_CUSTOM_COMMAND(
    OUTPUT
        ${CMAKE_CURRENT_BINARY_DIR}/Implementation_Guide.html
    COMMAND
        ${ASCIIDOC_EXECUTABLE} ${ASCIIDOC_OPTIONS}
        -o ${CMAKE_CURRENT_BINARY_DIR}/Implementation_Guide.html
        ${CMAKE_CURRENT_SOURCE_DIR}/Implementation_Guide.txt
    DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/Implementation_Guide.txt
    WORKING_DIRECTORY
        ${CMAKE_CURRENT_BINARY_DIR}
)

#
# we need to make a target out of the two ADD_CUSTOM_COMMAND() calls
# because otherwise cmake ignores the manpages
ADD_CUSTOM_TARGET(
    implementation_doc
    DEPENDS
        ${CMAKE_CURRENT_BINARY_DIR}/Implementation_Guide.html
)

#
# always build the manpages before building the program
ADD_DEPENDENCIES(
    kdumptool
    implementation_doc
)

# vim: set sw=4 ts=4 et:
