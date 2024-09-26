cmake_minimum_required(VERSION 3.3.0)

project(game-architecture-comparison)

set(CMAKE_CXX_STANDARD 20)
set(CMAKE_EXPORT_COMPILE_COMMANDS on)
set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/cmake" ${CMAKE_MODULE_PATH})

# no rtti
if(MSVC)
	string(REGEX REPLACE "/GR" "/GR-" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
else()
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-rtti")
endif()

# no exceptions
if(MSVC)
	string(REGEX REPLACE "/EHsc" "/EHs-c-" CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS}")
else()
	set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -fno-exceptions")
endif()

include(wall)

option(GAC_ENABLE_ASAN "Enable ASan" OFF)
if (GAC_ENABLE_ASAN)
  include(asan)
endif()

add_subdirectory(deps/cppasta)
add_subdirectory(deps/glwrap)

add_library(shared-lib STATIC shared/shared.cpp)
target_include_directories(shared-lib PUBLIC shared/)
target_link_libraries(shared-lib PUBLIC cppasta)
target_link_libraries(shared-lib PUBLIC glwx)

add_subdirectory(classic-ecs/)
add_subdirectory(base-entity/)
add_subdirectory(unity-style/)
add_subdirectory(uber-entity/)
add_subdirectory(no-polymorphism/)
add_subdirectory(hybrid/)
add_subdirectory(hybrid-lua/)
