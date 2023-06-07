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
PROJECT(kdump CXX C)
INCLUDE(CTest)
INCLUDE(FindPkgConfig)
SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake)

#
# Configuration
#

cmake_minimum_required(VERSION 3.7)
set (PACKAGE_STRING "kdump")
set (PACKAGE_VERSION "1.0.2")

include_directories("${PROJECT_BINARY_DIR}")

option(CALIBRATE "Run QEMU to calibrate the build host" ON)

#
# Defines
#
ADD_DEFINITIONS(-D_FILE_OFFSET_BITS=64)
ADD_DEFINITIONS(-Wall -W -Wno-unused-parameter)

#
# Find programs
#

FIND_PROGRAM(ASCIIDOC_EXECUTABLE asciidoc)
SET(ASCIIDOC_OPTIONS -a toclevels=3 -a badges -a toc -a numbered)
SET(ASCIIDOC_MAN_OPTIONS ${ASCIIDOC_OPTIONS} -d manpage)
FIND_PROGRAM(A2X_EXECUTABLE a2x)

# get the version number from asciidoc
EXECUTE_PROCESS(
    COMMAND
        asciidoc --version
    OUTPUT_VARIABLE
        asciidoc_version_string
)
string(REGEX REPLACE "asciidoc (.*)\n" "\\1"
    asciidoc_version_string "${asciidoc_version_string}")

#
# Use -L for asciidoc >= 8.3.0
#
IF (asciidoc_version_string VERSION_LESS 8.3.0)
    SET(A2X_MAN_OPTIONS --doctype=manpage --format=manpage)
ELSE (asciidoc_version_string VERSION_LESS 8.3.0)
    SET(A2X_MAN_OPTIONS -L --doctype=manpage --format=manpage)
ENDIF (asciidoc_version_string VERSION_LESS 8.3.0)

#
# Find libraries
#
set (CMAKE_LIBRARY_PATH /usr/lib64 ${CMAKE_LIBRARY_PATH})


# libblkid
pkg_check_modules(BLKID REQUIRED blkid)


#
# Check for FADUMP
#

IF("${CMAKE_SYSTEM_PROCESSOR}" MATCHES "ppc64(le)?")
    SET(HAVE_FADUMP TRUE)
ELSE()
    SET(HAVE_FADUMP FALSE)
ENDIF()

#
# Configure file
#

configure_file(
    "${PROJECT_SOURCE_DIR}/config.h.in"
    "${PROJECT_BINARY_DIR}/config.h"
)

#
# Subdirs
#

ADD_SUBDIRECTORY(kdumptool)
ADD_SUBDIRECTORY(dracut)
ADD_SUBDIRECTORY(calibrate)
ADD_SUBDIRECTORY(doc)
ADD_SUBDIRECTORY(init)

#
# Configuration and Udev
#
ADD_CUSTOM_COMMAND(
    OUTPUT
        sysconfig.kdump
    COMMAND
        HAVE_FADUMP=${HAVE_FADUMP}
        awk -f ${CMAKE_CURRENT_SOURCE_DIR}/process_cond.awk
        ${CMAKE_CURRENT_SOURCE_DIR}/sysconfig.kdump.in
        > ${CMAKE_CURRENT_BINARY_DIR}/sysconfig.kdump
    DEPENDS
        sysconfig.kdump.in
)
ADD_CUSTOM_TARGET(
    sysconfig
    ALL
    DEPENDS
        sysconfig.kdump
)

INSTALL (
    FILES
        ${CMAKE_CURRENT_SOURCE_DIR}/kdump-read-config.sh
    DESTINATION
        /usr/lib/kdump
    PERMISSIONS
        OWNER_READ OWNER_WRITE OWNER_EXECUTE
        GROUP_READ GROUP_EXECUTE
        WORLD_READ WORLD_EXECUTE
)

INSTALL(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/sysconfig.kdump
    DESTINATION
        /usr/share/fillup-templates/
)

ADD_CUSTOM_COMMAND(
    OUTPUT
        70-kdump.rules
    COMMAND
        ARCH=${CMAKE_SYSTEM_PROCESSOR}
        awk -f ${CMAKE_CURRENT_SOURCE_DIR}/process_cond.awk
        ${CMAKE_CURRENT_SOURCE_DIR}/70-kdump.rules.in
        > ${CMAKE_CURRENT_BINARY_DIR}/70-kdump.rules
    DEPENDS
        70-kdump.rules.in
)
ADD_CUSTOM_TARGET(
    kdump.rules
    ALL
    DEPENDS
        70-kdump.rules
)

INSTALL(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/70-kdump.rules
    DESTINATION
        /usr/lib/kdump
)

IF("${CMAKE_SYSTEM_PROCESSOR}" MATCHES "ppc64(le)?")
    INSTALL(
        FILES
            ${CMAKE_CURRENT_SOURCE_DIR}/kdump-migrate-action.sh
        DESTINATION
            /usr/lib/kdump
        PERMISSIONS
            OWNER_READ OWNER_WRITE OWNER_EXECUTE
            GROUP_READ GROUP_EXECUTE
            WORLD_READ WORLD_EXECUTE
    )
ENDIF()

#
# Source Packages
#

SET(CPACK_GENERATOR "TBZ2") # This line is need for a CMake (Version 2.4.7) Bug - Fixed in CVS
SET(CPACK_SOURCE_GENERATOR "TBZ2")
SET(CPACK_SOURCE_PACKAGE_FILE_NAME "${PACKAGE_STRING}-${PACKAGE_VERSION}")
SET(CPACK_SOURCE_IGNORE_FILES "${CPACK_SOURCE_IGNORE_FILES};/.hg;build")
INCLUDE(CPack)


# vim: set sw=4 ts=4 et:
