add_executable(hybrid-asteroids main.cpp)
target_link_libraries(hybrid-asteroids PRIVATE shared-lib)
set_wall(hybrid-asteroids)