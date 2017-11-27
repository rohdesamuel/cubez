# Cubez - A Fast C API Game Engine

**Cubez** is an Entity Component System based Game Engine. ECS is a game engine design pattern that favors composition over inheritance. This principle allows the game designer a greater control and flexibility in game design.

From [Wikipedia](https://en.wikipedia.org/wiki/Entity%E2%80%93component%E2%80%93system) on the subject:
> Entity–component–system (ECS) is an architectural pattern that is mostly used in game development. An ECS follows the Composition
> over inheritance principle that allows greater flexibility in defining entities where every object in a game's scene is an entity
> (e.g. enemies, bullets, vehicles, etc.). Every Entity consists of one or more components which add additional behavior or
> functionality. Therefore, the behavior of an entity can be changed at runtime by adding or removing components. This eliminates
> the ambiguity problems of deep and wide inheritance hierarchies that are difficult to understand, maintain and extend. Common ECS
> approaches are highly compatible and often combined with data oriented design techniques.

#### Dependencies
Compiler with c++11 capabilities

```
sudo apt-get install libglm-dev
sudo apt-get install libglew-dev
sudo apt-get install libsdl2-2.0
```
