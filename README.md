# Cubez - C API Game Engine

**Cubez** is an Entity Component System Game Engine.

## Features

* Rendering
  * Camera
  * Lighting
  * Wavefront obj loading (no material importing yet)
  * Customizable rendering backend (OpenGL, needs Vulkan implementation)
  * Customizable rendering frontend (Forward Rendering, implemented client-side)
  * Default Forward PBR with materials (Albedo, Normal, Metallic, Roughness, AO, Emission)
* GUI
* Audio
  * WAV audio loading
  * Play/stop, Pause, loop, pan, volume
* Game Loop
  * Classic ECS implementation for updates
  * Semaphores for system ordering
  * Reader/Writer lock for component mutations
  * Events (Publisher/ Subscriber model)
  * Scenes
  * Coroutines
* User Input

## Dependencies

Comes with pre-compiled binaries with the following versions:

* glew --> 2.1.0
* sdl2 --> 2.0.9
* freetype --> 2.10.0 (not pre-compiled for Linux)

### Linux

g++ version >= 7.0

### Windows

msvc version >= 19.0

## Design

### Entities and Components

The engine design is a classic ECS design. This means that entities are ids into component arrays. Components are implemented in the backend as sparse arrays.

**Why use entities and components?** Allows for a modular design pattern favoring composition over inheritance. Also allows for more cache coherency, i.e. better performance for smaller components.

### Programs

A program is a list of synchronous systems to execute. Events are not thread-safe and are local to programs. Components can be used for cross-program communication. Events are fast and non-thread safe. Adding/removing Components is slower but are thread-safe. For cross-platform ordering, semaphores on systems with the `addbarrier` method. A barrier is reset every loop. A single system is assigned to access the barrier. Once that system runs, the barrier is taken down and all other systems waiting on the same barrier are allowed to run. When the game loop is run, each program is started at the same time and each thread is joined to wait for the next loop update. Programs can be detached so that this behavior is not the case with `qb_detach_program`.

**Why use multiple programs?** Allows for a job queue design pattern. The main thread can generate work while multiple programs consume the work. This allows for high CPU utilization allowing for more complex scenes.

### Systems

System are run sequentially and in order of priority. They are used to update components and respond to events. Systems can be ordered across programs using barriers. To update components, a system is used to select components using either the `addmutable` or `addconst` methods. Adding a mutable component adds a writer lock in the engine for accessing if the component is marked as shared. Adding a const component adds a reader lock if the component is marked as shared. To select over multiple components a system can use the `setjoin` method. This can perform inner, left, or cross joins. For inner and left joins, it uses the associated entity ids in the component arrays as keys to join on.

**Why use systems?** Systems are for high performance and updating many entities at once. This is because systems take advantage of cache coherency because of the linear layout of the components. 

### Coroutines

Coroutines are cooperatively scheduled continuations. When inside a coroutine, one can: yield to the cooperative scheduler, wait for X frames or Y seconds while yielding to the scheduler, yielding control directly to another coroutine. There are two ways to start coroutines, either with `qb_coro_sync` or `qb_coro_async`. These pick two schedulers: the first is runs through each coroutine sequentially in each game loop update; the second schedules the coroutine on a separate thread independent of the main game loop.

**Why use coroutines?** Coroutines are more ergonomic than systems and simple to use. However, coroutines are slow because yielding causes all registers to flush while switching to a new stack. Coroutines are not as good as systems for updating many entities at once. They are useful for smaller, periodic events.

## Examples

### Game Loop

#### cubez.h

