#include <cubez/draw.h>

#include <cglm/struct.h>

#include <cubez/memory.h>
#include <cubez/render.h>
#include <vector>

size_t MAX_DRAW_COMMANDS = 128;

struct qbDrawCommands_ {
  qbDrawCommand_* commands;
  size_t commands_count;
  qbCamera_ camera;

  qbDrawCommand_ cur;
};

static qbMemoryAllocator allocator = nullptr;
static qbMemoryAllocator cmd_allocator = nullptr;

void qb_draw_init(qbDrawCommands cmds, const qbCamera_* camera);
qbDrawCommands qb_draw_begin(const struct qbCamera_* camera) {
  if (!allocator) {
    allocator = qb_memallocator_pool();
  }

  if (!cmd_allocator) {
    cmd_allocator = qb_memallocator_pool();
  }

  qbDrawCommands ret = (qbDrawCommands)qb_alloc(allocator, sizeof(qbDrawCommands_));
  new (ret) qbDrawCommands_{};

  ret->cur = {};
  ret->commands = (qbDrawCommand_*)qb_alloc(cmd_allocator, sizeof(qbDrawCommand_) * MAX_DRAW_COMMANDS);
  ret->camera = *camera;
  ret->cur.args.transform = GLMS_MAT4_IDENTITY_INIT;
  ret->cur.args.color = { 0.f, 0.f, 0.f, 1.f };

  qb_draw_init(ret, &ret->camera);

  return ret;
}

void qb_draw_init(qbDrawCommands cmds, const qbCamera_* camera) {
  cmds->commands[cmds->commands_count++] = {
    .type = QB_DRAW_CAMERA,
    .command = qbDrawCommandInit_ {
      .camera = camera
    }
  };
}

void qb_draw_settransform(qbDrawCommands cmds, mat4s transform) {
  cmds->cur.args.transform = transform;
}

void qb_draw_gettransform(qbDrawCommands cmds, mat4s* transform) {
  *transform = cmds->cur.args.transform;
}

void qb_draw_identity(qbDrawCommands cmds) {
  cmds->cur.args.transform = glms_mat4_identity();
}

void qb_draw_rotatef(qbDrawCommands cmds, float angle, float x, float y, float z) {
  mat4s& t = cmds->cur.args.transform;
  t = glms_rotate(t, angle, { x, y, z });
}

void qb_draw_rotatev(qbDrawCommands cmds, float angle, vec3s axis) {
  mat4s& t = cmds->cur.args.transform;
  t = glms_rotate(t, angle, axis);
}

void qb_draw_translatef(qbDrawCommands cmds, float x, float y, float z) {
  mat4s& t = cmds->cur.args.transform;
  t = glms_translate(t, { x, y, z });
}

void qb_draw_translatev(qbDrawCommands cmds, vec3s pos) {
  mat4s& t = cmds->cur.args.transform;
  t = glms_translate(t, pos);
}

void qb_draw_scalef(qbDrawCommands cmds, float scale) {
  mat4s& t = cmds->cur.args.transform;
  t = glms_scale_uni(t, scale);
}

void qb_draw_scalev(qbDrawCommands cmds, vec3s scale) {
  mat4s& t = cmds->cur.args.transform;
  t = glms_scale(t, scale);
}

void qb_draw_colorf(qbDrawCommands cmds, float r, float g, float b) {
  cmds->cur.args.color = { r, g, b, 1.f };
}

void qb_draw_colorv(qbDrawCommands cmds, vec3s c) {
  cmds->cur.args.color = glms_vec4(c, cmds->cur.args.color.w);
}

void qb_draw_alpha(qbDrawCommands cmds, float alpha) {
  cmds->cur.args.color.w = alpha;
}

void qb_draw_material(qbDrawCommands cmds, struct qbMaterial_* material) {
  cmds->cur.args.material = material;
}

void qb_draw_mesh(qbDrawCommands cmds, struct qbMesh_* mesh) {
  assert(false && "unimplemented");
}

void qb_draw_tri(qbDrawCommands cmds, float x0, float y0, float x1, float y1, float x2, float y2) {
  cmds->cur.command.tri = { { {x0, y0}, {x1, y1}, {x2, y2} } };
  cmds->cur.type = QB_DRAW_TRI;
  cmds->commands[cmds->commands_count++] = std::move(cmds->cur);
}

void qb_draw_quad(qbDrawCommands cmds, float w, float h) {
  qb_draw_scalev(cmds, { w, h, 1.f });

  cmds->cur.command.quad = { w, h };
  cmds->cur.type = QB_DRAW_QUAD;
  cmds->commands[cmds->commands_count++] = std::move(cmds->cur);
}

void qb_draw_square(qbDrawCommands cmds, float size) {
  qb_draw_quad(cmds, size, size);
}

void qb_draw_cube(qbDrawCommands cmds, float size) {
  qb_draw_box(cmds, size, size, size);
}

void qb_draw_box(qbDrawCommands cmds, float w, float h, float d) {
  qb_draw_scalev(cmds, { w, h, d });

  cmds->cur.command.box = { w, h, d };
  cmds->cur.type = QB_DRAW_BOX;
  cmds->commands[cmds->commands_count++] = std::move(cmds->cur);
}

void qb_draw_circle(qbDrawCommands cmds, float r) {
  qb_draw_scalev(cmds, { r, r, 1.f });

  cmds->cur.command.circle = { r };
  cmds->cur.type = QB_DRAW_CIRCLE;
  cmds->commands[cmds->commands_count++] = std::move(cmds->cur);
}

void qb_draw_sphere(qbDrawCommands cmds, float r) {
  qb_draw_scalev(cmds, { r, r, r });

  cmds->cur.command.sphere = { r };
  cmds->cur.type = QB_DRAW_SPHERE;
  cmds->commands[cmds->commands_count++] = std::move(cmds->cur);
}

qbResult qb_draw_end(qbDrawCommands cmds) {
  qbRenderer renderer = qb_renderer();
  qbResult result = renderer->drawcommands_submit(renderer, cmds);
  
  qb_dealloc(cmd_allocator, cmds->commands);
  qb_dealloc(allocator, cmds);

  return result;
}

void qb_draw_commands(qbDrawCommands cmds, size_t* count, qbDrawCommand_** commands) {
  if (count) {
    *count = cmds->commands_count;
  }

  if (commands) {
    *commands = cmds->commands;
  }
}