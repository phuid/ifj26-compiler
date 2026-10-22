cmake_minimum_required(VERSION 3.20)

project(ifj26-compiler LANGUAGES C CXX)

include(CTest)

set(TEST_LIST_VARIABLES)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

file(GLOB APP_SOURCES CONFIGURE_DEPENDS
     "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")
add_executable(ifj26-compiler ${APP_SOURCES})
target_compile_options(ifj26-compiler PRIVATE -Wall -Wextra -Werror)

if(BUILD_TESTING)
    find_package(GTest REQUIRED)
    include(GoogleTest)

    file(GLOB SRC_LIBRARY_SOURCES CONFIGURE_DEPENDS
         "${CMAKE_CURRENT_SOURCE_DIR}/src/*.c")
    list(REMOVE_ITEM SRC_LIBRARY_SOURCES
         "${CMAKE_CURRENT_SOURCE_DIR}/src/main.c")

    file(GLOB SRC_LIBRARY_HEADERS CONFIGURE_DEPENDS
         "${CMAKE_CURRENT_SOURCE_DIR}/src/*.h")
    set(C_LINKAGE_HEADER
        "${CMAKE_CURRENT_BINARY_DIR}/c_linkage_headers.h")
    file(WRITE "${C_LINKAGE_HEADER}"
         "#ifdef __cplusplus\nextern \"C\" {\n#endif\n")
    foreach(SRC_LIBRARY_HEADER IN LISTS SRC_LIBRARY_HEADERS)
        file(APPEND "${C_LINKAGE_HEADER}"
             "#include \"${SRC_LIBRARY_HEADER}\"\n")
    endforeach()
    file(APPEND "${C_LINKAGE_HEADER}"
         "#ifdef __cplusplus\n}\n#endif\n")

    file(GLOB TEST_SOURCES CONFIGURE_DEPENDS
         "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cc"
         "${CMAKE_CURRENT_SOURCE_DIR}/tests/*.cpp")

    foreach(TEST_SOURCE IN LISTS TEST_SOURCES)
        get_filename_component(TEST_NAME "${TEST_SOURCE}" NAME_WE)
        add_executable("${TEST_NAME}" "${TEST_SOURCE}" ${SRC_LIBRARY_SOURCES})
        target_compile_options("${TEST_NAME}" PRIVATE
            "$<$<COMPILE_LANGUAGE:CXX>:-include${C_LINKAGE_HEADER}>")
        target_link_libraries("${TEST_NAME}" PRIVATE GTest::gtest_main)
        target_include_directories("${TEST_NAME}" PRIVATE
            "${CMAKE_CURRENT_SOURCE_DIR}/src")
        gtest_discover_tests("${TEST_NAME}"
            WORKING_DIRECTORY "${CMAKE_CURRENT_BINARY_DIR}"
            TEST_LIST "${TEST_NAME}_TESTS"
            ADD_TAGS_AS_LABELS)
        list(APPEND TEST_LIST_VARIABLES "${TEST_NAME}_TESTS")
    endforeach()

    # test labels for filtering
    configure_file(
        "${CMAKE_CURRENT_SOURCE_DIR}/cmake/modifyTests.cmake.in"
        "${CMAKE_CURRENT_BINARY_DIR}/modifyTests.cmake"
        @ONLY)
    set_property(DIRECTORY APPEND PROPERTY TEST_INCLUDE_FILES
        "${CMAKE_CURRENT_BINARY_DIR}/modifyTests.cmake")
endif()
