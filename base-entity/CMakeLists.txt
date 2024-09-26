add_executable(base-entity-asteroids main.cpp)
target_link_libraries(base-entity-asteroids PRIVATE shared-lib)
set_wall(base-entity-asteroids)