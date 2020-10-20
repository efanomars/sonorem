# Copyright © 2019-2020  Stefano Marsili, <stemars@gmx.ch>
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

# File:   CommonUtil.cmake


# CheckBinaryNotSourceTree    Check not building in the source tree
#
function(CheckBinaryNotSourceTree)
    if (${CMAKE_SOURCE_DIR} STREQUAL ${CMAKE_BINARY_DIR})
        message(FATAL_ERROR "Prevented in-tree built. Please create a build directory outside of the ${CMAKE_PROJECT_NAME} source code and call cmake from there")
    endif()
endfunction(CheckBinaryNotSourceTree)


# CheckBuildType              Check CMAKE_BUILD_TYPE is valid
#
function(CheckBuildType)
    if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
    elseif ("${CMAKE_BUILD_TYPE}" STREQUAL "Release")
    elseif ("${CMAKE_BUILD_TYPE}" STREQUAL "MinSizeRel")
    elseif ("${CMAKE_BUILD_TYPE}" STREQUAL "RelWithDebInfo")
    else()
        message(FATAL_ERROR "Wrong CMAKE_BUILD_TYPE, set to either Debug, Release, MinSizeRel or RelWithDebInfo")
    endif()
    #
    set(CMAKE_EXPORT_COMPILE_COMMANDS ON    PARENT_SCOPE)
    #
endfunction(CheckBuildType)


# DefineSharedLibOption       Define cmake option BUILD_SHARED_LIBS
#
function(DefineSharedLibOption)
    option(BUILD_SHARED_LIBS "Build shared library ${CMAKE_PROJECT_NAME}" ON)
    mark_as_advanced(BUILD_SHARED_LIBS)
endfunction(DefineSharedLibOption)

# DefineCommonOptions         Define cmake options for all subprojects
#
function(DefineCommonOptions)
    option(BUILD_WITH_SANITIZE "Build with sanitize=address (Debug only!)" OFF)
    mark_as_advanced(BUILD_WITH_SANITIZE)
endfunction(DefineCommonOptions)


