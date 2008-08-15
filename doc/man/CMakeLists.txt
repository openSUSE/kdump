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
# replace the version number
CONFIGURE_FILE(
    "${CMAKE_CURRENT_SOURCE_DIR}/kdump.5.txt.in"
    "${CMAKE_CURRENT_BINARY_DIR}/kdump.7.txt.in"
)
CONFIGURE_FILE(
    "${CMAKE_CURRENT_SOURCE_DIR}/kdump.7.txt.in"
    "${CMAKE_CURRENT_BINARY_DIR}/kdump.7.txt.in"
)
CONFIGURE_FILE(
    "${CMAKE_CURRENT_SOURCE_DIR}/vmcore.5.txt.in"
    "${CMAKE_CURRENT_BINARY_DIR}/vmcore.5.txt.in"
)
CONFIGURE_FILE(
    "${CMAKE_CURRENT_SOURCE_DIR}/mkdumprd.8.txt.in"
    "${CMAKE_CURRENT_BINARY_DIR}/mkdumprd.8.txt.in"
)
CONFIGURE_FILE(
    "${CMAKE_CURRENT_SOURCE_DIR}/kdumptool.8.txt.in"
    "${CMAKE_CURRENT_BINARY_DIR}/kdumptool.8.txt.in"
)

ADD_CUSTOM_COMMAND(
    OUTPUT
        ${CMAKE_CURRENT_BINARY_DIR}/kdump.7.gz
    COMMAND
        ${A2X_EXECUTABLE} ${A2X_MAN_OPTIONS}
        -D ${CMAKE_CURRENT_BINARY_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}/kdump.7.txt &&
        rm -f ${CMAKE_CURRENT_SOURCE_DIR}/kdump.7.xml &&
        gzip -f ${CMAKE_CURRENT_BINARY_DIR}/kdump.7
    DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/kdump.7.txt
    WORKING_DIRECTORY
        ${CMAKE_CURRENT_BINARY_DIR}
)


ADD_CUSTOM_COMMAND(
    OUTPUT
        ${CMAKE_CURRENT_BINARY_DIR}/kdumptool.8.gz
    COMMAND
        ${A2X_EXECUTABLE} ${A2X_MAN_OPTIONS}
        -D ${CMAKE_CURRENT_BINARY_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}/kdumptool.8.txt &&
        rm -f ${CMAKE_CURRENT_SOURCE_DIR}/kdumptool.8.xml &&
        gzip -f ${CMAKE_CURRENT_BINARY_DIR}/kdumptool.8
    DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/kdumptool.8.txt
    WORKING_DIRECTORY
        ${CMAKE_CURRENT_BINARY_DIR}
)

ADD_CUSTOM_COMMAND(
    OUTPUT
        ${CMAKE_CURRENT_BINARY_DIR}/vmcore.5.gz
    COMMAND
        ${A2X_EXECUTABLE} ${A2X_MAN_OPTIONS}
        -D ${CMAKE_CURRENT_BINARY_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}/vmcore.5.txt &&
        rm -f ${CMAKE_CURRENT_SOURCE_DIR}/vmcore.5.xml &&
        gzip -f ${CMAKE_CURRENT_BINARY_DIR}/vmcore.5
    DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/vmcore.5.txt
    WORKING_DIRECTORY
        ${CMAKE_CURRENT_BINARY_DIR}
)

ADD_CUSTOM_COMMAND(
    OUTPUT
        ${CMAKE_CURRENT_BINARY_DIR}/kdump.5.gz
    COMMAND
        ${A2X_EXECUTABLE} ${A2X_MAN_OPTIONS}
        -D ${CMAKE_CURRENT_BINARY_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}/kdump.5.txt &&
        rm -f ${CMAKE_CURRENT_SOURCE_DIR}/kdump.5.xml &&
        gzip -f ${CMAKE_CURRENT_BINARY_DIR}/kdump.5
    DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/kdump.5.txt
    WORKING_DIRECTORY
        ${CMAKE_CURRENT_BINARY_DIR}
)

ADD_CUSTOM_COMMAND(
    OUTPUT
        ${CMAKE_CURRENT_BINARY_DIR}/mkdumprd.8.gz
    COMMAND
        ${A2X_EXECUTABLE} ${A2X_MAN_OPTIONS}
        -D ${CMAKE_CURRENT_BINARY_DIR}
        ${CMAKE_CURRENT_BINARY_DIR}/mkdumprd.8.txt &&
        rm -f ${CMAKE_CURRENT_SOURCE_DIR}/mkdumprd.8.xml &&
        gzip -f ${CMAKE_CURRENT_BINARY_DIR}/mkdumprd.8
    DEPENDS
        ${CMAKE_CURRENT_SOURCE_DIR}/mkdumprd.8.txt
    WORKING_DIRECTORY
        ${CMAKE_CURRENT_BINARY_DIR}
)


#
# we need to make a target out of the two ADD_CUSTOM_COMMAND() calls
# because otherwise cmake ignores the manpages
ADD_CUSTOM_TARGET(
    manpages
    DEPENDS
        ${CMAKE_CURRENT_BINARY_DIR}/kdump.7.gz
        ${CMAKE_CURRENT_BINARY_DIR}/vmcore.5.gz
        ${CMAKE_CURRENT_BINARY_DIR}/kdump.5.gz
        ${CMAKE_CURRENT_BINARY_DIR}/kdumptool.8.gz
        ${CMAKE_CURRENT_BINARY_DIR}/mkdumprd.8.gz
)

#
# always build the manpages before building the program
ADD_DEPENDENCIES(
    kdumptool
    manpages
)


INSTALL(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/kdump.7.gz
    DESTINATION
        share/man/man7
)

INSTALL(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/kdump.5.gz
    DESTINATION
        share/man/man5
)

INSTALL(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/kdumptool.8.gz
    ${CMAKE_CURRENT_BINARY_DIR}/mkdumprd.8.gz
    DESTINATION
        share/man/man8
)


# vim: set sw=4 ts=4 et:
