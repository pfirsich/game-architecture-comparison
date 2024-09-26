#include <vector>

#include <glm/gtx/transform.hpp>

#include <glw/fmt.hpp>
#include <glw/state.hpp>
#include <glwx/transform.hpp>
#include <glwx/window.hpp>

#include "shared.hpp"

struct Entity {
    glwx::Transform transform;
    glm::vec3 velocity = glm::vec3(0.0f);
    float radius;
    MeshHandle mesh;
    TextureHandle texture;
    bool marked_for_delection = false;

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
void flush_new_entities()
{
    for (auto& e : new_entities<T>()) {
        get_entities<T>().push_back(std::move(e));
    }
    new_entities<T>().clear();
}

template <typename T>
void destroy_marked_for_deletion()
{
    auto& entities = get_entities<T>();
    for (auto it = entities.begin(); it != entities.end();) {
        if (it->marked_for_delection) {
            it = entities.erase(it);
        } else {
            ++it;
        }
    }
}

struct Bullet final : public Entity {
    float lifetime = 1.0f;

    Bullet(const glwx::Transform& ship_trafo)
    {
        transform = ship_trafo;
        transform.setScale(1.0f);
        transform.move(-transform.getForward() * 0.5f); // move bullet slightly in front of the ship
        velocity = -transform.getForward() * 20.0f;
        mesh = get_bullet_mesh();
        texture = get_bullet_texture();
        radius = 1.0f;
    }

    void update(float dt)
    {
        lifetime -= dt;
        if (lifetime <= 0.0f) {
            destroy();
        }
        integrate(dt);
    }
};

struct Ship final : public Entity {
    BinaryInput shoot;

    Ship()
    {
        transform.setScale(0.1f);
        mesh = get_ship_mesh();
        texture = get_ship_texture();
        radius = 1.0f;
    }

    void update(float dt)
    {
        // control
        static int num_keys = 0;
        static const auto kb_state = SDL_GetKeyboardState(&num_keys);

        // control
        const auto accel = kb_state[SDL_SCANCODE_W] > 0;
        if (accel) {
            velocity += -transform.getForward() * dt * 2.0f;
        }

        const auto turn = kb_state[SDL_SCANCODE_A] - kb_state[SDL_SCANCODE_D];
        const auto quat = glm::angleAxis(
            turn * glm::pi<float>() * 2.0f * dt, glm::vec3(0.0f, 1.0f, 0.0f) * 0.5f);
        transform.setOrientation(quat * transform.getOrientation());

        shoot.update(kb_state[SDL_SCANCODE_SPACE]);
        if (shoot.pressed()) {
            get_entities<Bullet>().push_back(Bullet(transform));
        }

        integrate(dt);
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
        radius = size * 0.5f * 0.85f; // fudge factor for collider

        transform.setPosition(pos);
        transform.setScale(size);
        const auto orientation = glm::quat(
            randf(-1.0f, 1.0f), randf(-1.0f, 1.0f), randf(-1.0f, 1.0f), randf(-1.0f, 1.0f));
        transform.setOrientation(glm::normalize(orientation));

        const auto meshes = get_asteroid_meshes();
        const auto mesh_idx = randi(0, meshes.size() - 1);
        mesh = meshes[mesh_idx];
        texture = get_asteroid_texture();

        velocity = vel;
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

    void update(float dt) { integrate(dt); }
};

void collide_asteroid_bullet()
{
    for (auto& a : get_entities<Asteroid>()) {
        for (auto& b : get_entities<Bullet>()) {
            const auto rel = a.transform.getPosition() - b.transform.getPosition();
            const auto total_radius = a.radius + b.radius;
            if (glm::dot(rel, rel) < total_radius * total_radius) {
                a.destroy();
                b.destroy();

                if (a.radius < 0.5f) {
                    break;
                }

                const auto ortho = glm::normalize(glm::vec3(-b.velocity.z, 0.0f, b.velocity.x));
                // 1/(2^(1/3)) times the origional radius should yield half the volume.
                const auto radius = a.radius * 0.8f;
                for (size_t i = 0; i < 2; ++i) {
                    const auto dir = static_cast<float>(i) * 2.0f - 1.0f;
                    const auto pos = a.transform.getPosition() + dir * ortho * radius;
                    const auto vel = (a.velocity + dir * ortho * glm::length(a.velocity));
                    new_entities<Asteroid>().emplace_back(pos, vel, radius * 2.0f);
                }

                break;
            }
        }
    }
    destroy_marked_for_deletion<Asteroid>();
    destroy_marked_for_deletion<Bullet>();
    flush_new_entities<Asteroid>();
}

void collide_asteroid_asteroid()
{
    auto& asteroids = get_entities<Asteroid>();
    for (auto a_it = asteroids.begin(); a_it != asteroids.end(); ++a_it) {
        auto& a = *a_it;
        for (auto b_it = a_it + 1; b_it != asteroids.end(); ++b_it) {
            auto& b = *b_it;
            const auto rel = a.transform.getPosition() - b.transform.getPosition();
            const auto total_radius = a.radius + b.radius;
            if (glm::dot(rel, rel) < total_radius * total_radius) {
                collide_spheres(
                    a.transform, a.velocity, a.radius, b.transform, b.velocity, b.radius);
            }
        }
    }
}

int main()
{
    auto window
        = glwx::makeWindow("Game Architecture Comparison - No Polymorphism", 1920, 1080).value();
    glw::State::instance().setViewport(window.getSize().x, window.getSize().y);

    Ship ship;

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
        destroy_marked_for_deletion<Bullet>();

        collide_asteroid_asteroid();
        collide_asteroid_bullet();

        begin_frame();
        ship.draw();
        for (auto& e : get_entities<Asteroid>()) {
            e.draw();
        }
        for (auto& e : get_entities<Bullet>()) {
            e.draw();
        }
        end_frame();

        window.swap();
    }

    return 0;
}