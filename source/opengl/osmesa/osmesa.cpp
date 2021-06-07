/*
 *  Copyright (C) 2016  The BoxedWine Team
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA.
 */
#include "boxedwine.h"

#ifdef BOXEDWINE_OPENGL_OSMESA
#include <SDL_opengl.h>
#include <SDL.h>
#include "../glcommon.h"

#include <stdio.h>
#include "knativewindow.h"

#include "../../../lib/mesa/include/osmesa.h"

#ifdef BOXEDWINE_MSVC
#define LIBRARY_NAME "osmesa.dll"
#else 
#define LIBRARY_NAME Name needs to be set for this platform
#endif

//typedef OSMesaContext (GLAPIENTRY* pOSMesaCreateContext)(GLenum format, OSMesaContext sharelist);
//typedef OSMesaContext (GLAPIENTRY* fn_OSMesaCreateContextExt)(GLenum format, GLint depthBits, GLint stencilBits, GLint accumBits, OSMesaContext sharelist);
typedef OSMesaContext (GLAPIENTRY* fn_OSMesaCreateContextAttribs)(const int* attribList, OSMesaContext sharelist);
typedef void (GLAPIENTRY* fn_OSMesaDestroyContext)(OSMesaContext ctx);
typedef GLboolean (GLAPIENTRY* fn_OSMesaMakeCurrent)(OSMesaContext ctx, void* buffer, GLenum type, GLsizei width, GLsizei height);
typedef OSMESAproc (GLAPIENTRY* fn_OSMesaGetProcAddress)(const char* funcName);
typedef void (GLAPIENTRY* fn_OSMesaPixelStore)(GLint pname, GLint value);

typedef void(OPENGL_CALL_TYPE* fn_glFinish)();
typedef void(OPENGL_CALL_TYPE* fn_glFlush)();

static fn_glFinish pglFinish;
static fn_glFlush pglFlush;
static fn_OSMesaMakeCurrent pOSMesaMakeCurrent;
static fn_OSMesaCreateContextAttribs pOSMesaCreateContextAttribs;
static fn_OSMesaDestroyContext pOSMesaDestroyContext;
static fn_OSMesaPixelStore pOSMesaPixelStore;
static fn_OSMesaGetProcAddress pOSMesaGetProcAddress;

#include "../boxedwineGL.h"
#include "../../sdl/startupArgs.h"

class MesaBoxedwineGlContext {
public:
    MesaBoxedwineGlContext() : buffer(NULL), width(0), height(0), pitch(0), bpp(0), profile(0), major(0), minor(0), pixelFormat(NULL), context(NULL) {}
    ~MesaBoxedwineGlContext() {
        if (buffer) {
            delete[] buffer;
        }
        if (context) {
            pOSMesaDestroyContext(context);
        }
    }
    U8* buffer;
    U32 width;
    U32 height;
    U32 pitch;
    U32 bpp;
    U32 profile;
    U32 major;
    U32 minor;
    PixelFormat* pixelFormat;
    OSMesaContext context;
    std::shared_ptr<Wnd> wnd;
};

class MesaBoxedwineGL : public BoxedwineGL {
public:
    virtual void deleteContext(void* context);
    virtual bool makeCurrent(void* context, void* window);
    virtual std::string getLastError();
    virtual void* createContext(void* window, std::shared_ptr<Wnd> wnd, PixelFormat* pixelFormat, U32 width, U32 height, int major, int minor, int profile);
    virtual void swapBuffer(void* window);
    virtual void setSwapInterval(U32 vsync);
    virtual bool shareList(KThreadGlContext* src, KThreadGlContext* dst, void* window);

private:
    void* internalCreateContext(void* window, std::shared_ptr<Wnd> wnd, PixelFormat* pixelFormat, U32 width, U32 height, int major, int minor, int profile, OSMesaContext sharedContext);
};

void MesaBoxedwineGL::deleteContext(void* context) {
    MesaBoxedwineGlContext* c = (MesaBoxedwineGlContext*)context;    
    delete c;
}

