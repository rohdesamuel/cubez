#ifndef CUBEZ_DRAW__H
#define CUBEZ_DRAW__H

#include <cubez/cubez.h>
#include <cubez/render_pipeline.h>

typedef struct qbDrawCommands_* qbDrawCommands;

QB_API qbDrawCommands qb_draw_begin(const struct qbCamera_* camera);
QB_API void qb_draw_settransform(qbDrawCommands cmds, mat4s transform);
QB_API void qb_draw_gettransform(qbDrawCommands cmds, mat4s* transform);
QB_API void qb_draw_identity(qbDrawCommands cmds);

QB_API void qb_draw_pushtransform(qbDrawCommands cmds);
QB_API void qb_draw_poptransform(qbDrawCommands cmds);

QB_API void qb_draw_rotatef(qbDrawCommands cmds, float angle, float x, float y, float z);
QB_API void qb_draw_rotatev(qbDrawCommands cmds, float angle, vec3s axis);
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
QB_API qbResult qb_draw_end(qbDrawCommands cmds);


typedef enum qbDrawCommandType_ {
  QB_DRAW_NOOP,
  QB_DRAW_CAMERA,
  QB_DRAW_TRI,
  QB_DRAW_QUAD,
  QB_DRAW_BOX,
  QB_DRAW_CIRCLE,
  QB_DRAW_SPHERE,
  QB_DRAW_MESH,
  QB_DRAW_ENTITY,  
} qbDrawCommandType_;

typedef struct qbDrawCommandInit_ {
  const struct qbCamera_* camera;
} qbDrawCommandInit_;

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
};

typedef struct qbDrawCommandCircle_ {
  float r;
};

typedef struct qbDrawCommandSphere_ {
  float r;
};

typedef struct qbDrawCommandMesh_ {
  struct qbMesh_* mesh;
};

typedef struct qbDrawCommandInstanced_ {
  struct qbMesh_* mesh;
  mat4s* transforms;
  size_t instance_count;
};

typedef struct qbDrawCommandArgs_ {
  vec4s color;
  mat4s transform;
  struct qbMaterial_* material;

} qbDrawCommandArgs_, * qbDrawCommandArgs;

typedef struct qbDrawCommand_ {
  qbDrawCommandType_ type;
  union {
    qbDrawCommandInit_ init;
    qbDrawCommandTri_ tri;
    qbDrawCommandQuad_ quad;
    qbDrawCommandBox_ box;
    qbDrawCommandCircle_ circle;
    qbDrawCommandSphere_ sphere;
    qbDrawCommandMesh_ mesh;
  } command;
  qbDrawCommandArgs_ args;

} qbDrawCommand_, * qbDrawCommand;

QB_API void qb_draw_commands(qbDrawCommands cmds, size_t* count, struct qbDrawCommand_** commands);

#endif  // CUBEZ_DRAW__H