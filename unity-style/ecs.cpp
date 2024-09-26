#include "ecs.hpp"

#include <vector>

namespace detail {
ComponentId& get_component_id_counter()
{
    static ComponentId component_id_counter = 0;
    return component_id_counter;
}
}

GameObjectSlotMap<GameObject, GameObjectId>& game_objects()
{
    static GameObjectSlotMap<GameObject, GameObjectId> objs(2048);
    return objs;
}

GameObject* create_game_object()
{
    auto id = game_objects().insert({});
    auto obj = game_objects().get(id);
    obj->id = id;
    return obj;
}

GameObject* get_game_object(GameObjectId id)
{
    return game_objects().get(id);
}

void destroy_marked_for_destruction()
{
    auto& objs = game_objects();
    std::vector<GameObjectId> ids;
    auto id = objs.next({});
    while (id) {
        if (objs.get(id)->marked_for_destruction()) {
            ids.push_back(id);
        }
        id = objs.next(id);
    }
    for (const auto id : ids) {
        objs.remove(id);
    }
}