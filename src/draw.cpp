#include <cubez/draw.h>

#include <cglm/struct.h>

#include <cubez/memory.h>
#include <cubez/render.h>
#include <vector>

size_t MAX_DRAW_COMMANDS = 1024;
size_t MAX_STACK_TRANSFORM_COUNT = 1024;

struct qbDrawCommands_ {
  qbDrawCommand_* commands;
  size_t commands_count;

  mat4s* transform_stack;
  int32_t push_count;

  qbDrawCommand_ cur;
};

struct qbDrawBatch_ {
  qbDrawCommand_* commands;
  size_t commands_count;
};

static thread_local qbMemoryAllocator allocator = nullptr;
static thread_local qbMemoryAllocator cmd_allocator = nullptr;

void qb_draw_init(qbDrawCommands cmds) {
  cmds->commands[cmds->commands_count++] = {
    .type = QB_DRAW_BEGIN,
    .command = qbDrawCommandBegin_{}
  };
}

qbResult qb_draw_beginframe(const struct qbCamera_* camera) {
  qbRenderer renderer = qb_renderer();
  return renderer->draw_beginframe(renderer, camera);
}

void qb_draw_dealloc(qbDrawCommands* cmds) {
  qb_dealloc(cmd_allocator, (*cmds)->commands);
  qb_dealloc(allocator, (*cmds)->transform_stack);
  qb_dealloc(allocator, (*cmds));

  *cmds = nullptr;
}

qbDrawCommands qb_draw_begin() {
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
  ret->transform_stack = (mat4s*)qb_alloc(allocator, sizeof(mat4s) * MAX_STACK_TRANSFORM_COUNT);
  ret->cur.args.transform = GLMS_MAT4_IDENTITY_INIT;
  ret->cur.args.color = { 0.f, 0.f, 0.f, 1.f };

  qb_draw_init(ret);

  return ret;
}

