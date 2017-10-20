#include "player.h"

#include "input.h"
#include "physics.h"
#include "render.h"
#include "shader.h"
#include "ball.h"

#include <glm/gtc/matrix_transform.hpp>
#include <GL/glew.h>

namespace player {

// State
render::qbRenderable render_state;
qbComponent players;
qbEntity main_player;
qbSystem on_key_event;

struct Player {
  bool fire_bullets;
};

void initialize(const Settings& settings) {
  {
    std::cout << "Initializing player textures and shaders\n";
    render::Mesh mesh;
    mesh.count = 6;
    GLfloat vertices[] = {
      0.5f,  0.5f, 0.0f,   1.0f, 1.0f,
      0.5f, -0.5f, 0.0f,   1.0f, 0.0f,
      -0.5f, -0.5f, 0.0f,   0.0f, 0.0f,

      -0.5f,  0.5f, 0.0f,   0.0f, 1.0f,
      0.5f,  0.5f, 0.0f,   1.0f, 1.0f,
      -0.5f, -0.5f, 0.0f,   0.0f, 0.0f,
    };

    glGenVertexArrays(1, &mesh.vao);
    glBindVertexArray(mesh.vao);

    
    glGenBuffers(1, &mesh.vbo);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(vertices), vertices, GL_STATIC_DRAW);

    ShaderProgram shaders(settings.vs, settings.fs);
    shaders.use();

    GLint inPos = glGetAttribLocation(shaders.id(), "inPos");
    glEnableVertexAttribArray(inPos);
    glVertexAttribPointer(inPos, 3, GL_FLOAT, GL_FALSE,
        5 * sizeof(float), 0);

    GLint inTexCoord = glGetAttribLocation(shaders.id(), "inTexCoord");
    glEnableVertexAttribArray(inTexCoord);
    glVertexAttribPointer(inTexCoord, 3, GL_FLOAT, GL_FALSE,
        5 * sizeof(float), (void*)(3 * sizeof(float)));

    render::Material material;
    material.shader_id = shaders.id();
    material.texture_id = render::load_texture(settings.texture);

    render_state = render::create(material, mesh);
  }
  {
    qbComponentAttr attr;
    qb_componentattr_create(&attr);
    qb_componentattr_setdatatype(attr, Player);
    
    qb_component_create(&players, attr);
    qb_componentattr_destroy(&attr);
  }
  {
    qbEntityAttr attr;
    qb_entityattr_create(&attr);

    Player p;
    p.fire_bullets = false;

    physics::Transform t;
    t.p = settings.start_pos;
    t.v = {0.01, 0, 0};

    qb_entityattr_addcomponent(attr, players, &p);
    qb_entityattr_addcomponent(attr, physics::component(), &t);
    qb_entityattr_addcomponent(attr, render::component(), &render_state);

    qb_entity_create(&main_player, attr);

    qb_entityattr_destroy(&attr);
  }
  {
    qbSystemAttr attr;
    qb_systemattr_create(&attr);
    qb_systemattr_addsource(attr, players);
    qb_systemattr_addsource(attr, physics::component());
    qb_systemattr_addsink(attr, physics::component());
    qb_systemattr_setjoin(attr, qbComponentJoin::QB_JOIN_LEFT);
    qb_systemattr_setfunction(attr,
        [](qbElement* e, qbCollectionInterface*, qbFrame*) {
          physics::Transform t;
          qb_element_read(e[1], &t);
          if (input::is_key_pressed(qbKey::QB_KEY_SPACE)) {
            ball::create(t.p, {0.01, 0.0, 0.0});
          }
          if (input::is_key_pressed(QB_KEY_W)) {
            t.v.y += 0.001;
          }
          if (input::is_key_pressed(QB_KEY_S)) {
            t.v.y -= 0.001;  
          }
          if (input::is_key_pressed(QB_KEY_A)) {
            t.v.x -= 0.001;  
          }
          if (input::is_key_pressed(QB_KEY_D)) {
            t.v.x += 0.001;  
          }

          qb_element_write(e[1]);
        });

    qbSystem unused;
    qb_system_create(&unused, attr);
    qb_systemattr_destroy(&attr);
  }
  std::cout << "Finished initializing player\n";
}

}  // namespace player

