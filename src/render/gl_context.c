#include "gl_context.h"

#include <stdlib.h>

#if defined(_WIN32)

    #include <windows.h>

    #include <GL/gl.h>

struct GlContext {
    HWND window;
    HDC dc;
    HGLRC rc;
};

    #define WGL_CONTEXT_MAJOR_VERSION_ARB 0x2091
    #define WGL_CONTEXT_MINOR_VERSION_ARB 0x2092
    #define WGL_CONTEXT_PROFILE_MASK_ARB 0x9126
    #define WGL_CONTEXT_CORE_PROFILE_BIT_ARB 0x0001

typedef HGLRC(WINAPI* wglCreateContextAttribsARBProc)(HDC, HGLRC, const int*);

GlContext* gl_context_create(void) {
    WNDCLASSA wc = {
        .lpfnWndProc = DefWindowProcA,
        .hInstance = GetModuleHandleA(NULL),
        .lpszClassName = "spine2dumpGL",
    };
    RegisterClassA(&wc);
    HWND window = CreateWindowA(wc.lpszClassName, "", WS_OVERLAPPEDWINDOW, 0, 0, 1, 1, NULL, NULL,
                                wc.hInstance, NULL);
    if (window == NULL) {
        return NULL;
    }
    HDC dc = GetDC(window);
    PIXELFORMATDESCRIPTOR pfd = {
        .nSize = sizeof(pfd),
        .nVersion = 1,
        .dwFlags = PFD_DRAW_TO_WINDOW | PFD_SUPPORT_OPENGL,
        .iPixelType = PFD_TYPE_RGBA,
        .cColorBits = 32,
        .cAlphaBits = 8,
    };
    int pixel_format = ChoosePixelFormat(dc, &pfd);
    SetPixelFormat(dc, pixel_format, &pfd);

    HGLRC dummy = wglCreateContext(dc);
    wglMakeCurrent(dc, dummy);
    wglCreateContextAttribsARBProc create_attribs = (wglCreateContextAttribsARBProc)
        wglGetProcAddress("wglCreateContextAttribsARB");
    if (create_attribs == NULL) {
        wglMakeCurrent(NULL, NULL);
        wglDeleteContext(dummy);
        ReleaseDC(window, dc);
        DestroyWindow(window);
        return NULL;
    }

    int attribs[] = {
        WGL_CONTEXT_MAJOR_VERSION_ARB,
        3,
        WGL_CONTEXT_MINOR_VERSION_ARB,
        3,
        WGL_CONTEXT_PROFILE_MASK_ARB,
        WGL_CONTEXT_CORE_PROFILE_BIT_ARB,
        0,
    };
    HGLRC rc = create_attribs(dc, NULL, attribs);
    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(dummy);
    if (rc == NULL) {
        ReleaseDC(window, dc);
        DestroyWindow(window);
        return NULL;
    }
    wglMakeCurrent(dc, rc);

    GlContext* context = calloc(1, sizeof(*context));
    if (context == NULL) {
        wglMakeCurrent(NULL, NULL);
        wglDeleteContext(rc);
        ReleaseDC(window, dc);
        DestroyWindow(window);
        return NULL;
    }
    context->window = window;
    context->dc = dc;
    context->rc = rc;
    return context;
}

void gl_context_destroy(GlContext* context) {
    if (context == NULL) {
        return;
    }
    wglMakeCurrent(NULL, NULL);
    wglDeleteContext(context->rc);
    ReleaseDC(context->window, context->dc);
    DestroyWindow(context->window);
    free(context);
}

void* gl_context_get_proc(const char* name) {
    void* proc = (void*)wglGetProcAddress(name);
    if (proc == NULL || proc == (void*)0x1 || proc == (void*)0x2 || proc == (void*)0x3 ||
        proc == (void*)-1) {
        static HMODULE gl = NULL;
        if (gl == NULL) {
            gl = LoadLibraryA("opengl32.dll");
        }
        proc = (void*)GetProcAddress(gl, name);
    }
    return proc;
}

#elif defined(__APPLE__)

    #include <OpenGL/OpenGL.h>
    #include <dlfcn.h>

struct GlContext {
    CGLContextObj context;
};

GlContext* gl_context_create(void) {
    CGLPixelFormatAttribute attributes[] = {
        kCGLPFAOpenGLProfile,        (CGLPixelFormatAttribute)kCGLOGLPVersion_3_2_Core,
        kCGLPFAAccelerated,          kCGLPFAColorSize,
        (CGLPixelFormatAttribute)24, (CGLPixelFormatAttribute)0,
    };
    CGLPixelFormatObj pixel_format = NULL;
    GLint format_count = 0;
    if (CGLChoosePixelFormat(attributes, &pixel_format, &format_count) != kCGLNoError) {
        return NULL;
    }
    CGLContextObj cgl_context = NULL;
    CGLError error = CGLCreateContext(pixel_format, NULL, &cgl_context);
    CGLDestroyPixelFormat(pixel_format);
    if (error != kCGLNoError) {
        return NULL;
    }
    if (CGLSetCurrentContext(cgl_context) != kCGLNoError) {
        CGLDestroyContext(cgl_context);
        return NULL;
    }
    GlContext* context = calloc(1, sizeof(*context));
    if (context == NULL) {
        CGLSetCurrentContext(NULL);
        CGLDestroyContext(cgl_context);
        return NULL;
    }
    context->context = cgl_context;
    return context;
}

