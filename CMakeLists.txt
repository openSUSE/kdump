PROJECT(kdump CXX)

#
# Configuration
#

cmake_minimum_required(VERSION 2.4)
set (PACKAGE_STRING "kdump")
set (PACKAGE_VERSION "0.5.0")

#
# Find programs
#

FIND_PROGRAM(ASCIIDOC_EXECUTABLE asciidoc)
SET(ASCIIDOC_OPTIONS -a toclevels=3 -a badges -a toc -a numbered)

#
# Subdirs
#

ADD_SUBDIRECTORY(doc)


# vim: set sw=4 ts=4 et:
