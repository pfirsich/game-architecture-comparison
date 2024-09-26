#pragma once

#include <span>
#include <string_view>
#include <variant>

#include <cppasta/generational_index.hpp>
#include <cppasta/primitive_typedefs.hpp>
#include <cppasta/slot_map.hpp>
#include <glw/shader.hpp>
#include <glw/texture.hpp>
#include <glwx/primitive.hpp>
#include <glwx/transform.hpp>

constexpr glm::vec2 view_bounds_size(28.0f, 17.0f);

struct BinaryInput {
    bool state = false;
    bool last_state = false;

    void update(bool s)
    {
        last_state = state;
        state = s;
    }

    bool pressed() const { return state && !last_state; }
};

template <typename T, typename Key>
using PagedStorage = pasta::PagedSlotMapStorage<T, Key, std::vector<uint16_t>, std::allocator>;

template <typename T>
using SlotMapKey = pasta::CompositeId<T, uint16_t, uint16_t>;

template <typename T, typename Key = SlotMapKey<T>>
using SlotMap = pasta::SlotMap<T, PagedStorage, Key>;

struct TextureHandleTag { };
using TextureHandle = SlotMapKey<TextureHandleTag>;
TextureHandle load_texture(std::string_view path);

struct MeshHandleTag { };
using MeshHandle = SlotMapKey<MeshHandleTag>;
MeshHandle load_obj_mesh(std::string_view path, bool normalize = false);

struct ShaderHandleTag { };
using ShaderHandle = SlotMapKey<ShaderHandleTag>;
ShaderHandle load_shader(std::string_view vert_path, std::string_view frag_path);
glw::ShaderProgram::UniformLocation uniform_location(ShaderHandle handle, std::string_view name);

ShaderHandle get_shader();
MeshHandle get_ship_mesh();
TextureHandle get_ship_texture();
std::span<MeshHandle> get_asteroid_meshes();
TextureHandle get_asteroid_texture();
MeshHandle get_bullet_mesh();
TextureHandle get_bullet_texture();

int randi(int min, int max);
float randf(float min, float max);
bool randb();

void collide_spheres(glwx::Transform& a_trafo, glm::vec3& a_vel, float a_rad,
    glwx::Transform& b_trafo, glm::vec3& b_vel, float b_rad);

struct Uniform {
    glw::ShaderProgram::UniformLocation loc;
    std::variant<TextureHandle> value;
};

void init(float aspect);
void begin_frame();
void draw(ShaderHandle shader, MeshHandle mesh, const glwx::Transform& trafo,
    std::span<const Uniform> uniforms);
void end_frame();