void gl_context_destroy(GlContext* context) {
    if (context == NULL) {
        return;
    }
    CGLSetCurrentContext(NULL);
    CGLDestroyContext(context->context);
    free(context);
}

void* gl_context_get_proc(const char* name) {
    static void* library = NULL;
    if (library == NULL) {
        library = dlopen("/System/Library/Frameworks/OpenGL.framework/OpenGL", RTLD_LAZY);
    }
    return dlsym(library, name);
}

#else

    #include <EGL/egl.h>
    #include <EGL/eglext.h>

struct GlContext {
    EGLDisplay display;
    EGLContext context;
};

GlContext* gl_context_create(void) {
    EGLDisplay display = eglGetDisplay(EGL_DEFAULT_DISPLAY);
    if (display == EGL_NO_DISPLAY) {
        return NULL;
    }
    if (!eglInitialize(display, NULL, NULL)) {
        return NULL;
    }
    if (!eglBindAPI(EGL_OPENGL_API)) {
        return NULL;
    }
    EGLint config_attributes[] = {
        EGL_SURFACE_TYPE, EGL_PBUFFER_BIT, EGL_RENDERABLE_TYPE, EGL_OPENGL_BIT, EGL_NONE,
    };
    EGLConfig config = NULL;
    EGLint config_count = 0;
    if (!eglChooseConfig(display, config_attributes, &config, 1, &config_count) ||
        config_count < 1) {
        eglTerminate(display);
        return NULL;
    }
    EGLint context_attributes[] = {
        EGL_CONTEXT_MAJOR_VERSION,
        3,
        EGL_CONTEXT_MINOR_VERSION,
        3,
        EGL_CONTEXT_OPENGL_PROFILE_MASK,
        EGL_CONTEXT_OPENGL_CORE_PROFILE_BIT,
        EGL_NONE,
    };
    EGLContext egl_context = eglCreateContext(display, config, EGL_NO_CONTEXT, context_attributes);
    if (egl_context == EGL_NO_CONTEXT) {
        eglTerminate(display);
        return NULL;
    }
    if (!eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, egl_context)) {
        eglDestroyContext(display, egl_context);
        eglTerminate(display);
        return NULL;
    }
    GlContext* context = calloc(1, sizeof(*context));
    if (context == NULL) {
        eglMakeCurrent(display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
        eglDestroyContext(display, egl_context);
        eglTerminate(display);
        return NULL;
    }
    context->display = display;
    context->context = egl_context;
    return context;
}

void gl_context_destroy(GlContext* context) {
    if (context == NULL) {
        return;
    }
    eglMakeCurrent(context->display, EGL_NO_SURFACE, EGL_NO_SURFACE, EGL_NO_CONTEXT);
    eglDestroyContext(context->display, context->context);
    eglTerminate(context->display);
    free(context);
}

void* gl_context_get_proc(const char* name) {
    return (void*)eglGetProcAddress(name);
}

#endif

#if defined(_WIN32)
    #include <GL/gl.h>
#elif defined(__APPLE__)
    #include <OpenGL/gl3.h>
#else
    #include <GL/gl.h>
#endif

#ifndef APIENTRY
    #define APIENTRY
#endif
#ifndef GL_FRAMEBUFFER
    #define GL_FRAMEBUFFER 0x8D40
#endif
#ifndef GL_COLOR_ATTACHMENT0
    #define GL_COLOR_ATTACHMENT0 0x8CE0
#endif

typedef void(APIENTRY* GlGenFramebuffers)(GLsizei, GLuint*);
typedef void(APIENTRY* GlBindFramebuffer)(GLenum, GLuint);
typedef void(APIENTRY* GlFramebufferTexture2D)(GLenum, GLenum, GLenum, GLuint, GLint);
typedef void(APIENTRY* GlDeleteFramebuffers)(GLsizei, const GLuint*);

int gl_context_read_rgba(unsigned int gl_texture,
                         unsigned int tex_target,
                         int width,
                         int height,
                         unsigned char* pixels) {
    GlGenFramebuffers gen_framebuffers = (GlGenFramebuffers)gl_context_get_proc(
        "glGenFramebuffers");
    GlBindFramebuffer bind_framebuffer = (GlBindFramebuffer)gl_context_get_proc(
        "glBindFramebuffer");
    GlFramebufferTexture2D framebuffer_texture = (GlFramebufferTexture2D)gl_context_get_proc(
        "glFramebufferTexture2D");
    GlDeleteFramebuffers delete_framebuffers = (GlDeleteFramebuffers)gl_context_get_proc(
        "glDeleteFramebuffers");
    if (gen_framebuffers == NULL || bind_framebuffer == NULL || framebuffer_texture == NULL ||
        delete_framebuffers == NULL) {
        return -1;
    }

    GLuint framebuffer = 0;
    gen_framebuffers(1, &framebuffer);
    bind_framebuffer(GL_FRAMEBUFFER, framebuffer);
    framebuffer_texture(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, (GLenum)tex_target,
                        (GLuint)gl_texture, 0);
    glReadPixels(0, 0, width, height, GL_RGBA, GL_UNSIGNED_BYTE, pixels);
    bind_framebuffer(GL_FRAMEBUFFER, 0);
    delete_framebuffers(1, &framebuffer);
    return 0;
}
