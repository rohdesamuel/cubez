#ifndef RENDERING__H
#define RENDERING__H

#include "common.h"
#ifdef __COMPILE_AS_LINUX__
#include <GL/glew.h>
#include <stdio.h>
#include <stdlib.h>
#include <X11/X.h>
#include <X11/Xlib.h>
#include <GL/gl.h>
#include <GL/glx.h>
#include <GL/glu.h>
#endif

struct RenderingContext {
#ifdef __COMPILE_AS_LINUX__
Display                 *dpy;
Window                  root;
GLint*                   att;
XVisualInfo             *vi;
Colormap                cmap;
XSetWindowAttributes    swa;
Window                  win;
GLXContext              glc;
XWindowAttributes       gwa;
XEvent                  xev;
#endif
};

void init_rendering(RenderingContext* context, int width, int height);

#endif  // RENDERING__H
