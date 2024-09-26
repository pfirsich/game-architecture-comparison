#include <glm/gtx/transform.hpp>

#include <glw/fmt.hpp>
#include <glw/state.hpp>
#include <glwx/transform.hpp>
#include <glwx/window.hpp>

#include "ecs.hpp"
#include "shared.hpp"

struct Transform {
    glwx::Transform value;
};

struct Velocity {
    glm::vec3 value = glm::vec3(0.0f);
};

struct Input {
    bool accel = false;
    float turn = 0.0f;
    BinaryInput shoot;
};

struct KeyboardControlled { };

struct Mesh {
    MeshHandle mesh;
    TextureHandle texture;
};

struct Asteroid {
    float radius;
};

struct Lifetime {
    float time;
};

struct Bullet { };

ecs::Entity create_ship()
{
    const auto ship = ecs::create();
    ecs::add<Transform>(ship).value.setScale(glm::vec3(0.1f));
    ecs::add<Velocity>(ship);
    ecs::add<Input>(ship);
    ecs::add<KeyboardControlled>(ship);
    ecs::add<Mesh>(ship, get_ship_mesh(), get_ship_texture());
    return ship;
}

ecs::Entity create_asteroid(const glm::vec3& position, const glm::vec3& velocity, float size)
{
    const auto asteroid = ecs::create();

    ecs::add<Asteroid>(asteroid, size * 0.5f * 0.85f); // fudge factor for collider

    auto& trafo = ecs::add<Transform>(asteroid).value;
    trafo.setPosition(position);
    trafo.setScale(size);
    const auto orientation
        = glm::quat(randf(-1.0f, 1.0f), randf(-1.0f, 1.0f), randf(-1.0f, 1.0f), randf(-1.0f, 1.0f));
    trafo.setOrientation(glm::normalize(orientation));

    ecs::add<Velocity>(asteroid, velocity);

    const auto meshes = get_asteroid_meshes();
    const auto mesh_idx = randi(0, meshes.size() - 1);
    ecs::add<Mesh>(asteroid, meshes[mesh_idx], get_asteroid_texture());

    return asteroid;
}

ecs::Entity create_asteroid()
{
    const auto edge = (randi(0, 1) * 2 - 1) * 0.4f * view_bounds_size;
    const auto axis_pos = randf(-0.5f, 0.5f) * view_bounds_size;
    const auto pos
        = randb() ? glm::vec3(axis_pos.x, 0.0f, edge.y) : glm::vec3(edge.x, 0.0f, axis_pos.y);

    const auto angle = randf(0.0f, glm::pi<float>() * 2.0f);
    const auto speed = randf(1.0f, 3.0f);
    const auto vel = glm::vec3(glm::cos(angle), 0.0f, glm::sin(angle)) * speed;

    const auto size = randf(1.0f, 5.0f);

    return create_asteroid(pos, vel, size);
}

ecs::Entity create_bullet(const glwx::Transform& ship_trafo)
{
    const auto bullet = ecs::create();

    auto& trafo = ecs::add<Transform>(bullet, ship_trafo).value;
    trafo.setScale(glm::vec3(1.0f));
    trafo.move(-trafo.getForward() * 0.5f); // move bullet slightly in front of the ship
    ecs::add<Velocity>(bullet, -trafo.getForward() * 20.0f);
    ecs::add<Mesh>(bullet, get_bullet_mesh(), get_bullet_texture());
    ecs::add<Lifetime>(bullet, 1.0f);
    ecs::add<Bullet>(bullet);

    return bullet;
}

void sys_set_input(float)
{
    ecs::for_each<Input, KeyboardControlled>([](ecs::Entity entity) {
        static int num_keys = 0;
        static const auto kb_state = SDL_GetKeyboardState(&num_keys);

        auto& input = ecs::get<Input>(entity);
        input.accel = kb_state[SDL_SCANCODE_W] > 0;
        input.turn = kb_state[SDL_SCANCODE_A] - kb_state[SDL_SCANCODE_D];
        input.shoot.update(kb_state[SDL_SCANCODE_SPACE]);
    });
}

void sys_control(float dt)
{
    ecs::for_each<Transform, Velocity, Input>([dt](ecs::Entity entity) {
        auto& transform = ecs::get<Transform>(entity).value;
        auto& input = ecs::get<Input>(entity);
        auto& velocity = ecs::get<Velocity>(entity).value;

        if (input.accel) {
            velocity += -transform.getForward() * dt * 2.0f;
        }

        const auto turn = glm::angleAxis(
            input.turn * glm::pi<float>() * 2.0f * dt, glm::vec3(0.0f, 1.0f, 0.0f) * 0.5f);
        transform.setOrientation(turn * transform.getOrientation());

        if (input.shoot.pressed()) {
            create_bullet(transform);
        }
    });
}

void sys_physics(float dt)
{
    ecs::for_each<Transform, Velocity>([dt](ecs::Entity entity) {
        auto& transform = ecs::get<Transform>(entity);
        auto& velocity = ecs::get<Velocity>(entity);
        auto pos = transform.value.getPosition() + velocity.value * dt;

        if (pos.x < -view_bounds_size.x * 0.5f) {
            pos.x += view_bounds_size.x;
        }
        if (pos.x > view_bounds_size.x * 0.5f) {
            pos.x -= view_bounds_size.x;
        }
        if (pos.z < -view_bounds_size.y * 0.5f) {
            pos.z += view_bounds_size.y;
        }
        if (pos.z > view_bounds_size.y * 0.5f) {
            pos.z -= view_bounds_size.y;
        }
        transform.value.setPosition(pos);
    });
}

void sys_render()
{
    const auto shader = get_shader();
    std::array<Uniform, 1> uniforms {
        Uniform { uniform_location(get_shader(), "u_texture"), TextureHandle {} },
    };
    ecs::for_each<Transform, Mesh>([&](ecs::Entity entity) {
        auto& transform = ecs::get<Transform>(entity);
        auto& mesh = ecs::get<Mesh>(entity);
        uniforms[0].value = mesh.texture;
        draw(shader, mesh.mesh, transform.value, uniforms);
    });
}

void sys_collide_asteroids()
{
    ecs::for_each<Transform, Velocity, Asteroid>([&](ecs::Entity a) {
        auto& a_trafo = ecs::get<Transform>(a).value;
        auto& a_velocity = ecs::get<Velocity>(a).value;
        const auto a_radius = ecs::get<Asteroid>(a).radius;
        ecs::for_each<Transform, Velocity, Asteroid>([&](ecs::Entity b) {
            if (a == b) {
                return;
            }

            auto& b_trafo = ecs::get<Transform>(b).value;
            auto& b_velocity = ecs::get<Velocity>(b).value;
            const auto b_radius = ecs::get<Asteroid>(b).radius;

            const auto rel = b_trafo.getPosition() - a_trafo.getPosition();
            if (glm::dot(rel, rel) < (a_radius + b_radius) * (a_radius + b_radius)) {
                collide_spheres(a_trafo, a_velocity, a_radius, b_trafo, b_velocity, b_radius);
            }
        });
    });
}

void sys_shoot_asteroids()
{
    // This is a good example of something a naive ECS cannot do well.
    // We are destroying the bullet if it hit something and then have to handle some boolean flag
    // all around.
    // Ideally we would remember the collision and queue an event and only later respond to that
    // event, so that we don't have to be careful with deleting a bullet that's still used in other
    // operations.
    // Essentially we would use a generalized variant of this flag.
    // While it is not affected by it, this systems also hints at another problem which is creating
    // entities during iteration. We iterate over all asteroids and inside that loop we sometimes
    // create more asteroids.
    // There is no easy way to know whether that new asteroid will be iterated over now or not.
    // To remedy this I could add a component that disables the entity temporarily and remove it
    // later, but that's easy to forget and very annoying.
    // I could also mark entities internally to be "asleep" and not actually existing until I mark
    // all as existing.
    // Or I could collect a list of entities upfront (and generate a skipfield maybe) for every
    // iteration and only iterate over that.
    constexpr auto bullet_radius = 1.0f;
    ecs::for_each<Transform, Velocity, Bullet>([&](ecs::Entity bullet) {
        const auto bullet_pos = ecs::get<Transform>(bullet).value.getPosition();
        const auto bullet_vel = ecs::get<Velocity>(bullet).value;
        bool hit = false;
        ecs::for_each<Transform, Velocity, Asteroid>([&](ecs::Entity asteroid) {
            if (hit) {
                return;
            }

            const auto asteroid_pos = ecs::get<Transform>(asteroid).value.getPosition();
            const auto asteroid_vel = ecs::get<Velocity>(asteroid).value;
            const auto asteroid_radius = ecs::get<Asteroid>(asteroid).radius;

            const auto rel = asteroid_pos - bullet_pos;
            const auto total_radius = asteroid_radius + bullet_radius;
            if (glm::dot(rel, rel) < total_radius * total_radius) {
                hit = true;
                ecs::destroy(asteroid);

                if (asteroid_radius < 0.5f) {
                    return;
                }

                const auto ortho = glm::normalize(glm::vec3(-bullet_vel.z, 0.0f, bullet_vel.x));
                // 1/(2^(1/3)) times the origional radius should yield half the volume.
                const auto radius = asteroid_radius * 0.8f;
                for (size_t i = 0; i < 2; ++i) {
                    const auto dir = static_cast<float>(i) * 2.0f - 1.0f;
                    const auto pos = asteroid_pos + dir * ortho * radius;
                    const auto vel = (asteroid_vel + dir * ortho * glm::length(asteroid_vel));
                    create_asteroid(pos, vel, radius * 2.0f);
                }
            }
        });
        if (hit) {
            ecs::destroy(bullet);
        }
    });
}

void sys_lifetime(float dt)
{
    ecs::for_each<Lifetime>([&](ecs::Entity entity) {
        auto& time = ecs::get<Lifetime>(entity).time;
        time -= dt;
        if (time <= 0.0f) {
            ecs::destroy(entity);
        }
    });
}

int main()
{
    auto window
        = glwx::makeWindow("Game Architecture Comparison - Classic ECS", 1920, 1080).value();
    glw::State::instance().setViewport(window.getSize().x, window.getSize().y);

    create_ship();

    for (size_t i = 0; i < 12; ++i) {
        create_asteroid();
    }

    init(static_cast<float>(window.getSize().x) / window.getSize().y);

    SDL_Event event;
    bool running = true;
    float time = glwx::getTime();
    while (running) {
        while (SDL_PollEvent(&event) != 0) {
            switch (event.type) {
            case SDL_QUIT:
                running = false;
                break;
            case SDL_KEYDOWN:
                switch (event.key.keysym.sym) {
                case SDLK_ESCAPE:
                    running = false;
                    break;
                }
            }
        }

        const auto now = glwx::getTime();
        const auto dt = now - time;
        time = now;

        sys_set_input(dt);
        sys_control(dt);
        sys_collide_asteroids();
        sys_physics(dt);
        sys_lifetime(dt);
        sys_shoot_asteroids();

        begin_frame();
        sys_render();
        end_frame();

        window.swap();
    }

    return 0;
}