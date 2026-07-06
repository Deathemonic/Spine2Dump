#ifndef SPINE2DUMP_GL_CONTEXT_H
#define SPINE2DUMP_GL_CONTEXT_H

typedef struct GlContext GlContext;

GlContext* gl_context_create(void);
void gl_context_destroy(GlContext* context);
void* gl_context_get_proc(const char* name);

#endif
