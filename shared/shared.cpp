#include "shared.hpp"

#include <string>

#include <fmt/core.h>

#include <glw/buffer.hpp>
#include <glwx/shader.hpp>
#include <glwx/texture.hpp>

#define TINYOBJLOADER_IMPLEMENTATION // define this in only *one* .cc
#include "tiny_obj_loader.h"

namespace {
auto& get_texture_storage()
{
    static SlotMap<glw::Texture, TextureHandle> storage(64);
    return storage;
}

auto& get_mesh_storage()
{
    static SlotMap<glwx::Primitive, MeshHandle> storage(64);
    return storage;
}

auto& get_buffer_storage()
{
    static SlotMap<glw::Buffer> storage(64);
    return storage;
}

auto& get_shader_storage()
{
    static SlotMap<glw::ShaderProgram, ShaderHandle> storage(64);
    return storage;
}
}

TextureHandle load_texture(std::string_view path)
{
    auto tex = glwx::makeTexture2D(std::filesystem::path(path));
    if (!tex) {
        fmt::println(stderr, "Could not load texture from '{}'", path);
        std::exit(1);
    }
    return get_texture_storage().insert(std::move(tex.value()));
}

MeshHandle load_obj_mesh(std::string_view path, bool normalize)
{
    tinyobj::ObjReaderConfig reader_config;
    reader_config.triangulate = true;

    tinyobj::ObjReader reader;
    if (!reader.ParseFromFile(std::string(path), reader_config)) {
        if (!reader.Error().empty()) { }
        std::exit(1);
    }

    if (!reader.Warning().empty()) {
        fmt::println(stderr, "TinyObjReader warning for OBJ file '{}': {}", path, reader.Warning());
    }

    const auto vertex_count = [&reader]() {
        usize n = 0;
        for (const auto& shape : reader.GetShapes()) {
            for (const auto& num_face_vertices : shape.mesh.num_face_vertices) {
                n += num_face_vertices;
            }
        }
        return n;
    }();

    struct Vertex {
        glm::vec3 position;
        glm::vec3 normal = glm::vec3(0.0f);
        glm::vec2 texcoord = glm::vec2(0.0f);
        glm::vec3 color = glm::vec3(1.0f);
    };

    std::vector<Vertex> vertex_data(vertex_count);
    usize vert_idx = 0;

    const auto& attrib = reader.GetAttrib();
    for (const auto& shape : reader.GetShapes()) {
        const auto num_faces = shape.mesh.num_face_vertices.size();
        usize indices_offset = 0;
        for (usize f = 0; f < num_faces; ++f) {
            const auto num_face_vertices = shape.mesh.num_face_vertices[f];
            // all faces should be triangles, because we passed it in the config
            assert(num_face_vertices == 3);

            for (usize v = 0; v < num_face_vertices; ++v) {
                assert(vert_idx < vertex_count);
                const auto& idx = shape.mesh.indices[indices_offset++];
                vertex_data[vert_idx].position
                    = glm::make_vec3(attrib.vertices.data() + 3 * idx.vertex_index);
                vertex_data[vert_idx].color
                    = glm::make_vec3(attrib.colors.data() + 3 * idx.vertex_index);
                if (idx.normal_index >= 0) {
                    vertex_data[vert_idx].normal
                        = glm::make_vec3(attrib.normals.data() + 3 * idx.normal_index);
                }
                if (idx.texcoord_index >= 0) {
                    vertex_data[vert_idx].texcoord
                        = glm::make_vec2(attrib.texcoords.data() + 2 * idx.texcoord_index);
                }
                vert_idx++;
            }
        }
    }

    if (normalize) {
        glm::vec3 center = glm::vec3(0.0f);
        for (const auto& v : vertex_data) {
            center += v.position;
        }
        center /= vertex_data.size();

        float sqr_radius = 0.0f;
        for (const auto& v : vertex_data) {
            const auto rel = v.position - center;
            const auto sqr_dist = glm::dot(rel, rel);
            sqr_radius = std::max(sqr_radius, sqr_dist);
        }

        const auto radius = glm::sqrt(sqr_radius);
        for (auto& v : vertex_data) {
            v.position -= center;
            v.position /= radius * 2.0f;
        }
    }

    auto& buffers = get_buffer_storage();
    auto vertex_buffer = buffers.get(buffers.insert(glw::Buffer()));
    vertex_buffer->data(
        glw::Buffer::Target::Array, glw::Buffer::UsageHint::StaticDraw, vertex_data);

    static glw::VertexFormat vfmt {
        { 0, 3, glw::AttributeType::F32 }, // position
        { 1, 3, glw::AttributeType::F32 }, // normal
        { 2, 2, glw::AttributeType::F32 }, // texcoord
        { 3, 3, glw::AttributeType::F32 }, // color
    };

    auto prim = glwx::Primitive(glw::DrawMode::Triangles);
    prim.addVertexBuffer(*vertex_buffer, vfmt);

    return get_mesh_storage().insert(std::move(prim));
}

ShaderHandle load_shader(std::string_view vert_path, std::string_view frag_path)
{
    auto prog = glwx::makeShaderProgram(
        std::filesystem::path(vert_path), std::filesystem::path(frag_path));
    if (!prog) {
        fmt::println(stderr, "Could not load shader from '{}'/'{}'", vert_path, frag_path);
        std::exit(1);
    }
    return get_shader_storage().insert(std::move(prog.value()));
}

glw::ShaderProgram::UniformLocation uniform_location(ShaderHandle handle, std::string_view name)
{
    return get_shader_storage().get(handle)->getUniformLocation(std::string(name));
}

