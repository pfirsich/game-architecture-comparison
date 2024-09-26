add_executable(unity-style-asteroids main.cpp ecs.cpp)
target_link_libraries(unity-style-asteroids PRIVATE shared-lib)
set_wall(unity-style-asteroids)