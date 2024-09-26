#include <cstdio>

#include "ecs.hpp"

struct Position {
    float x;
    float y;
};

struct Sprite {
    float w;
    float h;
    u64 texture;
};

int main()
{
    const auto ent1 = ecs::create();
    assert(ecs::exists(ent1));
    const auto ent2 = ecs::create();
    assert(ecs::exists(ent1));
    assert(ecs::exists(ent2));

    ecs::add<Position>(ent1, 1.0f, 2.0f);
    assert(ecs::has<Position>(ent1));
    ecs::add<Position>(ent2, 3.0f, 4.0f);
    assert(ecs::has<Position>(ent2));

    std::printf("positions #1\n");
    ecs::for_each<Position>([](ecs::Entity entity) {
        const auto& pos = ecs::get<Position>(entity);
        std::printf("Entity (%d, %d) Position: %f, %f\n", entity.idx(), entity.gen(), pos.x, pos.y);
    });

    ecs::remove<Position>(ent2);
    assert(!ecs::has<Position>(ent2));

    ecs::destroy(ent1);
    const auto ent3 = ecs::create();
    assert(ecs::exists(ent3));
    assert(!ecs::exists(ent1));

    std::printf("positions #2\n");
    ecs::for_each<Position>([](ecs::Entity entity) {
        const auto& pos = ecs::get<Position>(entity);
        std::printf("Entity (%d, %d) Position: %f, %f\n", entity.idx(), entity.gen(), pos.x, pos.y);
    });

    ecs::add<Position>(ent3, 5.0f, 6.0f);
    ecs::add<Sprite>(ent3, 100.0f, 100.0f, 1_u64);

    std::printf("positions, sprite\n");
    ecs::for_each<Position, Sprite>([](ecs::Entity entity) {
        const auto& pos = ecs::get<Position>(entity);
        const auto& sprite = ecs::get<Sprite>(entity);
        std::printf("Entity (%d, %d) Position: %f, %f, Sprite: %f, %f, %lu\n", entity.idx(),
            entity.gen(), pos.x, pos.y, sprite.w, sprite.h, sprite.texture);
    });
}