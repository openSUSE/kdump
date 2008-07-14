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

cmake_minimum_required(VERSION 2.4)
set (PACKAGE_STRING "kdump")
set (PACKAGE_VERSION "0.5.0")

include_directories("${PROJECT_BINARY_DIR}")


#
# Defines
#
ADD_DEFINITIONS(-D_FILE_OFFSET_BITS=64)

#
# Find programs
#

FIND_PROGRAM(ASCIIDOC_EXECUTABLE asciidoc)
SET(ASCIIDOC_OPTIONS -a toclevels=3 -a badges -a toc -a numbered)
SET(ASCIIDOC_MAN_OPTIONS ${ASCIIDOC_OPTIONS} -d manpage)
FIND_PROGRAM(A2X_EXECUTABLE a2x)
SET(A2X_MAN_OPTIONS --doctype=manpage --format=manpage)

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

ADD_SUBDIRECTORY(doc)
ADD_SUBDIRECTORY(kdumptool)
ADD_SUBDIRECTORY(tests)


# vim: set sw=4 ts=4 et:
