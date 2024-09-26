add_executable(classic-ecs-asteroids main.cpp)
target_link_libraries(classic-ecs-asteroids PRIVATE shared-lib)
set_wall(classic-ecs-asteroids)

add_executable(test-classic-ecs test-ecs.cpp)
target_link_libraries(test-classic-ecs PRIVATE shared-lib)
target_compile_definitions(test-classic-ecs PRIVATE TESTS)
set_wall(test-classic-ecs)