cmake_minimum_required(VERSION 3.11)
project(toferp)

set(CMAKE_CXX_STANDARD 11)
add_compile_options(-Wall -Wextra -g)

set(SOURCE_FILES
        main.cpp
        common.h
        VarManager.cpp
        VarManager.h
        Reader.cpp
        Reader.h)

add_executable(toferp ${SOURCE_FILES})

find_package( ZLIB REQUIRED )
if ( ZLIB_FOUND )
    include_directories( ${ZLIB_INCLUDE_DIRS} )
    target_link_libraries( toferp ${ZLIB_LIBRARIES} )
endif( ZLIB_FOUND )
