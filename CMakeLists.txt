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
PROJECT(kdump CXX)
INCLUDE(CTest)
SET(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} ${CMAKE_SOURCE_DIR}/cmake)

#
# Configuration
#

cmake_minimum_required(VERSION 2.6.2)
set (PACKAGE_STRING "kdump")
set (PACKAGE_VERSION "0.8.11")

include_directories("${PROJECT_BINARY_DIR}")


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

# zlib
INCLUDE(FindZLIB)
SET(EXTRA_LIBS ${EXTRA_LIBS} ${ZLIB_LIBRARY})
INCLUDE_DIRECTORIES( ${ZLIB_INCLUDE_DIR} )

IF(NOT ZLIB_FOUND)
    MESSAGE(FATAL_ERROR "Zlib not found. Install zlib-devel or something like that")
ENDIF(NOT ZLIB_FOUND)

# libelf
INCLUDE(Findlibelf)
SET(EXTRA_LIBS ${EXTRA_LIBS} ${LIBELF_LIBRARIES})
INCLUDE_DIRECTORIES(${LIBELF_INCLUDE_DIRS})

IF(NOT LIBELF_FOUND)
    MESSAGE(FATAL_ERROR "libelf not found. Install libelf-devel or something like that")
ENDIF(NOT LIBELF_FOUND)

# libcurl
INCLUDE(FindCURL)
SET(EXTRA_LIBS ${EXTRA_LIBS} ${CURL_LIBRARIES})
INCLUDE_DIRECTORIES(${CURL_INCLUDE_DIRS})

IF(NOT CURL_FOUND)
    MESSAGE(FATAL_ERROR "CURL not found. Install curl-devel or something like that")
ENDIF(NOT CURL_FOUND)

# libssh2
INCLUDE(Findlibssh2)
IF (LIBSSH2_FOUND)
    SET(EXTRA_LIBS ${EXTRA_LIBS} ${LIBSSH2_LIBRARIES})
    INCLUDE_DIRECTORIES(${LIBSSH2_INCLUDE_DIRS})
ENDIF (LIBSSH2_FOUND)

IF(NOT LIBSSH2_FOUND)
    MESSAGE("libssh2 not found. Install libssh2-devel or something like that")
    MESSAGE("Building without SSH support")
    SET(LIBSSH2_FOUND FALSE)
ENDIF(NOT LIBSSH2_FOUND)

# libesmtp
INCLUDE(Findesmtp)

IF (ESMTP_FOUND)
    SET(EXTRA_LIBS ${EXTRA_LIBS} ${ESMTP_LIBRARIES})
    INCLUDE_DIRECTORIES(${ESMTP_INCLUDE_DIRS})
ENDIF (ESMTP_FOUND)

IF(NOT ESMTP_FOUND)
    MESSAGE("esmtp not found. Install libesmtp-devel or something like that")
    MESSAGE("Building without mail support!")
    SET(ESMTP_FOUND FALSE)
ENDIF(NOT ESMTP_FOUND)

# OpenSSL
INCLUDE(Findlibssl)

IF (LIBSSL_FOUND)
    SET(EXTRA_LIBS ${EXTRA_LIBS} ${LIBSSL_LIBRARIES})
    INCLUDE_DIRECTORIES(${LIBSSL_INCLUDE_DIRS})
ENDIF (LIBSSL_FOUND)

IF(NOT LIBSSL_FOUND)
    MESSAGE(
        "libssl not found. Install libopenssl-devel or something like that")
    MESSAGE("Building without OpenSSL support")
    SET(LIBSSL_FOUND FALSE)
ENDIF(NOT LIBSSL_FOUND)


#
# Check for FADUMP
#

IF(“${CMAKE_SYSTEM_PROCESSOR}” STREQUAL “ppc64”)
    SET(HAVE_FADUMP TRUE)
ELSE(“${CMAKE_SYSTEM_PROCESSOR}” STREQUAL “ppc64”)
    SET(HAVE_FADUMP FALSE)
ENDIF(“${CMAKE_SYSTEM_PROCESSOR}” STREQUAL “ppc64”)

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
ADD_SUBDIRECTORY(doc)
ADD_SUBDIRECTORY(init)
ADD_SUBDIRECTORY(tests)

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

INSTALL(FILES
    ${CMAKE_CURRENT_BINARY_DIR}/sysconfig.kdump
    DESTINATION
        /var/adm/fillup-templates/
)

INSTALL(FILES
    ${CMAKE_CURRENT_SOURCE_DIR}/70-kdump.rules
    DESTINATION
        /etc/udev/rules.d
)

#
# Source Packages
#

SET(CPACK_GENERATOR "TBZ2") # This line is need for a CMake (Version 2.4.7) Bug - Fixed in CVS
SET(CPACK_SOURCE_GENERATOR "TBZ2")
SET(CPACK_SOURCE_PACKAGE_FILE_NAME "${PACKAGE_STRING}-${PACKAGE_VERSION}")
SET(CPACK_SOURCE_IGNORE_FILES "${CPACK_SOURCE_IGNORE_FILES};/.hg;build")
INCLUDE(CPack)


# vim: set sw=4 ts=4 et:
