#include <glm/gtx/transform.hpp>

#include <glw/fmt.hpp>
#include <glw/state.hpp>
#include <glwx/transform.hpp>
#include <glwx/window.hpp>

#include "ecs.hpp"
#include "shared.hpp"

GameObjectId create_ship();
GameObjectId create_asteroid(const glm::vec3& position, const glm::vec3& velocity, float size);
GameObjectId create_asteroid();
GameObjectId create_bullet(const glwx::Transform& ship_trafo);

struct Transform : public Component {
    glwx::Transform transform;

    Transform(const glwx::Transform& trafo = {}) : transform(trafo) { }
};

struct Velocity : public Component {
    glm::vec3 velocity = glm::vec3(0.0f);

    Velocity(const glm::vec3& vel = {}) : velocity(vel) { }

    void update(float dt) override
    {
        auto& trafo = get_component<Transform>().transform;
        auto pos = trafo.getPosition() + velocity * dt;

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
        trafo.setPosition(pos);
    }
};

struct Input : public Component {
    bool accel = false;
    float turn = 0.0f;
    BinaryInput shoot;

    void update(float dt) override
    {
        auto& trafo = get_component<Transform>().transform;
        auto& velocity = get_component<Velocity>().velocity;

        if (accel) {
            velocity += -trafo.getForward() * dt * 2.0f;
        }

        const auto rot = glm::angleAxis(
            turn * glm::pi<float>() * 2.0f * dt, glm::vec3(0.0f, 1.0f, 0.0f) * 0.5f);
        trafo.setOrientation(rot * trafo.getOrientation());

        if (shoot.pressed()) {
            create_bullet(trafo);
        }
    }
};

struct KeyboardControlled : public Component {
    void update(float) override
    {
        static int num_keys = 0;
        static const auto kb_state = SDL_GetKeyboardState(&num_keys);

        auto& input = get_component<Input>();
        input.accel = kb_state[SDL_SCANCODE_W] > 0;
        input.turn = kb_state[SDL_SCANCODE_A] - kb_state[SDL_SCANCODE_D];
        input.shoot.update(kb_state[SDL_SCANCODE_SPACE]);
    }
};

struct Mesh : public Component {
    MeshHandle mesh;
    TextureHandle texture;

    Mesh(MeshHandle m, TextureHandle t) : mesh(m), texture(t) { }

    void update(float) override
    {
        static const auto shader = get_shader();
        std::array<Uniform, 1> uniforms {
            Uniform { uniform_location(get_shader(), "u_texture"), TextureHandle {} },
        };
        uniforms[0].value = texture;
        draw(shader, mesh, get_component<Transform>().transform, uniforms);
    }
};

struct Lifetime : public Component {
    float time = 1.0f;

    Lifetime(float t = 1.0f) : time(t) { }

    void update(float dt) override
    {
        time -= dt;
        if (time <= 0.0f) {
            parent->destroy();
        }
    }
};

struct Collider : public Component {
    float radius;

    Collider(float r) : radius(r) { }
};

struct Asteroid : public Component { };

struct Bullet : public Component { };

GameObjectId create_ship()
{
    auto ship = create_game_object();
    ship->add_component<Transform>().transform.setScale(glm::vec3(0.1f));
    ship->add_component<Velocity>();
    ship->add_component<Input>();
    ship->add_component<KeyboardControlled>();
    ship->add_component<Mesh>(get_ship_mesh(), get_ship_texture());
    return ship->id;
}

GameObjectId create_asteroid(const glm::vec3& position, const glm::vec3& velocity, float size)
{
    auto asteroid = create_game_object();

    asteroid->add_component<Collider>(size * 0.5f * 0.85f); // fudge factor for collider
    asteroid->add_component<Asteroid>();

    auto& trafo = asteroid->add_component<Transform>();
    trafo.transform.setPosition(position);
    trafo.transform.setScale(size);
    const auto orientation
        = glm::quat(randf(-1.0f, 1.0f), randf(-1.0f, 1.0f), randf(-1.0f, 1.0f), randf(-1.0f, 1.0f));
    trafo.transform.setOrientation(glm::normalize(orientation));

    asteroid->add_component<Velocity>(velocity);

    const auto meshes = get_asteroid_meshes();
    const auto mesh_idx = randi(0, meshes.size() - 1);
    asteroid->add_component<Mesh>(meshes[mesh_idx], get_asteroid_texture());

    return asteroid->id;
}

