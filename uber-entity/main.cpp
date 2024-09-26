#include <vector>

#include <glm/gtx/transform.hpp>

#include <glw/fmt.hpp>
#include <glw/state.hpp>
#include <glwx/transform.hpp>
#include <glwx/window.hpp>

#include "shared.hpp"

struct Entity {
    enum class Type { Ship, Asteroid, Bullet };

    Type type;
    glwx::Transform transform;
    glm::vec3 velocity = glm::vec3(0.0f);
    float radius;
    MeshHandle mesh;
    TextureHandle texture;
    BinaryInput shoot;
    float lifetime = 1.0f;
    bool marked_for_delection = false;

    Entity(Type t) : type(t) { }

    void destroy() { marked_for_delection = true; }

    void draw() const
    {
        static const auto shader = get_shader();
        static std::array<Uniform, 1> uniforms {
            Uniform { uniform_location(get_shader(), "u_texture"), TextureHandle {} },
        };

        uniforms[0].value = texture;
        ::draw(shader, mesh, transform, uniforms);
    }

    void integrate(float dt)
    {
        auto pos = transform.getPosition() + velocity * dt;

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
        transform.setPosition(pos);
    }
};

std::vector<Entity>& get_entities()
{
    static std::vector<Entity> entities;
    return entities;
}

std::vector<Entity>& new_entities()
{
    static std::vector<Entity> entities;
    return entities;
}

void destroy_marked_for_deletion()
{
    auto& entities = get_entities();
    for (auto it = entities.begin(); it != entities.end();) {
        if (it->marked_for_delection) {
            it = entities.erase(it);
        } else {
            ++it;
        }
    }
}

Entity create_bullet(const glwx::Transform& ship_trafo)
{
    Entity e(Entity::Type::Bullet);
    e.transform = ship_trafo;
    e.transform.setScale(1.0f);
    e.transform.move(-e.transform.getForward() * 0.5f); // move bullet slightly in front of the ship
    e.velocity = -e.transform.getForward() * 20.0f;
    e.mesh = get_bullet_mesh();
    e.texture = get_bullet_texture();
    e.radius = 1.0f;
    return e;
}

void update_bullet(Entity& e, float dt)
{
    assert(e.type == Entity::Type::Bullet);
    e.lifetime -= dt;
    if (e.lifetime <= 0.0f) {
        e.destroy();
    }
    e.integrate(dt);
}

Entity create_ship()
{
    Entity e(Entity::Type::Ship);
    e.transform.setScale(0.1f);
    e.mesh = get_ship_mesh();
    e.texture = get_ship_texture();
    e.radius = 1.0f;
    return e;
}

void update_ship(Entity& e, float dt)
{
    // control
    static int num_keys = 0;
    static const auto kb_state = SDL_GetKeyboardState(&num_keys);

    // control
    const auto accel = kb_state[SDL_SCANCODE_W] > 0;
    if (accel) {
        e.velocity += -e.transform.getForward() * dt * 2.0f;
    }

    const auto turn = kb_state[SDL_SCANCODE_A] - kb_state[SDL_SCANCODE_D];
    const auto quat
        = glm::angleAxis(turn * glm::pi<float>() * 2.0f * dt, glm::vec3(0.0f, 1.0f, 0.0f) * 0.5f);
    e.transform.setOrientation(quat * e.transform.getOrientation());

    e.shoot.update(kb_state[SDL_SCANCODE_SPACE]);
    if (e.shoot.pressed()) {
        new_entities().push_back(create_bullet(e.transform));
    }

    e.integrate(dt);
}

Entity create_asteroid(const glm::vec3& pos, const glm::vec3& vel, float size)
{
    Entity e(Entity::Type::Asteroid);

    e.radius = size * 0.5f * 0.85f; // fudge factor for collider

    e.transform.setPosition(pos);
    e.transform.setScale(size);
    const auto orientation
        = glm::quat(randf(-1.0f, 1.0f), randf(-1.0f, 1.0f), randf(-1.0f, 1.0f), randf(-1.0f, 1.0f));
    e.transform.setOrientation(glm::normalize(orientation));

    const auto meshes = get_asteroid_meshes();
    const auto mesh_idx = randi(0, meshes.size() - 1);
    e.mesh = meshes[mesh_idx];

    e.velocity = vel;

    return e;
}

Entity create_asteroid()
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

void update_asteroid(Entity& e, float dt)
{
    assert(e.type == Entity::Type::Asteroid);
    e.integrate(dt);
}

void collide_asteroid_asteroid(Entity& a, Entity& b)
{
    collide_spheres(a.transform, a.velocity, a.radius, b.transform, b.velocity, b.radius);
}

void collide_asteroid_bullet(Entity& a, Entity& b)
{
    a.destroy();
    b.destroy();

    if (a.radius < 0.5f) {
        return;
    }

    const auto ortho = glm::normalize(glm::vec3(-b.velocity.z, 0.0f, b.velocity.x));
    // 1/(2^(1/3)) times the origional radius should yield half the volume.
    const auto radius = a.radius * 0.8f;
    for (size_t i = 0; i < 2; ++i) {
        const auto dir = static_cast<float>(i) * 2.0f - 1.0f;
        const auto pos = a.transform.getPosition() + dir * ortho * radius;
        const auto vel = (a.velocity + dir * ortho * glm::length(a.velocity));
        new_entities().push_back(create_asteroid(pos, vel, radius * 2.0f));
    }
}

void sys_collisions()
{
    auto& entities = get_entities();
    for (auto& a : entities) {
        if (a.type != Entity::Type::Asteroid) {
            continue;
        }

        for (auto& b : entities) {
            if (&a == &b) {
                continue;
            }

            const auto rel = a.transform.getPosition() - b.transform.getPosition();
            const auto total_radius = a.radius + b.radius;
            if (glm::dot(rel, rel) < total_radius * total_radius) {
                if (b.type == Entity::Type::Asteroid) {
                    collide_asteroid_asteroid(a, b);
                } else if (b.type == Entity::Type::Bullet) {
                    collide_asteroid_bullet(a, b);
                    break;
                }
            }
        }
    }
    destroy_marked_for_deletion();
}

int main()
{
    auto window
        = glwx::makeWindow("Game Architecture Comparison - Uber-Entity", 1920, 1080).value();
    glw::State::instance().setViewport(window.getSize().x, window.getSize().y);

    get_entities().emplace_back(create_ship());

    for (size_t i = 0; i < 12; ++i) {
        get_entities().emplace_back(create_asteroid());
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

        for (auto& entity : get_entities()) {
            if (!entity.marked_for_delection) {
                switch (entity.type) {
                case Entity::Type::Ship:
                    update_ship(entity, dt);
                    break;
                case Entity::Type::Asteroid:
                    update_asteroid(entity, dt);
                    break;
                case Entity::Type::Bullet:
                    update_bullet(entity, dt);
                    break;
                }
            }
        }

        sys_collisions();

        destroy_marked_for_deletion();

        for (auto& e : new_entities()) {
            get_entities().push_back(std::move(e));
        }
        new_entities().clear();

        begin_frame();
        for (const auto& entity : get_entities()) {
            entity.draw();
        }
        end_frame();

        window.swap();
    }

    return 0;
}