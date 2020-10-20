# Copyright © 2020  Stefano Marsili, <stemars@gmx.ch>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public
# License along with this program; if not, see <http://www.gnu.org/licenses/>

# File:   test/CMakeLists.txt

include(CommonTesting)

option(BUILD_TESTING "Build tests" OFF)

if (BUILD_TESTING)


    # required fspropfaker version
    set(SONOREM_REQ_FSPROPFAKER_MAJOR_VERSION 0)
    set(SONOREM_REQ_FSPROPFAKER_MINOR_VERSION 1) # !-U-!
    set(SONOREM_REQ_FSPROPFAKER_VERSION "${SONOREM_REQ_FSPROPFAKER_MAJOR_VERSION}.${SONOREM_REQ_FSPROPFAKER_MINOR_VERSION}")

    include(FindPkgConfig)
    if (NOT PKG_CONFIG_FOUND)
        message(FATAL_ERROR "Mandatory 'pkg-config' not found!")
    endif()
    # Beware! The prefix passed to pkg_check_modules(PREFIX ...) shouldn't contain underscores!
    pkg_check_modules(FSPROPFAKERPKG   REQUIRED  fspropfaker>=${SONOREM_REQ_FSPROPFAKER_VERSION})

    # Test dirs
    set(STMMI_TEST_SOURCES_DIR  "${PROJECT_SOURCE_DIR}/test")

    set(STMMI_TEST_WITH_SOURCES_MODEL
            #"${STMMI_TEST_SOURCES_DIR}/testingcommon.h"
            "${PROJECT_SOURCE_DIR}/src/rfkill.h"
            "${PROJECT_SOURCE_DIR}/src/rfkill.cc"
            "${PROJECT_SOURCE_DIR}/src/sonomodel.h"
            "${PROJECT_SOURCE_DIR}/src/sonomodel.cc"
            "${PROJECT_SOURCE_DIR}/src/sonosources.h"
            "${PROJECT_SOURCE_DIR}/src/sonosources.cc"
            "${PROJECT_SOURCE_DIR}/src/util.h"
            "${PROJECT_SOURCE_DIR}/src/util.cc"
            "${STMMI_TEST_SOURCES_DIR}/testutil.h"
            "${STMMI_TEST_SOURCES_DIR}/testutil.cc"
            "${STMMI_TEST_SOURCES_DIR}/mainloopfixture.h"
            "${STMMI_TEST_SOURCES_DIR}/mainloopfixture.cc"
           )
    # Test sources should end with .cxx
    set(STMMI_TEST_SOURCES_MODEL
            "${STMMI_TEST_SOURCES_DIR}/testWaitingState.cxx"
           )

    TestFiles("${STMMI_TEST_SOURCES_MODEL}"
                "${STMMI_TEST_WITH_SOURCES_MODEL}"
                "${FSPROPFAKERPKG_INCLUDE_DIRS};${SONOREM_EXTRA_INCLUDE_DIRS};${PROJECT_SOURCE_DIR}/src"
                "${FSPROPFAKERPKG_LIBRARIES};${SONOREM_EXTRA_LIBRARIES}"
                FALSE)

    include(CTest)
endif()
