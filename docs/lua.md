# Lua Methods

## General

##### qb.start(screen_width, screen_height)
```
screen_width: integer
screen_height: integer
```

Called once when Cubez starts.

##### qb.init()
Called every time a new qbProgram is created. Every qbProgram is run on a separate thread. This
allows for running simultaneous Lua scripts.

##### qb.update()
Called every frame.

## Entities

##### qb.entity_create(components)

```lua
qb.entity_create({ Position:new{ x=0, y=0 } })
```

##### qb.entity_destroy(entity)
Creates

```lua
e = qb.entity_create(...)
qb.entity_destroy(e)
```

## Components

##### qb.component_create(name, schema)
Supported types in schema: 'boolean', 'number', 'string', 'table', 'function'.

```lua
Position = qb.component_create('Position', {
    { x = 'number' },
    { y = 'number' },
})
```

##### component_find(name)

```lua
Position = qb.component_find('Position')
```

## Systems

##### qb.system_create(components, fn)

```lua
qb.system_create({ Position, Velocity },
    function (pos, vel)
      pos.x = pos.x + vel.x
      pos.y = pos.y + vel.y
    end)
```