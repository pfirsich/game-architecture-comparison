# Game Architecture Comparison

I am contemplating what architecture to base my new engine on. This is a version of Asteroids implemented in a multitude of ways to compare different way to architecture a game.

The idea is to be able to try out a way to architecture a game in a day or two (not weeks+), with all the shared code being present and all the "gameplay" figured out (just copy&paste it).

The code is limited to gameplay code stuff, because that is the only thing that is relevant for my comparison.
That means no sounds, no particle effects and no UI. All that stuff would be largely independent of the architecture used. Of course the game itself is not fun at all and not very juicy either.

# Variations
* `classic-ecs/`: "pure" ECS with entities as integers, components as plain old data
* `unity-style/`: unity style where each entity has components, which have an update method
* `base-entity/`: one base entity class with a virtual update method, some data is reused in components
* `uber-entity/`: a single entity class for everything where for some entities parts are simply not used
* `no-polymorphism/`: separate entity class for every object, no unified container for them all
* `hybrid/`: world systems use ECS, entity systems use (see below for terminology)
* `hybrid-lua/`: Similar to hybrid, but entity systems are in Lua
* `free-components/` # everything is just handles to components (like krieger) 

# Notes
## Classic ECS
It's really cool how you create an entity add a few components and you have a new type of entity and it just works without writing any new "real" code.
Getting components constantly is a bit annoying. It feels you are doing more component wrangling than anything else (adding components, getting components, etc - boilerplate). This probably has to do with the fact that it's a very simple game.
There are just so many decisions you have to make when implementing an ECS. There is a lot of code just for the ECS (the ECS for this repo is as simple as you can possibly make it). I have done it a few times, otherwise this would have taken days or weeks and I would have had to dive into the deepest of rabbit holes out there. Some people forget they make games and just build ECSs forever.
Collision can be handled ad-hoc in this repo, but in a bigger game, you need a cental system that detects collisions, because you need acceleration structures and such. That means you likely need an event system and think that a "proper" ECS definitely needs an event system and when you start with that it starts to suck significantly more.
You launder entity type specific code into pretend-abstract components and systems and it makes the code harder to read, rather than easier. You tend to over-engineer and abstract too early, because ECS invites reuse. I think this is bad.
Also for many types of entities in smaller games you just want a single update function, put all the stuff that entity does in there and be done with it. I miss this here.

