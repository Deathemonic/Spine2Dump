#ifndef SPINE2DUMP_GL_CONTEXT_H
#define SPINE2DUMP_GL_CONTEXT_H

typedef struct GlContext GlContext;

GlContext* gl_context_create(void);
void gl_context_destroy(GlContext* context);
void* gl_context_get_proc(const char* name);
int gl_context_read_rgba(unsigned int gl_texture,
                         unsigned int tex_target,
                         int width,
                         int height,
                         unsigned char* pixels);

#endif
