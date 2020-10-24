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

# File:   sonorem-defs.cmake


# Libtool CURRENT/REVISION/AGE: here
#   MAJOR is CURRENT interface
#   MINOR is REVISION (implementation of interface)
#   AGE is always 0
set(SONOREM_MAJOR_VERSION 0)
set(SONOREM_MINOR_VERSION 3) # !-U-!
set(SONOREM_VERSION "${SONOREM_MAJOR_VERSION}.${SONOREM_MINOR_VERSION}.0")

# required stmm-input-gtk-dm version
set(SONOREM_REQ_STMM_INPUT_GTK_DM_MAJOR_VERSION 0)
set(SONOREM_REQ_STMM_INPUT_GTK_DM_MINOR_VERSION 16) # !-U-!
set(SONOREM_REQ_STMM_INPUT_GTK_DM_VERSION "${SONOREM_REQ_STMM_INPUT_GTK_DM_MAJOR_VERSION}.${SONOREM_REQ_STMM_INPUT_GTK_DM_MINOR_VERSION}")

# required stmm-input-gtk-bt version
set(SONOREM_REQ_STMM_INPUT_GTK_BT_MAJOR_VERSION 0)
set(SONOREM_REQ_STMM_INPUT_GTK_BT_MINOR_VERSION 19) # !-U-!
set(SONOREM_REQ_STMM_INPUT_GTK_BT_VERSION "${SONOREM_REQ_STMM_INPUT_GTK_BT_MAJOR_VERSION}.${SONOREM_REQ_STMM_INPUT_GTK_BT_MINOR_VERSION}")

# run time dependencies
set(SONOREM_REQ_SOX_VERSION          "14.4.1")
set(SONOREM_REQ_ESPEAK_VERSION       "1.48.0")

if ("${CMAKE_SCRIPT_MODE_FILE}" STREQUAL "")
    include(FindPkgConfig)
    if (NOT PKG_CONFIG_FOUND)
        message(FATAL_ERROR "Mandatory 'pkg-config' not found!")
    endif()
    # Beware! The prefix passed to pkg_check_modules(PREFIX ...) shouldn't contain underscores!
    pkg_check_modules(STMMINPUTGTKDM   REQUIRED  stmm-input-gtk-dm>=${SONOREM_REQ_STMM_INPUT_GTK_DM_VERSION})
    pkg_check_modules(STMMINPUTGTKBT   REQUIRED  stmm-input-gtk-bt>=${SONOREM_REQ_STMM_INPUT_GTK_BT_VERSION})
endif()

# include dirs
list(APPEND SONOREM_EXTRA_INCLUDE_DIRS  "${STMMINPUTGTKDM_INCLUDE_DIRS}")
list(APPEND SONOREM_EXTRA_INCLUDE_DIRS  "${STMMINPUTGTKBT_INCLUDE_DIRS}")

# libs
list(APPEND SONOREM_EXTRA_LIBRARIES     "${STMMINPUTGTKDM_LIBRARIES}")
list(APPEND SONOREM_EXTRA_LIBRARIES     "${STMMINPUTGTKBT_LIBRARIES}")