ShaderHandle get_shader()
{
    static auto shader = load_shader("assets/vert.glsl", "assets/frag.glsl");
    return shader;
}

MeshHandle get_ship_mesh()
{
    static auto mesh = load_obj_mesh("assets/Spaceship_FernandoTheFlamingo.obj");
    return mesh;
}

TextureHandle get_ship_texture()
{
    static auto texture = load_texture("assets/Atlas.png");
    return texture;
}

std::span<MeshHandle> get_asteroid_meshes()
{
    static std::array<MeshHandle, 3> meshes;
    static bool initialized = false;
    if (!initialized) {
        meshes[0] = load_obj_mesh("assets/Rock_1.obj", true);
        meshes[1] = load_obj_mesh("assets/Rock_2.obj", true);
        meshes[2] = load_obj_mesh("assets/Rock_3.obj", true);
        initialized = true;
    }
    return meshes;
}

TextureHandle get_asteroid_texture()
{
    return get_ship_texture();
}

MeshHandle get_bullet_mesh()
{
    static auto mesh = load_obj_mesh("assets/laser.obj");
    return mesh;
}

TextureHandle get_bullet_texture()
{
    static auto texture = load_texture("assets/laser.png");
    return texture;
}

int randi(int min, int max)
{
    return min + std::rand() % (max + 1 - min);
}

float randf(float min, float max)
{
    return min + std::rand() / static_cast<float>(RAND_MAX) * (max - min);
}

bool randb()
{
    return std::rand() % 2 == 0;
}

void collide_spheres(glwx::Transform& a_trafo, glm::vec3& a_vel, float a_rad,
    glwx::Transform& b_trafo, glm::vec3& b_vel, float b_rad)
{
    const auto rel = b_trafo.getPosition() - a_trafo.getPosition();
    const auto dist2 = glm::dot(rel, rel);
    const auto dist = glm::sqrt(dist2);
    const auto n_rel = rel / dist;

    // resolve
    const auto depth = a_rad + b_rad - dist;
    a_trafo.move(depth * 0.5f * -n_rel);
    b_trafo.move(depth * 0.5f * n_rel);

    // reflect
    const auto v_rel = b_vel - a_vel;
    const auto c = 2.0f * glm::dot(v_rel, n_rel);
    // assume masses are proportional to volume
    const auto a_mass = a_rad * a_rad * a_rad;
    const auto b_mass = b_rad * b_rad * b_rad;
    a_vel += c / (1.0f + a_mass / b_mass) * n_rel;
    b_vel -= c / (1.0f + b_mass / a_mass) * n_rel;
}

namespace {
struct State {
    glm::mat4 projection_matrix;
    glm::mat4 view_matrix;
    glm::vec3 light_dir;
};

State& get_state()
{
    static State state;
    return state;
}
}

void init(float aspect)
{
    auto& state = get_state();

    glwx::Transform camera_trafo;
    // Look down on XZ plane (up is +Z)
    camera_trafo.setPosition(glm::vec3(0.0f, 15.0f, 0.0f));
    camera_trafo.lookAt(glm::vec3(0.0f, 0.0f, 0.0f), glm::vec3(0.0f, 0.0f, 1.0f));

    // Look down on XZ plane (up is +Z)
    state.view_matrix = glm::inverse(camera_trafo.getMatrix());
    state.projection_matrix = glm::perspective(glm::radians(45.0f), aspect, 0.1f, 100.0f);
    state.light_dir = glm::normalize(glm::vec3(0.0f, 1.0f, 1.0f));

    glEnable(GL_DEPTH_TEST);

    glw::State::instance().setBlendEnabled(true);
    glw::State::instance().setBlendFunc(glw::BlendFunc::SrcAlpha, glw::BlendFunc::OneMinusSrcAlpha);

    // Bullets need to be double-sided, but to keep it simplel we just make everything double-sided
    glw::State::instance().setCullFaceEnabled(false);
}

void begin_frame()
{
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
}

namespace {
struct UniformValueVisitor {
    glw::ShaderProgram& prog;
    glw::ShaderProgram::UniformLocation loc;
    i32 unit = 0;

    void operator()(TextureHandle texture)
    {
        get_texture_storage().get(texture)->bind(unit);
        prog.setUniform(loc, unit);
        unit++;
    }

    template <typename T>
    void operator()(const T* v)
    {
        prog.setUniform(loc, *v);
    }
};

void apply_uniforms(glw::ShaderProgram& prog, std::span<const Uniform> uniforms)
{
    for (const auto& uniform : uniforms) {
        std::visit(UniformValueVisitor { prog, uniform.loc }, uniform.value);
    }
}
}

void draw(ShaderHandle shader, MeshHandle mesh, const glwx::Transform& trafo,
    std::span<const Uniform> uniforms)
{
    auto& state = get_state();

    auto& prog = *get_shader_storage().get(shader);
    prog.bind();

    const auto model = trafo.getMatrix();
    const auto model_view = state.view_matrix * model;
    const auto normal = glm::transpose(glm::inverse(glm::mat3(model_view)));
    prog.setUniform("u_model", model);
    prog.setUniform("u_normal", normal);
    prog.setUniform("u_view", state.view_matrix);
    prog.setUniform("u_projection", state.projection_matrix);
    prog.setUniform("u_light_dir", state.light_dir);

    apply_uniforms(prog, uniforms);

    get_mesh_storage().get(mesh)->draw();
}

void end_frame() { }