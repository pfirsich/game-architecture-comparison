#include <vector>

#include <glm/gtx/transform.hpp>

#include <glw/fmt.hpp>
#include <glw/state.hpp>
#include <glwx/transform.hpp>
#include <glwx/window.hpp>

#include "../classic-ecs/ecs.hpp"
#include "shared.hpp"

struct Ship;
struct Asteroid;
struct Bullet;

struct ShipTag { };
struct AsteroidTag { };
struct BulletTag { };

struct Velocity {
    glm::vec3 value = glm::vec3(0.0f);
};

struct Collider {
    float radius;
};

struct Mesh {
    MeshHandle mesh;
    TextureHandle texture;
};

struct Entity {
    ecs::Entity id;
    glwx::Transform* transform;
    glm::vec3* velocity;
    Collider* collider;
    Mesh* mesh;
    bool flushed = false;
    bool destroyed = false;

    Entity()
        : id(ecs::create())
        , transform(&ecs::add<glwx::Transform>(id))
        , velocity(&ecs::add<Velocity>(id).value)
        , collider(&ecs::add<Collider>(id))
        , mesh(&ecs::add<Mesh>(id))
    {
        ecs::add<Entity*>(id, this);
    }

    Entity(Entity&& other)
        : id(std::exchange(other.id, ecs::Entity {}))
        , transform(other.transform)
        , velocity(other.velocity)
        , collider(other.collider)
        , mesh(other.mesh)
        , flushed(other.flushed)
        , destroyed(other.destroyed)
    {
        ecs::get<Entity*>(id) = this;
    }

    Entity& operator=(Entity&& other)
    {
        if (id) {
            ecs::destroy(id);
        }
        id = std::exchange(other.id, ecs::Entity {});
        transform = other.transform;
        velocity = other.velocity;
        collider = other.collider;
        mesh = other.mesh;
        flushed = other.flushed;
        destroyed = other.destroyed;
        ecs::get<Entity*>(id) = this;
        return *this;
    }

    ~Entity()
    {
        if (id) {
            ecs::destroy(id);
        }
    }

    virtual void on_collision(ecs::Entity) {};

    void destroy() { destroyed = true; }
    bool alive() const { return flushed && !destroyed; }
};

template <typename T>
std::vector<T>& get_entities()
{
    static std::vector<T> entities;
    return entities;
}

template <typename T>
std::vector<T>& new_entities()
{
    static std::vector<T> entities;
    return entities;
}

template <typename T>
void flush_entities()
{
    auto& entities = get_entities<T>();
    for (auto it = entities.begin(); it != entities.end();) {
        if (it->destroyed) {
            it = entities.erase(it);
        } else {
            it->flushed = true;
            ++it;
        }
    }
    for (auto& e : new_entities<T>()) {
        entities.push_back(std::move(e));
    }
    new_entities<T>().clear();
}

struct Bullet final : public Entity {
    float lifetime = 1.0f;

    Bullet(const glwx::Transform& ship_trafo)
    {
        *transform = ship_trafo;
        transform->setScale(1.0f);
        transform->move(
            -transform->getForward() * 0.5f); // move bullet slightly in front of the ship
        *velocity = -transform->getForward() * 20.0f;
        mesh->mesh = get_bullet_mesh();
        mesh->texture = get_bullet_texture();
        collider->radius = 1.0f;
        ecs::add<BulletTag>(id);
    }

    void update(float dt)
    {
        lifetime -= dt;
        if (lifetime <= 0.0f) {
            destroy();
        }
    }

    void on_collision(Asteroid&) { destroy(); }

    void on_collision(ecs::Entity other)
    {
        if (ecs::has<AsteroidTag>(other)) {
            // reinterpret_cast because Asteroid is incomplete here
            on_collision(*reinterpret_cast<Asteroid*>(ecs::get<Entity*>(other)));
        }
    }
};

struct Ship final : public Entity {
    BinaryInput shoot;

    Ship()
    {
        transform->setScale(0.1f);
        mesh->mesh = get_ship_mesh();
        mesh->texture = get_ship_texture();
        collider = nullptr;
        ecs::remove<Collider>(id);
        ecs::add<ShipTag>(id);
    }

    void update(float dt)
    {
        // control
        static int num_keys = 0;
        static const auto kb_state = SDL_GetKeyboardState(&num_keys);

        // control
        const auto accel = kb_state[SDL_SCANCODE_W] > 0;
        if (accel) {
            *velocity += -transform->getForward() * dt * 2.0f;
        }

        const auto turn = kb_state[SDL_SCANCODE_A] - kb_state[SDL_SCANCODE_D];
        const auto quat = glm::angleAxis(
            turn * glm::pi<float>() * 2.0f * dt, glm::vec3(0.0f, 1.0f, 0.0f) * 0.5f);
        transform->setOrientation(quat * transform->getOrientation());

        shoot.update(kb_state[SDL_SCANCODE_SPACE]);
        if (shoot.pressed()) {
            new_entities<Bullet>().push_back(Bullet(*transform));
        }
    }
};

struct Asteroid final : public Entity {
    Asteroid(const glm::vec3& position, const glm::vec3& velocity, float size)
    {
        init(position, velocity, size);
    }

    Asteroid() { init(); }

