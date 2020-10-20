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

# File:   CMakeLists.txt

cmake_minimum_required(VERSION 3.0)

option(STMM_INSTALL_MAN_PAGE "Install man page in debian.orig/" ON)
option(STMM_INSTALL_LAUNCHER "Install launcher in share/applications/ (implies STMM_INSTALL_ICONS=ON)" ON)
option(STMM_INSTALL_ICONS "Install icons in share/icons/hicolor/(size)/apps/" ON)


project(sonorem CXX)

set(RUNTIME_OUTPUT_DIRECTORY "build")

set(CMAKE_MODULE_PATH ${CMAKE_MODULE_PATH} "${CMAKE_CURRENT_SOURCE_DIR}/share/cmake")

include(CommonUtil)

CheckBinaryNotSourceTree()
CheckBuildType()
DefineCommonCompileOptions("c++14")

# Source files (and headers only used for building)
set(STMMI_SNRM_SOURCES
        "${PROJECT_SOURCE_DIR}/src/config.h"
        "${PROJECT_SOURCE_DIR}/src/debugctx.h"
        "${PROJECT_SOURCE_DIR}/src/debugctx.cc"
        "${PROJECT_SOURCE_DIR}/src/evalargs.h"
        "${PROJECT_SOURCE_DIR}/src/evalargs.cc"
        "${PROJECT_SOURCE_DIR}/src/main.cc"
        "${PROJECT_SOURCE_DIR}/src/rfkill.h"
        "${PROJECT_SOURCE_DIR}/src/rfkill.cc"
        "${PROJECT_SOURCE_DIR}/src/sonodevicemanager.h"
        "${PROJECT_SOURCE_DIR}/src/sonodevicemanager.cc"
        "${PROJECT_SOURCE_DIR}/src/sonomodel.h"
        "${PROJECT_SOURCE_DIR}/src/sonomodel.cc"
        "${PROJECT_SOURCE_DIR}/src/sonosources.h"
        "${PROJECT_SOURCE_DIR}/src/sonosources.cc"
        "${PROJECT_SOURCE_DIR}/src/sonowindow.h"
        "${PROJECT_SOURCE_DIR}/src/sonowindow.cc"
        "${PROJECT_SOURCE_DIR}/src/util.h"
        "${PROJECT_SOURCE_DIR}/src/util.cc"
        )

set(STMMI_SNRM_DATA_DIR ${PROJECT_SOURCE_DIR}/data)
set(STMMI_DEVFLO_DATA_FILES
        #"${STMMI_SNRM_DATA_DIR}/sounds/srart.mp3"
        )

add_executable(sonorem ${STMMI_SNRM_SOURCES} "${PROJECT_BINARY_DIR}/config.cc")

include("sonorem-defs.cmake")

target_include_directories(sonorem SYSTEM PUBLIC ${SONOREM_EXTRA_INCLUDE_DIRS})
# This allows config.cc to find the config.h include
target_include_directories(sonorem        PUBLIC ${PROJECT_SOURCE_DIR}/src)
target_include_directories(sonorem        PUBLIC "share/thirdparty")

target_link_libraries(sonorem ${SONOREM_EXTRA_LIBRARIES})

DefineTargetPublicCompileOptions(sonorem)

include(GNUInstallDirs)
set(SONOREM_PKG_DATA_DIR "${CMAKE_INSTALL_FULL_DATADIR}/sonorem")
set(SONOREM_PKG_REL_DATA_DIR  "${CMAKE_INSTALL_DATADIR}/sonorem")

# Create config file for executable
configure_file("${PROJECT_SOURCE_DIR}/src/config.cc.in"
               "${PROJECT_BINARY_DIR}/config.cc" @ONLY)
if (STMM_INSTALL_LAUNCHER)
    set(SONOREM_INSTALLED_PATH "${CMAKE_INSTALL_FULL_BINDIR}/sonorem")
endif()
if (STMM_INSTALL_MAN_PAGE)
    configure_file("${PROJECT_SOURCE_DIR}/scripts/priv/debian.orig/sonorem.1"
                   "${PROJECT_BINARY_DIR}/sonorem.1" @ONLY)
    CreateManGz(sonorem "sonorem.1")
endif()

if ($ENV{STMM_CMAKE_COMMENTS})
message(STATUS "")
message(STATUS "sonorem was configured with the following options:")
message(STATUS " STMMI_SNRM_SOURCES:            ${STMMI_SNRM_SOURCES}")
message(STATUS " SONOREM_EXTRA_LIBRARIES:       ${SONOREM_EXTRA_LIBRARIES}")
message(STATUS " CMAKE_BUILD_TYPE:              ${CMAKE_BUILD_TYPE}")
message(STATUS " CMAKE_CXX_COMPILER_ID:         ${CMAKE_CXX_COMPILER_ID}")
message(STATUS " CMAKE_CXX_COMPILER_VERSION:    ${CMAKE_CXX_COMPILER_VERSION}")
message(STATUS " install prefix:                ${CMAKE_INSTALL_PREFIX}")
endif()

enable_testing()
add_subdirectory(test)

install(TARGETS sonorem RUNTIME DESTINATION "bin")

if (STMM_INSTALL_LAUNCHER)
    install(FILES          "${STMMI_SNRM_DATA_DIR}/applications/com.efanomars.sonorem.desktop"
            DESTINATION  "${CMAKE_INSTALL_DATADIR}/applications/")
endif()
if (STMM_INSTALL_ICONS OR STMM_INSTALL_LAUNCHER)
    install(FILES            "${STMMI_SNRM_DATA_DIR}/icons/hicolor/24x24/apps/sonorem.png"
            DESTINATION    "${CMAKE_INSTALL_DATADIR}/icons/hicolor/24x24/apps/")
    install(FILES            "${STMMI_SNRM_DATA_DIR}/icons/hicolor/32x32/apps/sonorem.png"
            DESTINATION    "${CMAKE_INSTALL_DATADIR}/icons/hicolor/32x32/apps/")
    install(FILES            "${STMMI_SNRM_DATA_DIR}/icons/hicolor/48x48/apps/sonorem.png"
            DESTINATION    "${CMAKE_INSTALL_DATADIR}/icons/hicolor/48x48/apps/")
    install(FILES         "${STMMI_SNRM_DATA_DIR}/icons/hicolor/scalable/apps/sonorem.svg"
            DESTINATION "${CMAKE_INSTALL_DATADIR}/icons/hicolor/scalable/apps/")
endif()
if (STMM_INSTALL_MAN_PAGE)
    install(FILES                   "${PROJECT_BINARY_DIR}/sonorem.1.gz"
            DESTINATION "${CMAKE_INSTALL_DATADIR}/man/man1/")
endif()
 