## Base Entity
The common base class requires that all entities are heap allocated. This makes me really sad.
Entities cannot properly destroy themselves, but pretty much every architecture has this problem, I will realize later.
Some things you just cannot fit into the update function of an entity, even though it would be nice.
You want to loop over all entities of a certain type *constantly* and it's just askward.
It's pretend polymorphism, because you need to cast to the specific entity types all the time. It's a game, there is no way you can make this actually polymorphic. You pretend you forget the specific entity types and then you need to get them back.
Game complexity is not `O(n)` with `n = number of entity types`, but rather `O(n*n)`, because entities INTERACT. This interaction has to be easily reflected in the code.
It's hard to know when to put stuff in the common base class, because even though the current set of entity types does share data, a future entity might not need it.
There really is no good way to store them (can't use homogenous container).

## Unity Style
Collision is also handled separately, because there is no other nice way.
This just feels like a more ergonomic ECS version, which is also much slower (which is not super interesting to me).
I was just about to get covid when I started this, so I don't know if it was me, but this was the hardest to get working. Classic ECS had almost no bugs and I just wrote it down (pretty much), base entity was a matter of an hour or two, but this took days and I had to step through with gdb angrily multiple times.
Compared to OOP of course sharing code and splitting it later is easier.

## Uber Entity
It's very cool and very simple. I thought it was stupid, but I tried it, because it's so easy to do. There are very few decisions to make, you need no sort of library (like ECS or Unity). If the entity doesn't get too large, it's probably quite fast too (definitely in this case). Obviously this doesn't scale well to large games and becomes very inefficient (esp. memory) very quickly, but it's better than I thought and might be something I use for small games, because it's so stupidly simple.

## No Polymorphism
This has the worst name, but it's kind of like "Base Entity", i.e. OOP style, but instead of using polymorphism, everything is just stored in separate collections. This is what I consider having *no architecture at all*. It just **does the thing**. I love this. This is how I built games for 15 years or so. Almost nothing of this code is boilerplate. This is likely the fastest version too. Tricky problems like things updating in the wrong order sometimes are easy peasy here and nowhere else. This is not great for scripting imho, but I want to stay as close as I can. This was very easy and I enjoyed this the most. This is totally not a big-boy/girl way to do it and doesn't seem like a serious way to architecture a game, but I think for small-ish games you SHOULD do this.
If you really think about it, this is ECS with archetypes and you delete all the ECS code.

## Hybrid
Before you read this, read the conclusion first to see why I did this.
Essentially here I tried to turn my conclusion into code and used OOP for the "entity systems" and the ECS from "Classic ECS" for the "world systems". It's somehow kind of nice and somehow very weird. I think it's mostly weird, because I haven't done this as nicely as I could. Unfortunately I forgot to write notes while building this, so I forgot what I thought about it. It definitely inspired me to try the same with Lua.

## Hybrid Lua
With scripting the hybrid approach makes a lot more sense. This uses the most amount of code BY FAR (almost factor 3). The Lua code alone is about as large as some other approaches (like uber entity), which shocks me. All the binding and boilerplate is extremely annoying and there were a bunch of tricky bugs, but just changing a bit of Lua code, running again and it immediately opens is just so great. I have questioned my want for scripting a few times recently, but this reminded me it's necessary. This is pretty much what I want to use for my engine.

# Conclusions
The stuff that I was missing in ECS is that I don't just have a `Player::update` or `Asteroid::update`, which would likely contain most of the interesting code. And in base entity I had the opposite problem of systems not fitting into update.
Most systems are used for 1 to 100 entities that have their own HUGE state (player, enemies, items). It's silly to have separate components and systems in ECS, which tries to be fast, that only apply to a handful of entities at most. I would argue most of the code you write in a game is code like that.

That does not mean that most of the code that RUNS is code like that. The code the runs the most is imho all the heavy stuff that actually applies to almost all entities, such as rendering, animation, physics. This is the stuff that needs to be fast and the stuff you can actually use DOD for properly. This is where ECS is good.

Using DOD you won't make code fast that uses large amounts of data per entity and applies only to a very small number of entities, but it will make stuff like rendering or physics fast, where everything is mostly uniform and there is lots of it.
I think the real insight is to split these and acknowledge them as two different types and have entirely different interfaces for them.

So my conclusion for this experiment is that a game has "world systems" that do profit from using DOD and apply to most entities or deal with entity interactions and "entity systems" (a bit confusing, I know) that is the stuff you want to put into `Player::update` that applies to only a few entities.

I think a good game architecture would recognize this and simply provide two different abstractions for them or at least handle them very differently. It's also a great boundary for scripting ("entity systems" in the scripting language, "global systems" in C++).

# Maybe Try This Later
* `pod-zii/`: only POD types with ZII (zero is initialization) - Muratori style
* `member-components/`: pretend it's ECS, but components are optional member variables with a `Transform* get_transform` getter
* `krieger/`: https://www.youtube.com/watch?v=jjEsB611kxs - similar conclusion as me, but too much boilerplate and still a common abstraction

# Asset Sources
* https://quaternius.com/packs/ultimatespacekit.html
* https://opengameart.org/content/assets-free-laser-bullets-pack-2020

# Building
## Linux
```
cmake -B build -G Ninja .
cmake --build build
```

## Cross-Compile for Win64 on Linux
Get a relatively current mingw or clang (recent C++ features are used, so it has be more recent than most mingw distributions).
I used the one from [mstorsjo/llvm-mingw](https://github.com/mstorsjo/llvm-mingw/).
If using the linked clang-mingw, let's assume you unpacked it to `~/mingw`.

```
cmake -B build_windows -G Ninja -DCMAKE_SYSTEM_NAME=Windows -DCMAKE_C_COMPILER=~/mingw/bin/x86_64-w64-mingw32-gcc -DCMAKE_CXX_COMPILER=~/mingw/bin/x86_64-w64-mingw32-g++ .
```

Then copy over some .dlls from `~/mingw/86_64-w64-mingw32/bin/` (`libc++.dll` and `libunwind.dll` in my case) and `SDL2.dll` from `build_windows/_deps/sdl2-build/` and you're done :)

TODO: Use windres for icons