qbResult qb_draw_end(qbDrawCommands* cmds) {
  qbRenderer renderer = qb_renderer();
  (*cmds)->commands[(*cmds)->commands_count++] = {
    .type = QB_DRAW_END,
    .command = {}
  };

  qbResult result = renderer->drawcommands_submit(renderer, (*cmds)->commands_count, (*cmds)->commands);

  if ((*cmds)->push_count > 0)
    assert(false && "Unpopped transforms exist.");

  if ((*cmds)->push_count < 0)
    assert(false && "Too many transforms were popped.");

  qb_draw_dealloc(cmds);

  return result;
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

void qb_draw_pushtransform(qbDrawCommands cmds) {
  assert(cmds->push_count < MAX_STACK_TRANSFORM_COUNT);
  cmds->transform_stack[cmds->push_count++] = cmds->cur.args.transform;
}

void qb_draw_poptransform(qbDrawCommands cmds) {
  assert(cmds->push_count > 0);
  cmds->cur.args.transform = cmds->transform_stack[cmds->push_count--];
}

void qb_draw_multransform(qbDrawCommands cmds, mat4s* transform) {
  mat4s& t = cmds->cur.args.transform;
  t = glms_mat4_mul(t, *transform);
}

void qb_draw_rotatef(qbDrawCommands cmds, float angle, float x, float y, float z) {
  mat4s& t = cmds->cur.args.transform;
  t = glms_rotate(t, angle, { x, y, z });
}

void qb_draw_rotatev(qbDrawCommands cmds, float angle, vec3s axis) {
  mat4s& t = cmds->cur.args.transform;
  t = glms_rotate(t, angle, axis);
}

void qb_draw_rotateq(qbDrawCommands cmds, versors q) {
  mat4s& t = cmds->cur.args.transform;
  t = glms_quat_rotate(t, q);
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

void qb_draw_tri(qbDrawCommands cmds, float x0, float y0, float x1, float y1, float x2, float y2) {
  assert(cmds->commands_count < MAX_DRAW_COMMANDS);

  cmds->cur.command.tri = { { {x0, y0}, {x1, y1}, {x2, y2} } };
  cmds->cur.type = QB_DRAW_TRI;
  cmds->commands[cmds->commands_count++] = std::move(cmds->cur);
}

void qb_draw_quad(qbDrawCommands cmds, float w, float h) {
  assert(cmds->commands_count < MAX_DRAW_COMMANDS);

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
  assert(cmds->commands_count < MAX_DRAW_COMMANDS);

  qb_draw_scalev(cmds, { w, h, d });
  cmds->cur.command.box = { w, h, d };
  cmds->cur.type = QB_DRAW_BOX;
  cmds->commands[cmds->commands_count++] = std::move(cmds->cur);
}

void qb_draw_circle(qbDrawCommands cmds, float r) {
  assert(cmds->commands_count < MAX_DRAW_COMMANDS);

  qb_draw_scalev(cmds, { r, r, 1.f });
  cmds->cur.command.circle = { r };
  cmds->cur.type = QB_DRAW_CIRCLE;
  cmds->commands[cmds->commands_count++] = std::move(cmds->cur);
}

void qb_draw_sphere(qbDrawCommands cmds, float r) {
  assert(cmds->commands_count < MAX_DRAW_COMMANDS);

  qb_draw_scalev(cmds, { r, r, r });
  cmds->cur.command.sphere = { r };
  cmds->cur.type = QB_DRAW_SPHERE;
  cmds->commands[cmds->commands_count++] = std::move(cmds->cur);
}

void qb_draw_mesh(qbDrawCommands cmds, struct qbMesh_* mesh) {
  assert(cmds->commands_count < MAX_DRAW_COMMANDS);

  cmds->cur.command.mesh = { mesh };
  cmds->cur.type = QB_DRAW_MESH;
  cmds->commands[cmds->commands_count++] = std::move(cmds->cur);
}

void qb_draw_model(qbDrawCommands cmds, struct qbModel_* model) {
  assert(false && "unimplemented");
  assert(cmds->commands_count < MAX_DRAW_COMMANDS);
}

void qb_draw_instanced(qbDrawCommands cmds, struct qbMesh_* mesh, size_t instance_count, mat4s* transforms) {
  assert(false && "unimplemented");
  assert(cmds->commands_count < MAX_DRAW_COMMANDS);
}

void qb_draw_submitbuffer(qbDrawCommands cmds, qbDrawCommandBuffer buffer) {
  assert(cmds->commands_count < MAX_DRAW_COMMANDS);

  cmds->cur.command.submit_buffer = { buffer };
  cmds->cur.type = QB_DRAW_SUBMITBUFFER;
  cmds->commands[cmds->commands_count++] = std::move(cmds->cur);
}

void qb_draw_custom(qbDrawCommands cmds, int command_type, qbVar arg) {
  assert(cmds->commands_count < MAX_DRAW_COMMANDS);
  cmds->cur.command.custom = { command_type, arg };
  cmds->cur.type = QB_DRAW_CUSTOM;
  cmds->commands[cmds->commands_count++] = std::move(cmds->cur);
}

qbResult qb_drawbatch_create(qbDrawBatch* batch, qbDrawCommands cmds) {
  qbDrawBatch ret = new qbDrawBatch_{};
  ret->commands_count = cmds->commands_count;
  ret->commands = new qbDrawCommand_[ret->commands_count];

  for (size_t i = 0; i < ret->commands_count; ++i) {
    ret->commands[i] = cmds->commands[i];
  }

  *batch = ret;

  return QB_OK;
}

qbResult qb_drawbatch_destroy(qbDrawBatch* batch) {
  delete (*batch)->commands;
  delete *batch;

  *batch = nullptr;

  return QB_OK;
}

qbResult qb_draw_compile(qbDrawCommands* cmds, qbDrawCommandBuffer* buffer) {
  qbRenderer renderer = qb_renderer();
  qbResult result = renderer->drawcommands_compile(renderer, (*cmds)->commands_count, (*cmds)->commands, buffer);

  qb_draw_dealloc(cmds);
  return result;
}