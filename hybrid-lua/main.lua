local collider_entity_map = setmetatable({}, {__mode="v"})

local view_bounds_size = {x = 28, z = 17}

local shader = engine.load_shader("assets/vert.glsl", "assets/frag.glsl")
local ship_mesh = engine.load_mesh("assets/Spaceship_FernandoTheFlamingo.obj")
local asteroid_meshes = {
    engine.load_mesh("assets/Rock_1.obj", true),
    engine.load_mesh("assets/Rock_2.obj", true),
    engine.load_mesh("assets/Rock_3.obj", true),
}
local atlas_texture = engine.load_texture("assets/Atlas.png")
local bullet_mesh = engine.load_mesh("assets/laser.obj")
local bullet_texture = engine.load_texture("assets/laser.png")

local ship = nil
local asteroids = {}
local bullets = {}

local function length(x, y, z, w)
    z = z or 0
    w = w or 0
    return math.sqrt(x*x + y*y + z*z + w*w)
end

local function normalize(x, y, z, w)
    local len = length(x, y, z, w)
    if not z then
        return x/len, y/len
    elseif not w then
        return x/len, y/len, z/len
    else
        return x/len, y/len, z/len, w/len
    end
end

local next_entity_id = 0

local function create_entity(type_name, mesh, texture, radius)
    local entity = {
        id = next_entity_id,
        type = type_name,
        transform = engine.transform_create(),
        velocity = {x = 0, z = 0},
        collider = engine.collider_create(radius),
        radius = radius,
        mesh = mesh,
        texture = texture,
        update = function(dt) end,
        on_collision = function(other, nx, ny, nz, depth) end,
        mark_destroyed = function(self) self.destroyed = true end,
        destroy = function(self)
            collider_entity_map[self.collider] = nil
            engine.transform_destroy(self.transform)
            engine.collider_destroy(self.collider)
        end
    }
    next_entity_id = next_entity_id + 1
    collider_entity_map[entity.collider] = entity
    return entity
end

local function create_bullet(ship_trafo)
    local bullet = create_entity("bullet", bullet_mesh, bullet_texture, 1.0)
    engine.transform_set_position(bullet.transform, engine.transform_get_position(ship_trafo))
    engine.transform_set_orientation(bullet.transform, engine.transform_get_orientation(ship_trafo))
    -- This took me hours. For some reason the normals are messed up and you need to scale by -1 for it to look right - fine.
    -- But in all the other versions that -1 gets in there from somewhere (I don't know where)
    engine.transform_set_scale(bullet.transform, 1.0)
    local fx, fy, fz = engine.transform_get_forward(bullet.transform)
    engine.transform_move(bullet.transform, -fx * 0.5, 0, -fz * 0.5) -- move slightly in front of ship
    bullet.lifetime = 1.0
    local speed = 20
    bullet.velocity = {x = -fx * speed, z = -fz * speed}

    function bullet:update(dt)
        self.lifetime = self.lifetime - dt
        if self.lifetime <= 0.0 then
            self:mark_destroyed()
        end
    end

    function bullet:on_collision(other, nx, ny, nz, depth)
        if other.type == "asteroid" then
            self:mark_destroyed()
        end
    end

    return bullet
end

local function create_asteroid(x, z, vx, vz, size)
    local mesh = asteroid_meshes[engine.randi(1, #asteroid_meshes)]
    local radius = size * 0.5 * 0.85 -- fudge factor for collider
    local asteroid = create_entity("asteroid", mesh, atlas_texture, radius) 
    asteroid.velocity = {x = vx, z = vz}
    asteroid.radius = radius
    engine.transform_set_position(asteroid.transform, x, 0, z)
    engine.transform_set_scale(asteroid.transform, size)
    local qx, qy, qz, qw = normalize(engine.randf(-1, 1), engine.randf(-1, 1), engine.randf(-1, 1), engine.randf(-1, 1))
    engine.transform_rotate(asteroid.transform, qx, qy, qz, qw)

    function asteroid:on_collision(other, nx, ny, nz, depth)
        if other.type == "asteroid" then
            -- resolve collision
            -- only move by half the depth, because the other collider will do so as well
            engine.transform_move(self.transform, nx * depth * 0.5, 0, nz * depth * 0.5)

            -- bounce
            local mass = self.radius * self.radius * self.radius -- assume masses are proportional to volume
            local other_mass = other.radius * other.radius * other.radius
            local dvx, dvz = self.velocity.x - other.velocity.x, self.velocity.z - other.velocity.z
            local c = -2 * (dvx * nx + dvz * nz) / (1 + mass / other_mass)
            self.velocity.x = self.velocity.x + c * nx
            self.velocity.z = self.velocity.z + c * nz
        elseif other.type == "bullet" then
            if self.radius > 0.5 then
                local speed = length(self.velocity.x, self.velocity.z)
                local x, y, z = engine.transform_get_position(self.transform)
                local ox, oz = normalize(-other.velocity.z, other.velocity.x)
                -- 1/(2^(1/3)) times the origional radius should yield half the volume.
                local part_radius = self.radius * 0.8
                for i = 0, 1 do
                    local dir = i * 2 - 1
                    local px, pz = x + ox * dir * part_radius, z + oz * dir * part_radius
                    local vx, vz = self.velocity.x + dir * ox * speed, self.velocity.z + dir * oz * speed
                    asteroids[#asteroids + 1] = create_asteroid(px, pz, vx, vz, part_radius * 2)
                end
            end
            self:mark_destroyed()
            other:mark_destroyed()
        end
    end
    
    return asteroid
end

local function spawn_asteroid()
    local x, z
    if engine.randb() then
        x = engine.randf(-0.5, 0.5) * view_bounds_size.x
        z = (engine.randi(0, 1) * 2 - 1) * view_bounds_size.z * 0.4
    else
        x = (engine.randi(0, 1) * 2 - 1) * view_bounds_size.x * 0.4
        z = engine.randf(-0.5, 0.5) * view_bounds_size.z
    end

    local angle = engine.randf(0, 2 * math.pi)
    local speed = engine.randf(1, 3)
    local vx, vz = math.cos(angle) * speed, math.sin(angle) * speed

    local size = engine.randf(1, 5)

    return create_asteroid(x, z, vx, vz, size)
end

local function create_ship()
    local ship = create_entity("ship", ship_mesh, atlas_texture, 1.0)
    engine.transform_set_scale(ship.transform, 0.1)
    ship.last_shoot = false

    function ship:update(dt)
        local accel = engine.get_scancode_down(26) -- W
        if accel then
            local fx, fy, fz = engine.transform_get_forward(ship.transform)
            ship.velocity.x = ship.velocity.x - fx * dt * 2.0
            ship.velocity.z = ship.velocity.z - fz * dt * 2.0
        end

        local left = engine.get_scancode_down(4) and 1 or 0 -- A
        local right = engine.get_scancode_down(7) and 1 or 0 -- D
        local turn = left - right
        local turn_angle = turn * math.pi * 2.0 * dt * 0.5
        engine.transform_rotate(ship.transform, 0, math.sin(turn_angle * 0.5), 0, math.cos(turn_angle * 0.5))

        local shoot = engine.get_scancode_down(44)  -- space
        local shoot_pressed = shoot and not self.last_shoot
        self.last_shoot = shoot
        
        if shoot_pressed then
            bullets[#bullets + 1] = create_bullet(self.transform)
        end
    end

    return ship
end

local function destroy_marked_entities(entities)
    local num_entities = #entities

    local insert_idx = 1
    for i = 1, num_entities do
        if not entities[i].destroyed then
            if i ~= insert_idx then
                entities[insert_idx] = entities[i]
            end
            insert_idx = insert_idx + 1
        else
            entities[i]:destroy()
        end
    end

    for i = insert_idx, num_entities do
        entities[i] = nil
    end
end

local function update_entities(entities, dt)
    local n = #entities
    for i = 1, n do
        entities[i]:update(dt)
    end
    destroy_marked_entities(entities)
end

local function sys_physics(entities, dt)
    local n = #entities
    for i = 1, n do
        local entity = entities[i]
        local x, y, z = engine.transform_get_position(entity.transform)
        x, z = x + entity.velocity.x * dt, z + entity.velocity.z * dt

        if x < -view_bounds_size.x * 0.5 then
            x = x + view_bounds_size.x
        end
        if x > view_bounds_size.x * 0.5 then
            x = x - view_bounds_size.x
        end
        if z < -view_bounds_size.z * 0.5 then
            z = z + view_bounds_size.z
        end
        if z > view_bounds_size.z * 0.5 then
            z = z - view_bounds_size.z
        end

        engine.transform_set_position(entity.transform, x, y, z)
        engine.collider_set_position(entity.collider, engine.transform_get_position(entity.transform))
    end
end

local function sys_collision(entities)
    engine.detect_collisions()
    local n = #entities
    for e = 1, n do
        if entities[e].destroyed then
            goto continue
        end
        local num_col = engine.collider_get_num_collisions(entities[e].collider)
        for c = 1, num_col do
            local other_collider, nx, ny, nz, depth = engine.collider_get_collision(entities[e].collider, c)
            local other_entity = collider_entity_map[other_collider]
            if other_entity and not other_entity.destroyed then
                entities[e]:on_collision(other_entity, nx, ny, nz, depth)
            end
        end
        ::continue::
    end
end

local function draw_entities(entities)
    local n = #entities
    for i = 1, n do
        engine.draw(shader, entities[i].mesh, entities[i].transform, {
            u_texture = entities[i].texture,
        })
    end
end

ship = create_ship()

for i = 1, 12 do
    asteroids[i] = spawn_asteroid()
end

function update(dt)
    ship:update(dt)
    update_entities(asteroids, dt)
    update_entities(bullets, dt)
    
    sys_physics({ship}, dt)
    sys_physics(asteroids, dt)
    sys_physics(bullets, dt)
    
    sys_collision(asteroids)
    sys_collision(bullets)
    
    destroy_marked_entities(asteroids)
    destroy_marked_entities(bullets)

    engine.begin_frame()
    draw_entities({ship})
    draw_entities(asteroids)
    draw_entities(bullets)
    engine.end_frame()
end