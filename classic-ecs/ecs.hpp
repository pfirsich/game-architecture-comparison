#pragma once

#include <array>
#include <cassert>
#include <cstring>
#include <queue>

#include <cppasta/generational_index.hpp>
#include <cppasta/primitive_typedefs.hpp>

namespace ecs {

/*
This is my implementation of a "classic" canonical ECS, where an entity is just an integer
(generational index here), components are only data and systems do the work.

I am actually pretty proud of this. It has many obvious limitations that might be a real problem in
a real game (max 64 components, hard coded maximum number of entities, very wasteful of memory), but
it is the easiest and simplest ECS I could come up with and I think it's super simple.

The only thing that I might still consider doing is getting rid of the component_id stuff and just
requiring you define your own id. Then I could get rid of two functions (it's the ones that are the
least obvious as well), but I am afraid it might be a bit error-prone.
*/

struct EntityTag { };
using Entity = pasta::CompositeId<EntityTag>;

using ComponentMask = u64;

constexpr usize MaxComponents = 64;
constexpr usize MaxEntities = 1024;

namespace detail {
    static usize& get_component_id_counter()
    {
        static usize component_id_counter = 0;
        return component_id_counter;
    }

    template <typename T>
    usize component_id()
    {
        static auto id = get_component_id_counter()++;
        assert(id < MaxComponents);
        return id;
    }

    template <typename... Ts>
    ComponentMask component_mask()
    {
        return ((1 << component_id<Ts>()) | ...);
    }

    // These should be owned by World, but we only have one instance of ECS and we want this to be
    // simple
    template <typename T>
    T& component(usize idx)
    {
        static std::array<T, MaxEntities> pool;
        assert(idx < pool.size());
        return pool[idx];
    }
}

class World {
public:
    static World& instance()
    {
        static World world;
        return world;
    }

    World()
    {
        for (usize i = 0; i < entities_.size(); ++i) {
            entities_[i].id = Entity(i, 1);
        }
    }

    Entity create_entity()
    {
        const auto idx = get_free_entity_idx();
        assert(idx < entities_.size());
        return entities_[idx].id;
    }

    bool exists(Entity entity) const
    {
        assert(entity.idx() < entities_.size());
        return entities_[entity.idx()].id.gen() == entity.gen();
    }

    void destroy_entity(Entity entity)
    {
        assert(exists(entity));
        entities_[entity.idx()].id = entities_[entity.idx()].id.next_generation();
        entities_[entity.idx()].cmask = 0;
        free_list_.push(entity.idx());
    }

    template <typename T, typename... Args>
    T& add_component(Entity entity, Args&&... args)
    {
        static_assert(std::is_standard_layout_v<T>);
        assert(!has_component<T>(entity));
        entities_[entity.idx()].cmask |= detail::component_mask<T>();
        auto& comp = detail::component<T>(entity.idx());
        comp = T { std::forward<Args>(args)... };
        return comp;
    }

    template <typename T>
    T& get_component(Entity entity) const
    {
        assert(has_component<T>(entity));
        return detail::component<T>(entity.idx());
    }

    template <typename T>
    T* try_get_component(Entity entity) const
    {
        return has_component<T>(entity) ? &detail::component<T>(entity.idx()) : nullptr;
    }

    template <typename T>
    void remove_component(Entity entity)
    {
        assert(has_component<T>(entity));
        entities_[entity.idx()].cmask &= ~detail::component_mask<T>();
    }

    template <typename T>
    bool has_component(Entity entity) const
    {
        assert(entity.idx() < entities_.size());
        return exists(entity) && (entities_[entity.idx()].cmask & detail::component_mask<T>()) > 0;
    }

    template <typename... Components, typename Func>
    void for_each_entity(Func func) const
    {
        const auto mask = detail::component_mask<Components...>();
        for (size_t i = 0; i < next_entity_idx_; ++i) {
            if ((entities_[i].cmask & mask) == mask) {
                func(entities_[i].id);
            }
        }
    }

    template <typename... Components, typename Func>
    void for_each_entity_pair(Func func) const
    {
        const auto last = next_entity_idx_; // func might create more entities
        const auto mask = detail::component_mask<Components...>();
        for (size_t i = 0; i < last; ++i) {
            if ((entities_[i].cmask & mask) == mask) {
                for (size_t j = i + 1; j < last; ++j) {
                    if ((entities_[j].cmask & mask) == mask) {
                        func(entities_[i].id, entities_[j].id);
                    }
                }
            }
        }
    }

private:
    struct EntityData {
        Entity id;
        ComponentMask cmask = 0;
    };

    usize get_free_entity_idx()
    {
        if (free_list_.empty()) {
            return next_entity_idx_++;
        }
        const auto idx = free_list_.top();
        free_list_.pop();
        return idx;
    }

    // With components being PODs, we don't have to worry about destroying them
    std::array<EntityData, MaxEntities> entities_;
    // I don't know if this is actually good, but it keeps the components close to each other and
    // it's super easy to do
    std::priority_queue<usize> free_list_;
    usize next_entity_idx_ = 0;
};

Entity create()
{
    return World::instance().create_entity();
}

bool exists(Entity entity)
{
    return World::instance().exists(entity);
}

void destroy(Entity entity)
{
    return World::instance().destroy_entity(entity);
}

template <typename T, typename... Args>
T& add(Entity entity, Args&&... args)
{
    return World::instance().add_component<T>(entity, std::forward<Args>(args)...);
}

template <typename T>
T& get(Entity entity)
{
    return World::instance().get_component<T>(entity);
}

template <typename T>
T* try_get(Entity entity)
{
    return World::instance().try_get_component<T>(entity);
}

template <typename T>
void remove(Entity entity)
{
    return World::instance().remove_component<T>(entity);
}

template <typename T>
bool has(Entity entity)
{
    return World::instance().has_component<T>(entity);
}

template <typename... Components, typename Func>
void for_each(Func func)
{
    return World::instance().for_each_entity<Components...>(std::move(func));
}

template <typename... Components, typename Func>
void for_each_pair(Func func)
{
    return World::instance().for_each_entity_pair<Components...>(std::move(func));
}

}