Initializing the Game Engine
```c++
qbUniverseAttr_ uni_attr = {};
uni_attr.title = "Cubez example";
uni_attr.width = 1200;
uni_attr.height = 800;

// The engine is modularized and can have different portions enabled or disabled.
// QB_FEATURE_ALL enables: 
//   game loop (QB_FEATURE_GAME_LOOP): Default is a single-threaded update, rendering loop. Can be implemented client-side with
//                                     primitives defined in cubez.h.
//   rendering (QB_FEATURE_GRAPHICS): Enables rendering with OpenGL. Can be disabled or the rendering backend can be reimplemented using
//                                    a different rendering SDK.
//   audio (QB_FEATURE_AUDIO): Enables audio with SDL2.
//   user input (QB_FEATURE_INPUT): Enables input with SDL2.
//   logging (QB_FEATURE_LOGGER): Enables a separate thread for logging.
uni_attr.enabled = QB_FEATURE_ALL;

// This is only necessary if QB_FEATURE_GRAPHICS is enabeld.
qbRendererAttr_ renderer_attr = {};
uni_attr.create_renderer = qb_forwardrenderer_create;
uni_attr.destroy_renderer = qb_forwardrenderer_destroy;
uni_attr.renderer_args = &renderer_attr;

// This is only necessary if QB_FEATURE_AUDIO is enabeld.
qbAudioAttr_ audio_attr = {};
audio_attr.sample_frequency = 44100;
audio_attr.buffered_samples = 15;
uni_attr.audio_args = &audio_attr;

qb_init(uni, &uni_attr);
```

Creating a moving square with high-performance ECS backend.
```c++
struct PhysicsComponent {
  int x, y;
  int vx, vy;
};

qbComponent physics_component;
{
  qbComponentAttr attr;
  qb_componentattr_create(&attr);
  qb_componentattr_setdatatype(attr, PhysicsComponent);
  qb_component_create(&physics_component, attr);
  qb_componentattr_destroy(&attr);
}

qbSystem physics_update;
{
  qbSystemAttr attr;
  qb_systemattr_create(&attr);
  qb_system_addmutable(attr, physics_component);
  qb_system_setfunction(attr, [](qbInstance* selected, qbFrame* unused_frame) {
    PhysicsComponent* ball;
    qb_instance_getmutable(selected[0], &ball);
    
    ball->x += ball->vx;
    ball->y += ball->vy;
  });
  qb_system_create(&physics_update, attr);
  qb_systemattr_destroy(&attr);
}

qbEntity ball;
{
  qbEntityAttr attr;
  qb_entityattr_create(&attr);
  qb_entityattr_addcomponent(attr, physics_component);
  qb_entity_create(&ball, attr);
  qb_entityattr_destroy(&attr);
}
```

Creating a moving square with coroutines.
```c++
struct PhysicsComponent {
  int x, y;
  int vx, vy;
};

qbComponent physics_component;
{
  qbComponentAttr attr;
  qb_componentattr_create(&attr);
  qb_componentattr_setdatatype(attr, PhysicsComponent);
  qb_component_create(&physics_component, attr);
  qb_componentattr_destroy(&attr);
}

qbEntity ball;
{
  qbEntityAttr attr;
  qb_entityattr_create(&attr);
  qb_entityattr_addcomponent(attr, physics_component);
  qb_entity_create(&ball, attr);
  qb_entityattr_destroy(&attr);
}

qb_coro_sync([](qbVar v) {
  qbEntity ball_id = v.i;
  PhysicsComponent* ball;
  qb_instance_find(physics_component, ball_id, &ball);
  ball->x += ball->vx;
  ball->y += ball->vy;
  
  // Wait for 15ms.
  qb_coro_wait(0.015);
});
```

Creating and subscribing to an event
```c++

struct Collision {
  qbEntity a, b;
};

qbEvent collision_events;
{
  qbEventAttr attr;
  qb_eventattr_create(&attr);
  qb_eventattr_setmessagetype(attr, Collision);
  qb_event_create(&collision_events, attr);
}

qbSystem collision_handler;
{
  qbSystemAttr attr;
  qb_systemattr_create(&attr);
  qb_systemattr_settrigger(attr, QB_TRIGGER_EVENT);
  qb_system_setcallback(attr, [](qbFrame* frame) {
    PhysicsComponent* a;
    PhysicsComponent* b;
    
    Collision* collision = (Collision*)frame->event;
    qb_instance_find(physics_component, frame->a, &a);
    qb_instance_find(physics_component, frame->b, &b);
    
    // Do collision
    ...
    
  });
  qb_system_create(&collision_handler, attr);
  qb_systemattr_destroy(&attr);
}

qb_event_subscribe(collision_events, collision_handler);

// Publish a collision
Collision collision { some_entity_a, some_entity_b };

// Naively copies the collision event.
qb_event_send(collision_events, &collision);

```