GameObjectId create_asteroid()
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

GameObjectId create_bullet(const glwx::Transform& ship_trafo)
{
    auto bullet = create_game_object();

    auto& trafo = bullet->add_component<Transform>(ship_trafo);
    trafo.transform.setScale(glm::vec3(1.0f));
    trafo.transform.move(
        -trafo.transform.getForward() * 0.5f); // move bullet slightly in front of the ship
    bullet->add_component<Velocity>(-trafo.transform.getForward() * 20.0f);
    bullet->add_component<Mesh>(get_bullet_mesh(), get_bullet_texture());
    bullet->add_component<Lifetime>(1.0f);
    bullet->add_component<Bullet>();
    bullet->add_component<Collider>(1.0f);

    return bullet->id;
}

void sys_collisions()
{
    auto& objs = game_objects();
    auto a_id = objs.next({});
    while (a_id) {
        auto& a = *objs.get(a_id);
        if (a.marked_for_destruction() || !a.try_get_component<Asteroid>()) {
            a_id = objs.next(a_id);
            continue;
        }

        auto& a_trafo = a.get_component<Transform>().transform;
        auto& a_vel = a.get_component<Velocity>().velocity;
        const auto a_radius = a.get_component<Collider>().radius;

        auto b_id = objs.next({});
        while (b_id) {
            auto& b = *objs.get(b_id);
            if (a_id == b_id || b.marked_for_destruction()) {
                b_id = objs.next(b_id);
                continue;
            }
            if (!b.try_get_component<Asteroid>() && !b.try_get_component<Bullet>()) {
                b_id = objs.next(b_id);
                continue;
            }

            auto& b_trafo = b.get_component<Transform>().transform;
            auto& b_vel = b.get_component<Velocity>().velocity;
            const auto b_radius = b.get_component<Collider>().radius;

            const auto rel = a_trafo.getPosition() - b_trafo.getPosition();
            const auto total_radius = a_radius + b_radius;
            if (glm::dot(rel, rel) < total_radius * total_radius) {
                if (b.try_get_component<Asteroid>()) {
                    collide_spheres(a_trafo, a_vel, a_radius, b_trafo, b_vel, b_radius);
                } else if (b.try_get_component<Bullet>()) {
                    a.destroy();
                    b.destroy();

                    if (a_radius < 0.5f) {
                        break;
                    }

                    const auto ortho = glm::normalize(glm::vec3(-b_vel.z, 0.0f, b_vel.x));
                    // 1/(2^(1/3)) times the origional radius should yield half the volume.
                    const auto radius = a_radius * 0.8f;
                    for (size_t i = 0; i < 2; ++i) {
                        const auto dir = static_cast<float>(i) * 2.0f - 1.0f;
                        const auto pos = a_trafo.getPosition() + dir * ortho * radius;
                        const auto vel = a_vel + dir * ortho * glm::length(a_vel);
                        create_asteroid(pos, vel, radius * 2.0f);
                    }

                    break;
                }
            }
            b_id = objs.next(b_id);
        }
        a_id = objs.next(a_id);
    }
    destroy_marked_for_destruction();
}

int main()
{
    auto window
        = glwx::makeWindow("Game Architecture Comparison - Unity Style", 1920, 1080).value();
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

        update<KeyboardControlled>(dt);
        update<Input>(dt);
        update<Lifetime>(dt);
        update<Velocity>(dt);
        sys_collisions();

        begin_frame();
        update<Mesh>(dt);
        end_frame();

        window.swap();
    }

    return 0;
}