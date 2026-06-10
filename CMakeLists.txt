cmake_minimum_required(VERSION 3.20)

project(namumark LANGUAGES C CXX)

set(CMAKE_C_STANDARD 11)
set(CMAKE_C_STANDARD_REQUIRED ON)
set(CMAKE_C_EXTENSIONS OFF)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)

option(NAMUMARK_BUILD_TESTING "Build tests" ON)

add_library(namumark-lib STATIC
  lib/blocks.c
  lib/inlines.c
  lib/namumark.c
  lib/node.c
  lib/parser.c
  lib/renderer_ast.c
  lib/renderer_html.c
  lib/strbuf.c
)

add_library(namumark-shared SHARED
  lib/blocks.c
  lib/inlines.c
  lib/namumark.c
  lib/node.c
  lib/parser.c
  lib/renderer_ast.c
  lib/renderer_html.c
  lib/strbuf.c
)

set_target_properties(namumark-shared PROPERTIES
  OUTPUT_NAME namumark
)

target_include_directories(namumark-lib
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

target_include_directories(namumark-shared
  PUBLIC
    ${CMAKE_CURRENT_SOURCE_DIR}
)

if(MSVC)
  set(NAMUMARK_WARNING_FLAGS /W4)
else()
  set(NAMUMARK_WARNING_FLAGS -Wall -Wextra)
endif()

target_compile_options(namumark-lib
  PRIVATE
    ${NAMUMARK_WARNING_FLAGS}
)

target_compile_options(namumark-shared
  PRIVATE
    ${NAMUMARK_WARNING_FLAGS}
)

add_executable(namumark
  main/main.c
)

target_link_libraries(namumark
  PRIVATE
    namumark-lib
)

install(TARGETS namumark namumark-shared
  RUNTIME DESTINATION bin
  LIBRARY DESTINATION lib
  ARCHIVE DESTINATION lib
)

install(FILES lib/namumark.h DESTINATION include/namumark)
install(FILES docs/namumark-api.3 DESTINATION share/man/man3)

if(NAMUMARK_BUILD_TESTING)
  include(CTest)
  include(FetchContent)

  FetchContent_Declare(
    googletest
    URL https://github.com/google/googletest/archive/refs/tags/v1.15.2.zip
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
  )
  set(gtest_force_shared_crt ON CACHE BOOL "" FORCE)
  FetchContent_MakeAvailable(googletest)

  add_executable(namumark-test
    test/api_test.cc
    test/inlines_test.cc
    test/parser_test.cc
    test/renderer_test.cc
    test/strbuf_test.cc
  )

  target_link_libraries(namumark-test
    PRIVATE
      GTest::gtest_main
      namumark-lib
  )

  include(GoogleTest)
  gtest_discover_tests(namumark-test)
endif()
