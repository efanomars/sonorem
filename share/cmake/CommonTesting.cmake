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

# File:   CommonTesting.cmake

# TestFiles              Create test executables for a target library.
#
# Parameters:
# STMMI_TEST_SOURCES     list of test source files for each of which a test executable
#                        is created.
# STMMI_WITH_SOURCES     list of sources that are compiled with each of the tests in 
#                        STMMI_TEST_SOURCES. This function adds the additional source
#                        "${PROJECT_BINARY_DIR}/test/testconfig.cc" if it finds a
#                        file "${PROJECT_SOURCE_DIR}/test/testconfig.cc.in" to
#                        configure_file from (@ONLY). The following variables are defined:
#                          STMI_TESTING_SOURCE_DIR = "${PROJECT_SOURCE_DIR}/test"
#                          STMI_TESTING_EXE_PATH   = executable path
# STMMI_LINKED_INCLUDES  list of include directories needed by each test.
# STMMI_LINKED_LIBS      list of libraries that have to be linked to each test.
#                        One of them might be the to be tested library target name
#                        (cmake will recognize and link the freshly created one instead of 
#                        the possibly installed one).
#                        ex. "stmm-input-ev;stmm-input-gtk". Note: don't prepend 'lib'!
# STMMI_FAKE_IFACE       bool that tells whether the compiler definition STMI_TESTING_IFACE
#                        is defined for each test. This can be used to conditionally declare 
#                        a class method virtual to create fake subclasses.
#                      
#
# Implicit paramters (all the project's libraries have to define them):
# STMMI_HEADERS_DIR      The directory of public headers of the to be tested library
# STMMI_INCLUDE_DIR      The directory containing STMMI_HEADERS_DIR
# STMMI_SOURCES_DIR      The directory of private headers and sources of the to be tested library
#
function(TestFiles STMMI_TEST_SOURCES  STMMI_WITH_SOURCES  STMMI_LINKED_INCLUDES  STMMI_LINKED_LIBS  STMMI_FAKE_IFACE)

    if (BUILD_TESTING)

        if ($ENV{STMM_CMAKE_COMMENTS})
        message(STATUS "STMMI_TEST_SOURCES     ${STMMI_TEST_SOURCES}")
        message(STATUS "STMMI_WITH_SOURCES     ${STMMI_WITH_SOURCES}")
        message(STATUS "STMMI_LINKED_INCLUDES  ${STMMI_LINKED_INCLUDES}")
        message(STATUS "STMMI_LINKED_LIBS      ${STMMI_LINKED_LIBS}")
        message(STATUS "STMMI_FAKE_IFACE       ${STMMI_FAKE_IFACE}")
        message(STATUS "STMMI_HEADERS_DIR      ${STMMI_HEADERS_DIR}")
        message(STATUS "STMMI_SOURCES_DIR      ${STMMI_SOURCES_DIR}")
        endif()

        #  TODO
        #  #precompile a static lib with STMMI_WITH_SOURCES
        #  list(LENGTH STMMI_WITH_SOURCES STMMI_WITH_SOURCES_LEN)
        #  if (STMMI_WITH_SOURCES_LEN GREATER 0)
        #      add_library(stmmiwithsourcesobjlib OBJECT ${STMMI_WITH_SOURCES})
        #  endif (STMMI_WITH_SOURCES_LEN GREATER 0)
        #   ^ need to target_compile_definitions target_include_directories !!!
        #
        if (EXISTS "${PROJECT_SOURCE_DIR}/test/testconfig.cc.in")
            if (EXISTS "${PROJECT_SOURCE_DIR}/test/testconfig.h")
                file(COPY "${PROJECT_SOURCE_DIR}/test/testconfig.h" DESTINATION "${PROJECT_BINARY_DIR}/test")
            endif()
            set(STMI_TESTING_SOURCE_DIR "${PROJECT_SOURCE_DIR}/test")
            list(APPEND STMMI_WITH_SOURCES "${PROJECT_BINARY_DIR}/test/testconfig.cc")
        endif()
        #
        find_package(Threads REQUIRED)
        # Iterate over all tests found. For each, declare an executable and add it to the tests list.
        foreach (STMMI_TEST_CUR_FILE  ${STMMI_TEST_SOURCES})

#message(STATUS "STMMI_TEST_CUR_FILE     ${STMMI_TEST_CUR_FILE}")
            file(RELATIVE_PATH  STMMI_TEST_CUR_REL_FILE  ${PROJECT_SOURCE_DIR}/test  ${STMMI_TEST_CUR_FILE})

            string(REGEX REPLACE "[./]" "_" STMMI_TEST_CUR_TGT ${STMMI_TEST_CUR_REL_FILE})

            if (EXISTS "${PROJECT_SOURCE_DIR}/test/testconfig.cc.in")
                set(STMI_TESTING_EXE_PATH   "${PROJECT_BINARY_DIR}/test/${STMMI_TEST_CUR_TGT}")
                configure_file("${PROJECT_SOURCE_DIR}/test/testconfig.cc.in"
                               "${PROJECT_BINARY_DIR}/test/testconfig.cc" @ONLY)
            endif()

            add_executable(${STMMI_TEST_CUR_TGT} ${STMMI_TEST_CUR_FILE}
                           ${STMMI_WITH_SOURCES})
            #if (STMMI_WITH_SOURCES_LEN GREATER 0)
            #    target_link_libraries(${STMMI_TEST_CUR_TGT} stmmiwithsourcesobjlib) # link precompiled object files
            #endif (STMMI_WITH_SOURCES_LEN GREATER 0)

            # for auxiliary files
            target_include_directories(${STMMI_TEST_CUR_TGT} BEFORE PRIVATE ${PROJECT_SOURCE_DIR}/test)
            # the library files
            target_include_directories(${STMMI_TEST_CUR_TGT} BEFORE PRIVATE ${STMMI_INCLUDE_DIR})
            # tests can also involve non public part of the library!
            target_include_directories(${STMMI_TEST_CUR_TGT} BEFORE PRIVATE ${STMMI_SOURCES_DIR})
            target_include_directories(${STMMI_TEST_CUR_TGT} BEFORE PRIVATE ${STMMI_HEADERS_DIR})
            target_include_directories(${STMMI_TEST_CUR_TGT}        PRIVATE ${STMMI_LINKED_INCLUDES})
            target_include_directories(${STMMI_TEST_CUR_TGT}        PRIVATE ${PROJECT_SOURCE_DIR}/share/thirdparty)
            if (STMMI_FAKE_IFACE)
                target_compile_definitions(${STMMI_TEST_CUR_TGT} PUBLIC STMI_TESTING_IFACE)
            endif (STMMI_FAKE_IFACE)
            DefineTestTargetPublicCompileOptions(${STMMI_TEST_CUR_TGT})

            target_compile_definitions(${STMMI_TEST_CUR_TGT} PRIVATE  "_FILE_OFFSET_BITS=64")
            target_link_libraries(${STMMI_TEST_CUR_TGT} ${CMAKE_THREAD_LIBS_INIT})

            target_link_libraries(${STMMI_TEST_CUR_TGT}  ${STMMI_LINKED_LIBS})

            add_test(NAME ${STMMI_TEST_CUR_TGT} COMMAND ${STMMI_TEST_CUR_TGT})       # this is how to add tests to CMake

         endforeach (STMMI_TEST_CUR_FILE  ${STMMI_TEST_SOURCES})
    endif (BUILD_TESTING)

endfunction()
