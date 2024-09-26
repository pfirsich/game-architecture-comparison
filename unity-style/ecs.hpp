#pragma once

#include <array>
#include <memory>
#include <vector>

#include <fmt/format.h>

#include <cppasta/primitive_typedefs.hpp>
#include <cppasta/slot_map.hpp>

template <typename T, typename Key>
using GameObjectStorage = pasta::PagedSlotMapStorage<T, Key, std::vector<uint32_t>, std::allocator>;

template <typename T, typename Key = pasta::CompositeId<T>>
using GameObjectSlotMap = pasta::SlotMap<T, GameObjectStorage, Key>;

using ComponentId = u32;
constexpr usize MaxComponents = 12;

namespace detail {
ComponentId& get_component_id_counter();
}

struct GameObjectTag { };
using GameObjectId = pasta::CompositeId<GameObjectTag>;

template <typename T>
ComponentId component_id()
{
    static auto id = detail::get_component_id_counter()++;
    assert(id < MaxComponents);
    return id;
}

// Real version could use a variant and dispatch on it
struct CollisionEvent {
    GameObjectId a;
    GameObjectId b;
};

struct GameObject;

struct Component {
    GameObject* parent = nullptr;

    virtual ~Component() = default;

    virtual void update(float /*dt*/) { }
    virtual void on(const CollisionEvent&) { }

    template <typename T>
    T& get_component();

    template <typename T>
    T* try_get_component();
};

class GameObject {
public:
    GameObjectId id;

    GameObject() { }

    GameObject(GameObject&&) = default;
    GameObject& operator=(GameObject&&) = default;

    template <typename T, typename... Args>
    T& add_component(Args&&... args)
    {
        const auto id = component_id<T>();
        assert(id < components_.size());
        assert(!components_[id]);
        components_[id] = std::make_unique<T>(std::forward<Args>(args)...);
        components_[id]->parent = this;
        return *static_cast<T*>(components_[id].get());
    }

    template <typename T>
    T* try_get_component()
    {
        const auto id = component_id<T>();
        assert(id < components_.size());
        return static_cast<T*>(components_[id].get());
    }

    template <typename T>
    T& get_component()
    {
        const auto comp = try_get_component<T>();
        assert(comp);
        return *comp;
    }

    template <typename T>
    void remove_component()
    {
        const auto id = component_id<T>();
        assert(id < components_.size());
        components_[id].reset();
    }

    template <typename Event>
    void send(const Event& event)
    {
        for (auto& comp : components_) {
            if (comp) {
                comp->on(event);
            }
        }
    }

    bool marked_for_destruction() const { return marked_for_destruction_; }
    void destroy() { marked_for_destruction_ = true; }

private:
    std::array<std::unique_ptr<Component>, MaxComponents> components_ {};
    bool marked_for_destruction_ = false;
};

GameObjectSlotMap<GameObject, GameObjectId>& game_objects();
GameObject* create_game_object();
GameObject* get_game_object(GameObjectId id);
void destroy_marked_for_destruction();

template <typename Func>
void for_each_game_object(Func func)
{
    auto& objs = game_objects();
    auto id = objs.next({});
    while (id) {
        func(id, *objs.get(id));
        id = objs.next(id);
    }
    destroy_marked_for_destruction();
}

template <typename C>
static void update(float dt)
{
    for_each_game_object([dt](GameObjectId, GameObject& obj) {
        if (auto comp = obj.try_get_component<C>(); comp) {
            comp->update(dt);
        }
    });
}

template <typename T>
T& Component::get_component()
{
    return parent->get_component<T>();
}

template <typename T>
T* Component::try_get_component()
{
    return parent->try_get_component<T>();
}