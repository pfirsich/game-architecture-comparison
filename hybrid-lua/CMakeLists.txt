set(CMAKE_MODULE_PATH "${CMAKE_SOURCE_DIR}/hybrid-lua" ${CMAKE_MODULE_PATH})

find_package(luajit REQUIRED)

add_executable(hybrid-lua-asteroids main.cpp)
target_link_libraries(hybrid-lua-asteroids PRIVATE shared-lib luajit)
set_wall(hybrid-lua-asteroids)