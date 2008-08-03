# - Try to find esmtp
# Once done this will define
#
#  ESMTP_FOUND - system has esmtp
#  ESMTP_INCLUDE_DIRS - the esmtp include directory
#  ESMTP_LIBRARIES - Link these to use esmtp
#  ESMTP_DEFINITIONS - Compiler switches required for using esmtp
#
#  Copyright (c) 2008 Bernhard Walle <bwalle@suse.de>
#
#  Redistribution and use is allowed according to the terms of the New
#  BSD license.
#  For details see the accompanying COPYING-CMAKE-SCRIPTS file.
#


if (ESMTP_LIBRARIES AND ESMTP_INCLUDE_DIRS)
  # in cache already
  set(ESMTP_FOUND TRUE)
else (ESMTP_LIBRARIES AND ESMTP_INCLUDE_DIRS)
  find_path(ESMTP_INCLUDE_DIR
    NAMES
      libesmtp.h
    PATHS
      /usr/include
      /usr/local/include
      /opt/local/include
      /sw/include
  )

  find_library(ESMTP_LIBRARY
    NAMES
      esmtp
    PATHS
      /usr/lib
      /usr/local/lib
      /opt/local/lib
      /sw/lib
  )
  mark_as_advanced(ESMTP_LIBRARY)

  if (ESMTP_LIBRARY)
    set(ESMTP_FOUND TRUE)
  endif (ESMTP_LIBRARY)

  set(ESMTP_INCLUDE_DIRS
    ${ESMTP_INCLUDE_DIR}
  )

  if (ESMTP_FOUND)
    set(ESMTP_LIBRARIES
      ${ESMTP_LIBRARIES}
      ${ESMTP_LIBRARY}
    )
  endif (ESMTP_FOUND)

  if (ESMTP_INCLUDE_DIRS AND ESMTP_LIBRARIES)
     set(ESMTP_FOUND TRUE)
  endif (ESMTP_INCLUDE_DIRS AND ESMTP_LIBRARIES)

  if (ESMTP_FOUND)
    if (NOT esmtp_FIND_QUIETLY)
      message(STATUS "Found esmtp: ${ESMTP_LIBRARIES}")
    endif (NOT esmtp_FIND_QUIETLY)
  else (ESMTP_FOUND)
    if (esmtp_FIND_REQUIRED)
      message(FATAL_ERROR "Could not find esmtp")
    endif (esmtp_FIND_REQUIRED)
  endif (ESMTP_FOUND)

  # show the ESMTP_INCLUDE_DIRS and ESMTP_LIBRARIES variables only in the advanced view
  mark_as_advanced(ESMTP_INCLUDE_DIR ESMTP_INCLUDE_DIRS ESMTP_LIBRARIES)

endif (ESMTP_LIBRARIES AND ESMTP_INCLUDE_DIRS)