Creating and destroying a scene.
```c++
qbScene world_1, world_2;
qb_scene_create(&world_1, "world_1");
qb_scene_create(&world_2, "world_2");

qb_scene_set(world_1);

qbEntity ball_1;
...
qb_entity_create(&ball_1, attr);

qb_scene_set(world_2);

qbEntity ball_2;
...
qb_entity_create(&ball_1, attr);

// Only ball_1 exists in the scene. This function does the following:
// 1. Deactivates current active scene and calls the ondeactivate event (subscribed with qb_scene_ondeactivate)
// 2. Activates the given scene (subscribed with qb_scene_onactivate)
// 3. Sets the "working scene" to the given scene
// 4. Calls the onactivate event with the given scene
qb_scene_activate(scene_1);

// Only ball_2 exists in the scene, ball_1 was not destroyed.
qb_scene_activate(scene_2);
```

### Rendering

#### render.h
Defines window interaction, camera interation, lighting, creating renderables.

```c++
// Camera
qbCamera camera;
qbCameraAttr_ attr = {};
attr.fov = glm_rad(45.0f);
attr.height = 800;
attr.width = 1200;
attr.znear = 0.1f;
attr.zfar = 1000.0f;
attr.origin = vec3s{ 0.0f, 0.0f, 0.0f };
attr.rotation_mat = GLMS_MAT4_IDENTITY_INIT;
qb_camera_create(&camera, &attr);
qb_camera_activate(camera);

// Lighting
// Point light
qb_light_point(0, { 1.0f, 1.0f, 1.0f }, { 0.0f, 0.0f, 5.0f }, 15.0f, 200.0f);
qb_light_enable(0, qbLightType::QB_LIGHT_TYPE_POINT);

// Direction light
qb_light_directional(0, { 0.9f, 0.9f, 1.0f }, { 0.0f, 0.0f, -1.0f }, 0.05f);
qb_light_enable(0, qbLightType::QB_LIGHT_TYPE_DIRECTIONAL);
```

#### mesh.h

```c++
// Model loading
qbRenderable r = qb_model_load("SpaceShip", "resources/space_ship.obj");

// Material definition
qbMaterial material;
qbMaterialAttr_ attr = {};
{
  qbImageAttr_ image_attr = {};
  image_attr.type = qbImageType::QB_IMAGE_TYPE_2D;
  image_attr.generate_mipmaps = true;
  qb_image_load(&attr.albedo_map, &image_attr,
                "resources/space_ship_albedo.bmp");
  qb_image_load(&attr.normal_map, &image_attr,
                "resources/rustediron1_normal.png");
  qb_image_load(&attr.metallic_map, &image_attr,
                "resources/rustediron1_metallic.png");
  qb_image_load(&attr.roughness_map, &image_attr,
                "resources/rustediron1_roughness.png");
}
qb_material_create(&material, &attr, "ball");
```

#### render_pipeline.h

See `forward_renderer.h` in the example project.

#### gui.h

```c++
qbWindow main_menu;
qbWindowAttr_ attr = {};
attr.background_color = { 0.0f, 0.0f, 0.0f, 0.95f };

vec2s pos = { (1200.0f * 0.5f) - 256.0f, 100.0f };
vec2s size = { 512.0f, 600.0f };
qb_window_create(&main_menu, &attr, pos, size, nullptr, true);
```

### Audio

#### audio.h

```c++
qbAudioBuffer jump_buffer = qb_audio_loadwav("resources/jump.wav");
qbAudioPlaying jump = qb_audio_upload(jump_wav);
qb_audio_play(jump);
```

### User Input

#### input.h
```c++

qb_coro_sync([](qbVar v) {
  qbEntity ball_id = v.i;
  PhysicsComponent* ball;
  qb_instance_find(physics_component, ball_id, &ball);
  
  if (qb_is_key_pressed(QB_KEY_LEFT)) {
    ball->vx -= 1.0f;
  }
  
  if (qb_is_key_pressed(QB_KEY_RIGHT)) {
    ball->vx += 1.0f;
  }
  
  if (qb_is_key_pressed(QB_KEY_UP)) {
    ball->vy += 1.0f;
  }
  
  if (qb_is_key_pressed(QB_KEY_DOWN)) {
    ball->vy -= 1.0f;
  }
  
  ball->x += ball->vx;
  ball->y += ball->vy;
  
  // Wait for 15ms.
  qb_coro_wait(0.015);
});

```
