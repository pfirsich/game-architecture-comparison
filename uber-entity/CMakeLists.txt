add_executable(uber-entity-asteroids main.cpp)
target_link_libraries(uber-entity-asteroids PRIVATE shared-lib)
set_wall(uber-entity-asteroids)