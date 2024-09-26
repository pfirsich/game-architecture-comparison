#include <unordered_map>
#include <vector>

#include <glm/gtx/transform.hpp>

#include <glw/fmt.hpp>
#include <glw/state.hpp>
#include <glwx/transform.hpp>
#include <glwx/window.hpp>

#include "../classic-ecs/ecs.hpp"
#include "luax.hpp"
#include "shared.hpp"

template <typename T>
uint32_t key_to_int(SlotMapKey<T> v)
{
    return (v.gen() << 16) | v.idx();
}

template <typename T>
SlotMapKey<T> int_to_key(uint32_t v)
{
    return SlotMapKey<T>(v & 0xffff, v >> 16);
}

SlotMap<glwx::Transform>& transform_storage()
{
    static SlotMap<glwx::Transform> storage(32);
    return storage;
}

glwx::Transform& get_transform(lua_State* L, uint32_t id)
{
    auto ptr = transform_storage().find(int_to_key<glwx::Transform>(id));
    if (!ptr) {
        luax::error(L, "Invalid Transform ID {}", id);
    }
    assert(ptr);
    return *ptr;
}

int transform_create(lua_State* L)
{
    const auto key = transform_storage().insert({});
    return luax::ret(L, key_to_int(key));
}

int transform_destroy(lua_State* L)
{
    const auto [id] = luax::get_args<uint32_t>(L);
    if (!transform_storage().contains(int_to_key<glwx::Transform>(id))) {
        luax::error(L, "Invalid Transform ID {}", id);
    }
    transform_storage().remove(int_to_key<glwx::Transform>(id));
    return 0;
}

int transform_get_position(lua_State* L)
{
    const auto [id] = luax::get_args<uint32_t>(L);
    const auto pos = get_transform(L, id).getPosition();
    return luax::ret(L, pos.x, pos.y, pos.z);
}

int transform_get_orientation(lua_State* L)
{
    const auto [id] = luax::get_args<uint32_t>(L);
    const auto q = get_transform(L, id).getOrientation();
    return luax::ret(L, q.x, q.y, q.z, q.w);
}

int transform_get_scale(lua_State* L)
{
    const auto [id] = luax::get_args<uint32_t>(L);
    const auto s = get_transform(L, id).getScale();
    return luax::ret(L, s.x, s.y, s.z);
}

int transform_get_forward(lua_State* L)
{
    const auto [id] = luax::get_args<uint32_t>(L);
    const auto fwd = get_transform(L, id).getForward();
    return luax::ret(L, fwd.x, fwd.y, fwd.z);
}

int transform_set_position(lua_State* L)
{
    const auto [id, x, y, z] = luax::get_args<uint32_t, float, float, float>(L);
    get_transform(L, id).setPosition(glm::vec3(x, y, z));
    return 0;
}

int transform_set_scale(lua_State* L)
{
    const auto [id, scale] = luax::get_args<uint32_t, float>(L);
    get_transform(L, id).setScale(scale);
    return 0;
}

int transform_set_orientation(lua_State* L)
{
    const auto [id, x, y, z, w] = luax::get_args<uint32_t, float, float, float, float>(L);
    get_transform(L, id).setOrientation(glm::quat(w, x, y, z));
    return 0;
}

int transform_move(lua_State* L)
{
    const auto [id, x, y, z] = luax::get_args<uint32_t, float, float, float>(L);
    get_transform(L, id).move(glm::vec3(x, y, z));
    return 0;
}

int transform_rotate(lua_State* L)
{
    const auto [id, x, y, z, w] = luax::get_args<uint32_t, float, float, float, float>(L);
    get_transform(L, id).rotate(glm::quat(w, x, y, z));
    return 0;
}

struct CollisionSystem {
    struct Collision {
        uint32_t other;
        glm::vec3 normal;
        float depth;
    };

    struct Collider {
        glm::vec3 position;
        float radius;
        std::vector<Collision> collisions = {};
    };

    SlotMap<Collider> colliders;

    CollisionSystem() : colliders(1024) { }

    uint32_t create(float radius)
    {
        return key_to_int(colliders.insert(Collider { glm::vec3(0.0f), radius }));
    }

    void destroy(uint32_t id) { colliders.remove(int_to_key<Collider>(id)); }

    Collider& get_collider(uint32_t id)
    {
        auto collider = colliders.get(int_to_key<Collider>(id));
        assert(collider);
        return *collider;
    }

    const Collider& get_collider(uint32_t id) const
    {
        auto collider = colliders.get(int_to_key<Collider>(id));
        assert(collider);
        return *collider;
    }

    void set_position(uint32_t id, const glm::vec3& pos) { get_collider(id).position = pos; }

    void detect_collisions()
    {
        auto id = colliders.next({});
        while (id) {
            colliders.get(id)->collisions.clear();
            id = colliders.next(id);
        }

        auto a_id = colliders.next({});
        while (a_id) {
            auto& a = *colliders.get(a_id);

            auto b_id = colliders.next(a_id);
            while (b_id) {
                auto& b = *colliders.get(b_id);

                const auto rel = a.position - b.position;
                const auto total_radius = a.radius + b.radius;
                const auto dist2 = glm::dot(rel, rel);
                if (dist2 < total_radius * total_radius) {
                    const auto dist = glm::sqrt(dist2);
                    const auto n_rel = rel / dist;

                    // resolve
                    const auto depth = total_radius - dist;
                    a.collisions.push_back(
                        { .other = key_to_int(b_id), .normal = n_rel, .depth = depth });
                    b.collisions.push_back(
                        { .other = key_to_int(a_id), .normal = -n_rel, .depth = depth });
                }
                b_id = colliders.next(b_id);
            }
            a_id = colliders.next(a_id);
        }
    }

    uint32_t get_num_collisions(uint32_t id) const { return get_collider(id).collisions.size(); }

    Collision get_collision(uint32_t id, uint32_t idx) const
    {
        const auto collider = get_collider(id);
        assert(idx < collider.collisions.size());
        return collider.collisions[idx];
    }

    static CollisionSystem& instance()
    {
        static CollisionSystem sys;
        return sys;
    }
};

int detect_collisions(lua_State*)
{
    CollisionSystem::instance().detect_collisions();
    return 0;
}

int collider_create(lua_State* L)
{
    const auto [radius] = luax::get_args<float>(L);
    return luax::ret(L, CollisionSystem::instance().create(radius));
}

int collider_destroy(lua_State* L)
{
    const auto [id] = luax::get_args<uint32_t>(L);
    CollisionSystem::instance().destroy(id);
    return 0;
}

int collider_set_position(lua_State* L)
{
    const auto [id, x, y, z] = luax::get_args<uint32_t, float, float, float>(L);
    CollisionSystem::instance().set_position(id, glm::vec3(x, y, z));
    return 0;
}

int collider_get_num_collisions(lua_State* L)
{
    const auto [id] = luax::get_args<uint32_t>(L);
    return luax::ret(L, CollisionSystem::instance().get_num_collisions(id));
}

int collider_get_collision(lua_State* L)
{
    const auto [id, idx] = luax::get_args<uint32_t, uint32_t>(L);
    assert(idx > 0); // lua indices!
    const auto col = CollisionSystem::instance().get_collision(id, idx - 1);
    return luax::ret(L, col.other, col.normal.x, col.normal.y, col.normal.z, col.depth);
}

int get_scancode_down(lua_State* L)
{
    const auto [scancode] = luax::get_args<uint32_t>(L);
    static int num_keys = 0;
    static const auto kb_state = SDL_GetKeyboardState(&num_keys);
    return luax::ret(L, scancode < static_cast<uint32_t>(num_keys) && kb_state[scancode] > 0);
}

int lua_load_texture(lua_State* L)
{
    const auto [path] = luax::get_args<std::string_view>(L);
    return luax::ret(L, key_to_int(load_texture(path)));
}

int lua_load_mesh(lua_State* L)
{
    const auto [path, normalize] = luax::get_args<std::string_view, std::optional<bool>>(L, 1, 2);
    return luax::ret(L, key_to_int(load_obj_mesh(path, normalize.value_or(false))));
}

int lua_load_shader(lua_State* L)
{
    const auto [vert, frag] = luax::get_args<std::string_view, std::string_view>(L);
    return luax::ret(L, key_to_int(load_shader(vert, frag)));
}

int lua_begin_frame(lua_State*)
{
    begin_frame();
    return 0;
}

int lua_end_frame(lua_State*)
{
    end_frame();
    return 0;
}

