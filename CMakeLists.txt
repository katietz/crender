cmake_minimum_required(VERSION 3.0.0)

set(target_name crender)
project("${target_name}")

# update submodules

# jinja2cpp
add_subdirectory(Jinja2Cpp)

# yaml-cpp
add_subdirectory(yaml-cpp)


# Possibly set some flags
#set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-strict-aliasing" )
#set(CMAKE_POSITION_INDEPENDENT_CODE ON)


# Boost!
#find_package( Boost REQUIRED date_time thread system chrono)
#include_directories(SYSTEM ${Boost_INCLUDE_DIRS})

#if( NOT WIN32 AND NATIVE_SIZE EQUAL 64 )
#  set( Boost_LIBRARIES ${Boost_LIBRARIES} rt)
#endif()



# Not everyone considers GLOBing good form in a mature project.
file(GLOB_RECURSE src_cpp src/*.cpp src/*.cc)
file(GLOB_RECURSE src_src/*.h)

add_executable(${target_name} ${src_cpp} ${src_h} )
set_property(TARGET ${target_name} PROPERTY CXX_STANDARD 17)

target_include_directories(${target_name} PRIVATE
  ${CMAKE_SOURCE_DIR}/src
  )

target_include_directories(${target_name} SYSTEM PRIVATE
  ${CMAKE_SOURCE_DIR}/Jinja2Cpp/include
  ${CMAKE_SOURCE_DIR}/yaml-cpp/include
  )

#target_link_directories(${target_name} PRIVATE  # maybe this should be public?
#  )

target_link_libraries(${target_name} PRIVATE # maybe this should be public?
  jinja2cpp
  yaml-cpp
  pthread
)

if( APPLE OR UNIX OR MINGW )
  target_compile_options(${target_name} PRIVATE
    -Wall
    -Wextra
    -Werror
    -Wfloat-equal
    -Wshadow
#    -Wduplicated-branches
    -Wctor-dtor-privacy
    -Wnon-virtual-dtor
    -Wsuggest-override
    -Wredundant-decls
#    -Wlogical-op
    -Wold-style-cast
    )

elseif(MSVC)
  target_compile_options(${target_name} PRIVATE
    /W4
    /WX
    )

else()
  message(FATAL_ERROR "Can't set compile options")
endif()
