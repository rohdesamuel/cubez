#include "rendering.h"

#include <cstring>
#include <string>

void init_rendering(RenderingContext* context, GLint attributes[], int width, int height) {
  memset(context, 0, sizeof(RenderingContext));
  context->dpy = XOpenDisplay(NULL);

  if(context->dpy == NULL) {
    printf("\n\tcannot connect to X server\n\n");
    exit(0);
  }

  context->root = DefaultRootWindow(context->dpy);

  context->vi = glXChooseVisual(context->dpy, 0, attributes);

  if(context->vi == NULL) {
    printf("\n\tno appropriate visual found\n\n");
    exit(0);
  } 
  else {
    printf("\n\tvisual %p selected\n", (void *)context->vi->visualid);
  }

  context->cmap = XCreateColormap(context->dpy, context->root,
                                  context->vi->visual, AllocNone);

  context->swa.colormap = context->cmap;
  context->swa.event_mask = ExposureMask | KeyPressMask;

  context->win = XCreateWindow(
      context->dpy, context->root, 0, 0, width, height, 0,
      context->vi->depth, InputOutput, context->vi->visual,
      CWColormap | CWEventMask, &context->swa);

  XMapWindow(context->dpy, context->win);
  XStoreName(context->dpy, context->win, "Radiance Example");

  context->glc = glXCreateContext(context->dpy, context->vi, NULL, GL_TRUE);
  glXMakeCurrent(context->dpy, context->win, context->glc);
}
