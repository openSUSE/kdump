# - Try to find libssh2
# Once done this will define
#
#  LIBSSH2_FOUND - system has libssh2
#  LIBSSH2_INCLUDE_DIRS - the libssh2 include directory
#  LIBSSH2_LIBRARIES - Link these to use libssh2
#  LIBSSH2_DEFINITIONS - Compiler switches required for using libssh2
#
#  Copyright (c) 2008 Bernhard Walle <bwalle@suse.de>
#
#  Redistribution and use is allowed according to the terms of the New
#  BSD license.
#  For details see the accompanying COPYING-CMAKE-SCRIPTS file.
#


if (LIBSSH2_LIBRARIES AND LIBSSH2_INCLUDE_DIRS)
  # in cache already
  set(LIBSSH2_FOUND TRUE)
else (LIBSSH2_LIBRARIES AND LIBSSH2_INCLUDE_DIRS)
  find_path(LIBSSH2_INCLUDE_DIR
    NAMES
      libssh2.h
    PATHS
      /usr/include
      /usr/local/include
      /opt/local/include
      /sw/include
  )

  find_library(LIBSSH2_LIBRARY
    NAMES
      ssh2
    PATHS
      /usr/lib
      /usr/local/lib
      /opt/local/lib
      /sw/lib
  )
  mark_as_advanced(LIBSSH2_LIBRARY)

  if (LIBSSH2_LIBRARY)
    set(LIBSSH2_FOUND TRUE)
  endif (LIBSSH2_LIBRARY)

  set(LIBSSH2_INCLUDE_DIRS
    ${LIBSSH2_INCLUDE_DIR}
  )

  if (LIBSSH2_FOUND)
    set(LIBSSH2_LIBRARIES
      ${LIBSSH2_LIBRARIES}
      ${LIBSSH2_LIBRARY}
    )
  endif (LIBSSH2_FOUND)

  if (LIBSSH2_INCLUDE_DIRS AND LIBSSH2_LIBRARIES)
     set(LIBSSH2_FOUND TRUE)
  endif (LIBSSH2_INCLUDE_DIRS AND LIBSSH2_LIBRARIES)

  if (LIBSSH2_FOUND)
    if (NOT libssh2_FIND_QUIETLY)
      message(STATUS "Found libssh2: ${LIBSSH2_LIBRARIES}")
    endif (NOT libssh2_FIND_QUIETLY)
  else (LIBSSH2_FOUND)
    if (libssh2_FIND_REQUIRED)
      message(FATAL_ERROR "Could not find libssh2")
    endif (libssh2_FIND_REQUIRED)
  endif (LIBSSH2_FOUND)

  # show the LIBSSH2_INCLUDE_DIRS and LIBSSH2_LIBRARIES variables only in the advanced view
  mark_as_advanced(LIBSSH2_INCLUDE_DIRS LIBSSH2_LIBRARIES)

endif (LIBSSH2_LIBRARIES AND LIBSSH2_INCLUDE_DIRS)

