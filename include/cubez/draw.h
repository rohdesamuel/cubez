#ifndef CUBEZ_DRAW__H
#define CUBEZ_DRAW__H

#include <cubez/cubez.h>
#include <cubez/render_pipeline.h>
#include <cubez/mesh.h>

typedef struct qbDrawCommands_* qbDrawCommands;
typedef struct qbCommandBatch_* qbCommandBatch;
typedef struct qbDrawBatch_* qbDrawBatch;

// Should be called once per frame.
QB_API qbResult qb_draw_beginframe(const struct qbCamera_* camera, qbClearValue clear);

QB_API qbDrawCommands qb_draw_begin();
QB_API qbResult qb_draw_end(qbDrawCommands* cmds);

typedef struct qbDrawCompileAttr_ {
  qbBool is_dynamic;
} qbDrawCompileAttr_, *qbDrawCompileAttr;
QB_API qbCommandBatch qb_draw_compile(qbDrawCommands* cmds, qbDrawCompileAttr attr);

typedef struct qbDrawBatch_ {
  qbMesh mesh;
  size_t count;
  mat4s* transforms;
  //uint8_t* attributes;
  size_t attributes_size;
  qbBool is_dynamic;
} qbDrawBatch_, *qbDrawBatch;

QB_API void qb_draw_settransform(qbDrawCommands cmds, mat4s transform);
QB_API void qb_draw_gettransform(qbDrawCommands cmds, mat4s* transform);
QB_API void qb_draw_identity(qbDrawCommands cmds);

QB_API void qb_draw_pushtransform(qbDrawCommands cmds);
QB_API void qb_draw_poptransform(qbDrawCommands cmds);

QB_API void qb_draw_multransform(qbDrawCommands cmds, mat4s* transform);
QB_API void qb_draw_rotatef(qbDrawCommands cmds, float angle, float x, float y, float z);
QB_API void qb_draw_rotatev(qbDrawCommands cmds, float angle, vec3s axis);
QB_API void qb_draw_rotateq(qbDrawCommands cmds, versors q);
QB_API void qb_draw_translatef(qbDrawCommands cmds, float x, float y, float z);
QB_API void qb_draw_translatev(qbDrawCommands cmds, vec3s pos);
QB_API void qb_draw_scalef(qbDrawCommands cmds, float scale);
QB_API void qb_draw_scalev(qbDrawCommands cmds, vec3s scale);
QB_API void qb_draw_colorf(qbDrawCommands cmds, float r, float g, float b);
QB_API void qb_draw_colorv(qbDrawCommands cmds, vec3s c);
QB_API void qb_draw_alpha(qbDrawCommands cmds, float alpha);
QB_API void qb_draw_material(qbDrawCommands cmds, struct qbMaterial_* material);

QB_API void qb_draw_tri(qbDrawCommands cmds, float x0, float y0, float x1, float y1, float x2, float y2);
QB_API void qb_draw_quad(qbDrawCommands cmds, float w, float h);
QB_API void qb_draw_circle(qbDrawCommands cmds, float r);

QB_API void qb_draw_square(qbDrawCommands cmds, float size);
QB_API void qb_draw_cube(qbDrawCommands cmds, float size);
QB_API void qb_draw_box(qbDrawCommands cmds, float w, float h, float d);
QB_API void qb_draw_sphere(qbDrawCommands cmds, float r);
QB_API void qb_draw_mesh(qbDrawCommands cmds, struct qbMesh_* mesh);
QB_API void qb_draw_model(qbDrawCommands cmds, struct qbModel_* model);
QB_API void qb_draw_instanced(qbDrawCommands cmds, struct qbMesh_* mesh, size_t instance_count, mat4s* transforms);
QB_API void qb_draw_batch(qbDrawCommands cmds, qbCommandBatch batch);
QB_API void qb_draw_custom(qbDrawCommands cmds, int command_type, qbVar arg);

// Unimplemented.
QB_API void qb_draw_sprite(qbDrawCommands cmds, struct qbSprite_* sprite);

// Unimplemented.
QB_API void qb_draw_spritepart(qbDrawCommands cmds, struct qbSprite_* sprite, int32_t left, int32_t top, int32_t width, int32_t height);

QB_API qbDrawCommandBuffer qb_commandbatch_cmds(qbCommandBatch batch, uint32_t idx);
QB_API qbBool qb_commandbatch_isdynamic(qbCommandBatch batch);
QB_API qbBool qb_commandbatch_isstatic(qbCommandBatch batch);

typedef enum qbDrawCommandType_ {
  QB_DRAW_NOOP,
  QB_DRAW_BEGIN,
  QB_DRAW_END,
  QB_DRAW_TRI,
  QB_DRAW_QUAD,
  QB_DRAW_BOX,
  QB_DRAW_CIRCLE,
  QB_DRAW_SPHERE,
  QB_DRAW_MESH,
  QB_DRAW_ENTITY,
  QB_DRAW_BATCH,
  QB_DRAW_SPRITE,
  QB_DRAW_CUSTOM
} qbDrawCommandType_;

typedef struct qbDrawCommandBegin_ {
  uint64_t reserved;
} qbDrawCommandBegin_;

typedef struct qbDrawCommandEnd_ {
  uint64_t reserved;
} qbDrawCommandEnd_;

typedef struct qbDrawCommandTri_ {
  vec2s points[3];
} qbDrawCommandTri_;

typedef struct qbDrawCommandQuad_ {
  float w;
  float h;
} qbDrawCommandQuad_;

typedef struct qbDrawCommandBox_ {
  float w;
  float h;
  float d;
} qbDrawCommandBox_;

typedef struct qbDrawCommandCircle_ {
  float r;
} qbDrawCommandCircle_;

typedef struct qbDrawCommandSphere_ {
  float r;
} qbDrawCommandSphere_;

typedef struct qbDrawCommandMesh_ {
  struct qbMesh_* mesh;
} qbDrawCommandMesh_;

typedef struct qbDrawCommandInstanced_ {
  struct qbMesh_* mesh;
  mat4s* transforms;
  size_t instance_count;
} qbDrawCommandInstanced_;

typedef struct qbDrawCommandCustom_ {
  int command_type;
  qbVar data;
} qbDrawCommandCustom_;

typedef struct qbDrawCommandBatch_ {
  qbCommandBatch batch;
} qbDrawCommandBatch_;

typedef struct qbDrawCommandSprite_ {
  struct qbSprite_* sprite;

  int32_t left;
  int32_t top;
  int32_t width;
  int32_t height;
} qbDrawCommandSprite_;

typedef struct qbDrawCommandArgs_ {
  vec4s color;
  mat4s transform;
  struct qbMaterial_* material;

} qbDrawCommandArgs_, *qbDrawCommandArgs;

typedef struct qbDrawCommand_ {
  qbDrawCommandType_ type;
  union {
    qbDrawCommandBegin_ begin;
    qbDrawCommandEnd_ end;
    qbDrawCommandTri_ tri;
    qbDrawCommandQuad_ quad;
    qbDrawCommandBox_ box;
    qbDrawCommandCircle_ circle;
    qbDrawCommandSphere_ sphere;
    qbDrawCommandMesh_ mesh;
    qbDrawCommandBatch_ batch;
    qbDrawCommandSprite_ sprite;
    qbDrawCommandCustom_ custom;
  } command;
  qbDrawCommandArgs_ args;

} qbDrawCommand_, *qbDrawCommand;

#endif  // CUBEZ_DRAW__H