    void init(const glm::vec3& pos, const glm::vec3& vel, float size)
    {
        collider->radius = size * 0.5f * 0.85f; // fudge factor for collider
        *velocity = vel;

        transform->setPosition(pos);
        transform->setScale(size);
        const auto orientation = glm::quat(
            randf(-1.0f, 1.0f), randf(-1.0f, 1.0f), randf(-1.0f, 1.0f), randf(-1.0f, 1.0f));
        transform->setOrientation(glm::normalize(orientation));

        const auto meshes = get_asteroid_meshes();
        const auto mesh_idx = randi(0, meshes.size() - 1);
        mesh->mesh = meshes[mesh_idx];
        mesh->texture = get_asteroid_texture();

        ecs::add<AsteroidTag>(id);
    }

    void init()
    {
        const auto edge = (randi(0, 1) * 2 - 1) * 0.4f * view_bounds_size;
        const auto axis_pos = randf(-0.5f, 0.5f) * view_bounds_size;
        const auto pos
            = randb() ? glm::vec3(axis_pos.x, 0.0f, edge.y) : glm::vec3(edge.x, 0.0f, axis_pos.y);

        const auto angle = randf(0.0f, glm::pi<float>() * 2.0f);
        const auto speed = randf(1.0f, 3.0f);
        const auto vel = glm::vec3(glm::cos(angle), 0.0f, glm::sin(angle)) * speed;

        const auto size = randf(1.0f, 5.0f);

        init(pos, vel, size);
    }

    void update(float) { }

    void on_collision(Asteroid& other)
    {
        // Bit hacky, but we only want to execute this once
        if (this < &other) {
            collide_spheres(*transform, *velocity, collider->radius, *other.transform,
                *other.velocity, other.collider->radius);
        }
    }

    void on_collision(Bullet& b)
    {
        if (collider->radius > 0.5f) {
            const auto ortho = glm::normalize(glm::vec3(-b.velocity->z, 0.0f, b.velocity->x));
            // 1/(2^(1/3)) times the origional radius should yield half the volume.
            const auto radius = collider->radius * 0.8f;
            for (size_t i = 0; i < 2; ++i) {
                const auto dir = static_cast<float>(i) * 2.0f - 1.0f;
                const auto pos = transform->getPosition() + dir * ortho * radius;
                const auto vel = (*velocity + dir * ortho * glm::length(*velocity));
                new_entities<Asteroid>().emplace_back(pos, vel, radius * 2.0f);
            }
        }
        destroy();
    }

    void on_collision(ecs::Entity other)
    {
        if (ecs::has<AsteroidTag>(other)) {
            on_collision(*static_cast<Asteroid*>(ecs::get<Entity*>(other)));
        } else if (ecs::has<BulletTag>(other)) {
            on_collision(*static_cast<Bullet*>(ecs::get<Entity*>(other)));
        }
    }
};

void sys_collision()
{
    ecs::for_each_pair<glwx::Transform, Collider>([&](ecs::Entity a, ecs::Entity b) {
        auto a_ent = ecs::get<Entity*>(a);
        auto b_ent = ecs::get<Entity*>(b);
        if (!a_ent->alive() || !b_ent->alive()) {
            return;
        }

        const auto rel = ecs::get<glwx::Transform>(b).getPosition()
            - ecs::get<glwx::Transform>(a).getPosition();
        const auto total_radius = ecs::get<Collider>(a).radius + ecs::get<Collider>(b).radius;
        if (glm::dot(rel, rel) < total_radius * total_radius) {
            a_ent->on_collision(b);
            b_ent->on_collision(a);
        }
    });
    flush_entities<Asteroid>();
    flush_entities<Bullet>();
}

void sys_physics(float dt)
{
    ecs::for_each<glwx::Transform, Velocity>([dt](ecs::Entity entity) {
        auto& transform = ecs::get<glwx::Transform>(entity);
        auto& velocity = ecs::get<Velocity>(entity).value;
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
    });
}

void sys_render()
{
    const auto shader = get_shader();
    std::array<Uniform, 1> uniforms {
        Uniform { uniform_location(get_shader(), "u_texture"), TextureHandle {} },
    };
    ecs::for_each<glwx::Transform, Mesh>([&](ecs::Entity entity) {
        auto& transform = ecs::get<glwx::Transform>(entity);
        auto& mesh = ecs::get<Mesh>(entity);
        uniforms[0].value = mesh.texture;
        draw(shader, mesh.mesh, transform, uniforms);
    });
}

int main()
{
    auto window = glwx::makeWindow("Game Architecture Comparison - Hybrid", 1920, 1080).value();
    glw::State::instance().setViewport(window.getSize().x, window.getSize().y);

    Ship ship;
    ship.flushed = true;

    for (size_t i = 0; i < 12; ++i) {
        get_entities<Asteroid>().emplace_back();
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

        ship.update(dt);
        for (auto& e : get_entities<Asteroid>()) {
            e.update(dt);
        }
        for (auto& e : get_entities<Bullet>()) {
            e.update(dt);
        }
        flush_entities<Asteroid>();
        flush_entities<Bullet>();
        sys_physics(dt);
        sys_collision();

        begin_frame();
        sys_render();
        end_frame();

        window.swap();
    }

    return 0;
}