bool MesaBoxedwineGL::makeCurrent(void* context, void* window) {
    if (!context) {
        return true;
    }
    MesaBoxedwineGlContext* c = (MesaBoxedwineGlContext*)context;
    c->buffer = new U8[c->width * c->height * 4];
    if (pOSMesaMakeCurrent(c->context, c->buffer, GL_UNSIGNED_BYTE, c->width, c->height)) {
        pOSMesaPixelStore(OSMESA_Y_UP, 0);
        return true;
    }
    return false;
}

std::string MesaBoxedwineGL::getLastError() {
    return "";
}

void* MesaBoxedwineGL::createContext(void* window, std::shared_ptr<Wnd> wnd, PixelFormat* pixelFormat, U32 width, U32 height, int major, int minor, int profile) {
    return internalCreateContext(window, wnd, pixelFormat, width, height, major, minor, profile, NULL);
}

void* MesaBoxedwineGL::internalCreateContext(void* window, std::shared_ptr<Wnd> wnd, PixelFormat * pixelFormat, U32 width, U32 height, int major, int minor, int profile, OSMesaContext sharedContext) {
    MesaBoxedwineGlContext* c = new MesaBoxedwineGlContext();
    int attribs[100], n = 0;

    attribs[n++] = OSMESA_FORMAT;
    attribs[n++] = OSMESA_BGRA;
    attribs[n++] = OSMESA_DEPTH_BITS;
    attribs[n++] = pixelFormat->cDepthBits;
    attribs[n++] = OSMESA_STENCIL_BITS;
    attribs[n++] = pixelFormat->cStencilBits;
    attribs[n++] = OSMESA_ACCUM_BITS;
    attribs[n++] = pixelFormat->cAccumBits;
    if (profile) {
        attribs[n++] = OSMESA_PROFILE;
        attribs[n++] = profile;
    }
    if (major) {
        attribs[n++] = OSMESA_CONTEXT_MAJOR_VERSION;
        attribs[n++] = major;
        attribs[n++] = OSMESA_CONTEXT_MINOR_VERSION;
        attribs[n++] = minor;        
    }
    attribs[n++] = 0;

    c->context = pOSMesaCreateContextAttribs(attribs, sharedContext);
    c->width = width;
    c->height = height;   
    c->bpp = pixelFormat->cColorBits;
    c->pitch = width * 4;
    c->pixelFormat = pixelFormat;
    c->wnd = wnd;
    c->profile = profile;
    c->major = major;
    c->minor = minor;
    return c;
}

void MesaBoxedwineGL::swapBuffer(void* window) {
    MesaBoxedwineGlContext* c = (MesaBoxedwineGlContext*)KThread::currentThread()->currentContext;
    pglFlush();
    KNativeWindow::getNativeWindow()->bltWnd(KThread::currentThread(), c->wnd, c->buffer, c->pitch, c->bpp, c->width, c->height);
    KNativeWindow::getNativeWindow()->drawWindow(c->wnd);
}

void MesaBoxedwineGL::setSwapInterval(U32 vsync) {

}

bool MesaBoxedwineGL::shareList(KThreadGlContext* src, KThreadGlContext* dst, void* window) {
    if (src && dst) {
        if (dst->hasBeenMadeCurrent) {
            klog("could not share display lists, the destination context has been current already");
            return 0;
        } else if (dst->sharing)
        {
            klog("could not share display lists because dest has already shared lists before\n");
            return 0;
        }
        MesaBoxedwineGlContext* dstContext = (MesaBoxedwineGlContext*)dst->context;
        MesaBoxedwineGlContext* srcContext = (MesaBoxedwineGlContext*)src->context;

        dst->context = internalCreateContext(NULL, dstContext->wnd, dstContext->pixelFormat, dstContext->width, dstContext->height, dstContext->major, dstContext->minor, dstContext->profile, srcContext->context);
        dst->sharing = true;
        deleteContext(dstContext);
        return true;
    }
    return false;
}

static MesaBoxedwineGL mesaBoxedwineGL;

static bool isAvailable = false;
static bool isAvailableInitialized = false;

bool isMesaOpenglAvailable() {
    if (!isAvailableInitialized) {
        isAvailableInitialized = true;
        void* dll = SDL_LoadObject(LIBRARY_NAME);
        if (dll) {
            isAvailable = true;
            SDL_UnloadObject(dll);
        }
    }
    return isAvailable;
}

void glExtensionsLoaded();
static bool mesaOpenGlExtensionsLoaded = false;

