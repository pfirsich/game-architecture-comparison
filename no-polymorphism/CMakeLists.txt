add_executable(no-polymorphism-asteroids main.cpp)
target_link_libraries(no-polymorphism-asteroids PRIVATE shared-lib)
set_wall(no-polymorphism-asteroids)