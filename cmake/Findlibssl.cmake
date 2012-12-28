# - Try to find libssl
# Once done this will define
#
#  LIBSSL_FOUND - system has OpenSSL
#  LIBSSL_INCLUDE_DIR - the OpenSSL include directory
#  LIBSSL_LIBRARIES - Link these to use OpenSSL
#
#  Copyright (c) 2008 Bernhard Walle <bwalle@suse.de>
#  Copyright (c) 2011 Petr Tesarik <ptesarik@suse.cz>
#
#  Redistribution and use is allowed according to the terms of the New
#  BSD license.
#  For details see the accompanying COPYING-CMAKE-SCRIPTS file.
#


if (LIBSSL_INCLUDE_DIR AND LIBSSL_LIBRARIES)
  # in cache already
  set(LIBSSL_FOUND TRUE)
else (LIBSSL_INCLUDE_DIR AND LIBSSL_LIBRARIES)
  find_path(LIBSSL_INCLUDE_DIR
    NAMES
      openssl/ssl.h
    PATHS
      /usr/include
      /usr/local/include
      /opt/local/include
      /sw/include
  )

  find_library(LIBSSL_LIBRARY
    NAMES
      ssl
    PATHS
      /usr/lib
      /usr/local/lib
      /opt/local/lib
      /sw/lib
  )
  mark_as_advanced(LIBSSL_LIBRARY)

  find_library(LIBCRYPTO_LIBRARY
    NAMES
      crypto
    PATHS
      /usr/lib
      /usr/local/lib
      /opt/local/lib
      /sw/lib
  )
  mark_as_advanced(LIBCRYPTO_LIBRARY)

  if (LIBSSL_LIBRARY AND LIBCRYPTO_LIBRARY)
    set(LIBSSL_LIBRARIES
      ${LIBSSL_LIBRARIES}
      ${LIBSSL_LIBRARY}
      ${LIBCRYPTO_LIBRARY}
    )
  endif (LIBSSL_LIBRARY AND LIBCRYPTO_LIBRARY)

  if (LIBSSL_INCLUDE_DIR AND LIBSSL_LIBRARIES)
     set(LIBSSL_FOUND TRUE)
  endif (LIBSSL_INCLUDE_DIR AND LIBSSL_LIBRARIES)

  if (LIBSSL_FOUND)
    if (NOT libssl_FIND_QUIETLY)
      message(STATUS "Found libssl: ${LIBSSL_LIBRARIES}")
    endif (NOT libssl_FIND_QUIETLY)
  else (LIBSSL_FOUND)
    if (libssl_FIND_REQUIRED)
      message(FATAL_ERROR "Could not find libssl")
    endif (libssl_FIND_REQUIRED)
  endif (LIBSSL_FOUND)

  # show the LIBSSL_INCLUDE_DIR and LIBSSL_LIBRARIES variables only in the advanced view
  mark_as_advanced(LIBSSL_INCLUDE_DIR LIBSSL_LIBRARIES)

endif(LIBSSL_INCLUDE_DIR AND LIBSSL_LIBRARIES)