void loadMesaExtensions() {
    if (!mesaOpenGlExtensionsLoaded) {
        mesaOpenGlExtensionsLoaded = true;
        glExtensionsLoaded();
    }
}

// GLAPI void APIENTRY glFinish( void ) {
void osmesa_glFinish(CPU* cpu) {
    pglFinish();
}

// GLAPI void APIENTRY glFlush( void ) {
void osmesa_glFlush(CPU* cpu) {
    MesaBoxedwineGlContext* c = (MesaBoxedwineGlContext*)KThread::currentThread()->currentContext;
    pglFlush();
    KNativeWindow::getNativeWindow()->bltWnd(KThread::currentThread(), c->wnd, c->buffer, c->pitch, c->bpp, c->width, c->height);
    KNativeWindow::getNativeWindow()->drawWindow(c->wnd);
}

// GLXContext glXCreateContext(Display *dpy, XVisualInfo *vis, GLXContext share_list, Bool direct)
void osmesa_glXCreateContext(CPU* cpu) {

}

// void glXDestroyContext(Display *dpy, GLXContext ctx)
void osmesa_glXDestroyContext(CPU* cpu) {

}

// Bool glXMakeCurrent(Display *dpy, GLXDrawable drawable, GLXContext ctx) 
void osmesa_glXMakeCurrent(CPU* cpu) {
    //U32 isWindow = ARG5;
    //U32 depth = ARG4;
    //U32 height = ARG3;
    //U32 width = ARG2;
}

// void glXSwapBuffers(Display *dpy, GLXDrawable drawable)

void osmesa_glXSwapBuffers(CPU* cpu) {
}

static void* pDLL;

#undef GL_FUNCTION
#define GL_FUNCTION(func, RET, PARAMS, ARGS, PRE, POST, LOG) pgl##func = (gl##func##_func)pOSMesaGetProcAddress("gl" #func);

#undef GL_FUNCTION_CUSTOM
#define GL_FUNCTION_CUSTOM(func, RET, PARAMS) pgl##func = (gl##func##_func)pOSMesaGetProcAddress("gl" #func);

#undef GL_EXT_FUNCTION
#define GL_EXT_FUNCTION(func, RET, PARAMS) ext_gl##func = (gl##func##_func)pOSMesaGetProcAddress("gl" #func);

void initMesaOpenGL() {
    if (BoxedwineGL::current != &mesaBoxedwineGL) {
        BoxedwineGL::current = &mesaBoxedwineGL;
        mesaOpenGlExtensionsLoaded = false;
        if (!pDLL) {
            pDLL = SDL_LoadObject(LIBRARY_NAME);
            if (!pDLL) {
                return;
            }
        }
        pOSMesaGetProcAddress = (fn_OSMesaGetProcAddress)SDL_LoadFunction(pDLL, "OSMesaGetProcAddress");
        if (!pOSMesaGetProcAddress) {
            return;
        }
        pOSMesaMakeCurrent = (fn_OSMesaMakeCurrent)SDL_LoadFunction(pDLL, "OSMesaMakeCurrent");
        pOSMesaCreateContextAttribs = (fn_OSMesaCreateContextAttribs)SDL_LoadFunction(pDLL, "OSMesaCreateContextAttribs");
        pOSMesaDestroyContext = (fn_OSMesaDestroyContext)SDL_LoadFunction(pDLL, "OSMesaDestroyContext");
        pOSMesaPixelStore = (fn_OSMesaPixelStore)SDL_LoadFunction(pDLL, "OSMesaPixelStore");

        pglFinish = pOSMesaGetProcAddress("glFinish");
        pglFlush = pOSMesaGetProcAddress("glFlush");
#include "../glfunctions.h"

        int99Callback[Finish] = osmesa_glFinish;
        int99Callback[Flush] = osmesa_glFlush;
        int99Callback[XCreateContext] = osmesa_glXCreateContext;
        int99Callback[XMakeCurrent] = osmesa_glXMakeCurrent;
        int99Callback[XDestroyContext] = osmesa_glXDestroyContext;
        int99Callback[XSwapBuffer] = osmesa_glXSwapBuffers;
    }
}

void shutdownMesaOpenGL() {
    if (pDLL) {
        SDL_UnloadObject(pDLL);
        pDLL = NULL;
    }
}

#endif