int lua_draw(lua_State* L)
{
    const auto [shader, mesh, transform] = luax::get_args<uint32_t, uint32_t, uint32_t>(L, 4, 4);

    // Check if the first argument is a table
    if (!lua_istable(L, 4)) {
        return luaL_error(L, "Expected table as argument 4");
    }

    static std::array<Uniform, 16> uniform_array;
    size_t i = 0;

    lua_pushnil(L); // first key
    // Iterate over uniforms
    while (lua_next(L, 4) != 0) {
        // Key is at index -2 and value at index -1
        if (lua_type(L, -2) != LUA_TSTRING) {
            lua_pop(L, 1); // Remove value, keep key for next iteration
            continue; // Skip non-string keys
        }

        if (!luax::is_integer(L, -1)) {
            lua_pop(L, 1); // Remove value, keep key for next iteration
            continue; // Skip non-integer values
        }

        const char* key = lua_tostring(L, -2);
        const auto value = static_cast<uint32_t>(lua_tointeger(L, -1));

        uniform_array[i].loc = uniform_location(int_to_key<ShaderHandleTag>(shader), key);
        uniform_array[i].value = int_to_key<TextureHandleTag>(value);
        i++;

        // Remove value, keep key for next iteration
        lua_pop(L, 1);
    }

    draw(int_to_key<ShaderHandleTag>(shader), int_to_key<MeshHandleTag>(mesh),
        get_transform(L, transform), std::span<const Uniform>(uniform_array).first(i));

    return 0;
}

int lua_randi(lua_State* L)
{
    const auto [min, max] = luax::get_args<int, int>(L);
    return luax::ret(L, randi(min, max));
}

int lua_randf(lua_State* L)
{
    const auto [min, max] = luax::get_args<float, float>(L);
    return luax::ret(L, randf(min, max));
}

int lua_randb(lua_State* L)
{
    return luax::ret(L, randb());
}

int main()
{
    auto window = glwx::makeWindow("Game Architecture Comparison - Hybrid Lua", 1920, 1080).value();
    glw::State::instance().setViewport(window.getSize().x, window.getSize().y);

    init(static_cast<float>(window.getSize().x) / window.getSize().y);

    auto lua = luaL_newstate();
    luaL_openlibs(lua); // TODO: don't open io, os, ...

    auto bind_func = [&lua](lua_State* L, const char* name, lua_CFunction func) {
        lua_pushstring(L, name);
        lua_pushcfunction(L, func);
        lua_rawset(lua, -3);
    };

    lua_createtable(lua, 0, 20);

    bind_func(lua, "randi", lua_randi);
    bind_func(lua, "randf", lua_randf);
    bind_func(lua, "randb", lua_randb);

    bind_func(lua, "transform_create", transform_create);
    bind_func(lua, "transform_destroy", transform_destroy);
    bind_func(lua, "transform_get_position", transform_get_position);
    bind_func(lua, "transform_get_orientation", transform_get_orientation);
    bind_func(lua, "transform_get_scale", transform_get_scale);
    bind_func(lua, "transform_get_forward", transform_get_forward);
    bind_func(lua, "transform_set_position", transform_set_position);
    bind_func(lua, "transform_set_scale", transform_set_scale);
    bind_func(lua, "transform_set_orientation", transform_set_orientation);
    bind_func(lua, "transform_move", transform_move);
    bind_func(lua, "transform_rotate", transform_rotate);

    bind_func(lua, "detect_collisions", detect_collisions);
    bind_func(lua, "collider_create", collider_create);
    bind_func(lua, "collider_destroy", collider_destroy);
    bind_func(lua, "collider_set_position", collider_set_position);
    bind_func(lua, "collider_get_num_collisions", collider_get_num_collisions);
    bind_func(lua, "collider_get_collision", collider_get_collision);

    bind_func(lua, "get_scancode_down", get_scancode_down);

    bind_func(lua, "load_texture", lua_load_texture);
    bind_func(lua, "load_mesh", lua_load_mesh);
    bind_func(lua, "load_shader", lua_load_shader);
    bind_func(lua, "begin_frame", lua_begin_frame);
    bind_func(lua, "draw", lua_draw);
    bind_func(lua, "end_frame", lua_end_frame);

    lua_setglobal(lua, "engine");

    lua_pushcfunction(lua, luax::error_handler);

    auto res = luaL_loadfilex(lua, "hybrid-lua/main.lua", "bt");
    if (res) {
        fmt::println("Error in loadfilex: {}", lua_tostring(lua, -1));
        lua_error(lua);
    }

    const auto stack_before = lua_gettop(lua);
    res = lua_pcall(lua, 0, LUA_MULTRET, -2);
    if (res) {
        fmt::println("Error running main.lua: {}", lua_tostring(lua, -1));
        lua_error(lua);
    }
    const auto num_res = lua_gettop(lua) - (stack_before - 1); // -1 because of the function itself
    lua_pop(lua, num_res); // pop results

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

        lua_getglobal(lua, "update");
        lua_pushnumber(lua, dt);
        if (lua_pcall(lua, 1, 0, -3)) {
            fmt::println(stderr, "Error in update: {}", lua_tostring(lua, -1));
            return 1;
        }

        window.swap();
    }

    return 0;
}