# DefineCommonCompileOptions  Define compile options for all subprojects
#
# Parameters:
# STMMI_CPP_STD   The compiler c++ standard to use (ex. c++14)
#
function(DefineCommonCompileOptions STMMI_CPP_STD)
    string(STRIP "${CMAKE_CXX_FLAGS} -std=${STMMI_CPP_STD}" CMAKE_CXX_FLAGS)
    set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}" PARENT_SCOPE)
    if (("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU") OR ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang"))
        if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
            if (BUILD_WITH_SANITIZE)
                set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fsanitize=address" PARENT_SCOPE)
            endif()
        endif()
    endif()
endfunction(DefineCommonCompileOptions)

# DefineAsSecondaryTarget    In script mode, create the target for a library
#                               and sets its dependencies.
#
# Parameters:
# STMMI_TARGET        The (library) target. Ex. 'stmm-input'.
# STMMI_LIB_FILE      The library '.so' or '.a' file path.
#                     Ex. "${PROJECT_SOURCE_DIR}/../libstmm-input/build/libstmm-input.so"
# STMMI_INCLUDE_DIRS  The library's (non system) include directories.
# STMMI_SUBPRJ_DEPS   The list of (non system library) dependencies (these are all targets!).
#                     Ex. library stmm-input-gtk-dm would pass "stmm-input-gtk;stmm-input-ev;stmm-input-dl"
# STMMI_SYS_LIBS      The additional librariy dependencies
#
function(DefineAsSecondaryTarget STMMI_TARGET  STMMI_LIB_FILE STMMI_INCLUDE_DIRS STMMI_SUBPRJ_DEPS STMMI_SYS_LIBS)
    if (NOT ("${CMAKE_SCRIPT_MODE_FILE}" STREQUAL ""))
        message(FATAL_ERROR "Function DefineAsSecondaryTarget cannot be called in script mode")
    endif()
    if (TARGET ${STMMI_TARGET})
        return()
    endif()
    if (BUILD_SHARED_LIBS)
        add_library(${STMMI_TARGET} SHARED IMPORTED)
    else()
        add_library(${STMMI_TARGET} STATIC IMPORTED)
    endif()
    set_target_properties(${STMMI_TARGET} PROPERTIES IMPORTED_LOCATION             "${STMMI_LIB_FILE}")
    set_target_properties(${STMMI_TARGET} PROPERTIES INTERFACE_INCLUDE_DIRECTORIES "${STMMI_INCLUDE_DIRS}")
    set(STMMI_TEMP_PROPS "")
    foreach (STMMI_CUR_SUBPRJ_DEP  ${STMMI_SUBPRJ_DEPS})
        get_target_property(STMMI_TEMP_SUBPRJ_PROP ${STMMI_CUR_SUBPRJ_DEP} INTERFACE_LINK_LIBRARIES)
        list(APPEND STMMI_TEMP_PROPS "${STMMI_TEMP_SUBPRJ_PROP}")
        list(APPEND STMMI_TEMP_PROPS "${STMMI_CUR_SUBPRJ_DEP}")
    endforeach (STMMI_CUR_SUBPRJ_DEP  ${STMMI_SUBPRJ_DEPS})
    if (NOT ("${STMMI_SYS_LIBS}" STREQUAL ""))
        list(APPEND STMMI_TEMP_PROPS "${STMMI_SYS_LIBS}")
    endif()
    set_target_properties(${STMMI_TARGET} PROPERTIES INTERFACE_LINK_LIBRARIES      "${STMMI_TEMP_PROPS}")
endfunction(DefineAsSecondaryTarget)

# DefineTargetInterfaceCompileOptions    Define compile options for a header only library target
#
# Parameters:
# STMMI_TARGET   The (library) target
#
function(DefineTargetInterfaceCompileOptions STMMI_TARGET)
    DefineTargetCompileOptionsType(${STMMI_TARGET} ON OFF)
endfunction(DefineTargetInterfaceCompileOptions)

# DefineTargetPublicCompileOptions       Define compile options for a normal target
#
# Parameters:
# STMMI_TARGET   The (library) target
#
function(DefineTargetPublicCompileOptions STMMI_TARGET)
    DefineTargetCompileOptionsType(${STMMI_TARGET} OFF OFF)
endfunction(DefineTargetPublicCompileOptions)

# DefineTestTargetPublicCompileOptions   Define compile options for a test target
#
# Parameters:
# STMMI_TARGET   The (library) target
#
function(DefineTestTargetPublicCompileOptions STMMI_TARGET)
    DefineTargetCompileOptionsType(${STMMI_TARGET} OFF ON)
endfunction(DefineTestTargetPublicCompileOptions)

# private:
function(DefineTargetCompileOptionsType STMMI_TARGET STMMI_INTERFACE_TYPE STMMI_IS_TEST)
    # STMMI_COMPILE_WARNINGS holds target specific flags
    set(STMMI_COMPILE_WARNINGS "${CMAKE_CXX_FLAGS}")
    set(STMMI_PRIVATE_COMPILE_WARNINGS "")
    if (("${CMAKE_CXX_COMPILER_ID}" STREQUAL "GNU") OR ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang"))
        set(STMMI_COMPILE_WARNINGS "${STMMI_COMPILE_WARNINGS} -Wall -Wextra \
-pedantic-errors -Wmissing-include-dirs -Winit-self \
-Woverloaded-virtual -Wsign-promo")
#         if ("${CMAKE_BUILD_TYPE}" STREQUAL "Debug")
#             set(STMMI_COMPILE_WARNINGS "${STMMI_COMPILE_WARNINGS} -ggdb")
#         endif()
    elseif ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "MSVC")
        # no idea if this really works
        set(STMMI_COMPILE_WARNINGS "${STMMI_COMPILE_WARNINGS} /W2 /EHsc") # /WX warnings as errors
    endif()
    if (NOT STMMI_IS_TEST)
        set(STMMI_PRIVATE_COMPILE_WARNINGS "${STMMI_PRIVATE_COMPILE_WARNINGS} $ENV{STMM_CPP_OPTIONS}")
        if ("${CMAKE_CXX_COMPILER_ID}" STREQUAL "Clang")
            string(REPLACE "-Wsuggest-override" "" STMMI_PRIVATE_COMPILE_WARNINGS ${STMMI_PRIVATE_COMPILE_WARNINGS})
            string(REPLACE "-Wlogical-op" "" STMMI_PRIVATE_COMPILE_WARNINGS ${STMMI_PRIVATE_COMPILE_WARNINGS})
        endif()
    endif()
    if ("$ENV{STMM_CPP_DEBUG}" STREQUAL "ON")
        target_compile_definitions(${STMMI_TARGET} PUBLIC STMM_TRACE_DEBUG)
    endif ()
    #
    if ("$ENV{STMM_SNAP_PACKAGING}" STREQUAL "ON")
        target_compile_definitions(${STMMI_TARGET} PUBLIC STMM_SNAP_PACKAGING)
    endif ()
    #
    string(STRIP "${STMMI_COMPILE_WARNINGS}" STMMI_COMPILE_WARNINGS)
    if (NOT ("${STMMI_COMPILE_WARNINGS}" STREQUAL ""))
        string(REPLACE " " ";" REPLACED_FLAGS ${STMMI_COMPILE_WARNINGS})
    endif()
    if (STMMI_INTERFACE_TYPE)
        target_compile_options(${STMMI_TARGET} INTERFACE ${REPLACED_FLAGS})
    else()
        target_compile_options(${STMMI_TARGET} PUBLIC ${REPLACED_FLAGS})
        string(STRIP "${STMMI_PRIVATE_COMPILE_WARNINGS}" STMMI_PRIVATE_COMPILE_WARNINGS)
        if (NOT ("${STMMI_PRIVATE_COMPILE_WARNINGS}" STREQUAL ""))
            string(REPLACE " " ";" REPLACED_FLAGS ${STMMI_PRIVATE_COMPILE_WARNINGS})
            target_compile_options(${STMMI_TARGET} PRIVATE ${REPLACED_FLAGS})
        endif()
    endif()
endfunction(DefineTargetCompileOptionsType)

# CreateManGz      Create man page zip from man page
#
# Parameters:
# STMMI_TARGET     The target for which the manual should be created
# STMMI_MANFILE    The man file to zip (expected to be in CMAKE_BINARY_DIR). Ex. "jointris.1"
function(CreateManGz STMMI_TARGET STMMI_MANFILE)
    add_custom_command(OUTPUT "${STMMI_MANFILE}.gz"
                       COMMAND gzip -k "${CMAKE_BINARY_DIR}/${STMMI_MANFILE}"
                       DEPENDS "${CMAKE_BINARY_DIR}/${STMMI_MANFILE}"
                       WORKING_DIRECTORY ${CMAKE_BINARY_DIR}
                       COMMENT "Gzip: for ${STMMI_MANFILE}"
                       VERBATIM)
    add_custom_target(${STMMI_TARGET}_man DEPENDS "${STMMI_MANFILE}.gz")
    add_dependencies(${STMMI_TARGET} ${STMMI_TARGET}_man)
endfunction(CreateManGz)
