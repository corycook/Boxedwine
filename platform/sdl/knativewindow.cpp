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
#include "knativewindow.h"
#include <SDL.h>

#include "kscheduler.h"
#include "crc.h"
#include "devfb.h"
#include "devinput.h"
#include "kunixsocket.h"
#include "sdlcallback.h"
#include "pixelformat.h"
#include "../../source/emulation/hardmmu/hard_memory.h"
#include "../../source/util/threadutils.h"
#include "../../source/sdl/startupArgs.h"
#include "../../source/opengl/boxedwineGL.h"

#if !defined(BOXEDWINE_DISABLE_UI) && !defined(__TEST)
#include "../../source/ui/mainui.h"
#endif

class WndSdl : public Wnd {
public:
    WndSdl() : pixelFormat(NULL), pixelFormatIndex(0), openGlContext(NULL), activated(0), processId(0), hwnd(0)
#ifdef BOXEDWINE_RECORDER
        , bits(0), bitsSize(0)
#endif
        , sdlTexture(NULL), sdlTextureHeight(0), sdlTextureWidth(0)
    {}

    virtual void setText(char* text) {
    }

    virtual void show(bool bShow);
    virtual void destroy();

    virtual U32 glSetPixelFormat(U32 index) {
        pixelFormatIndex = index;
        pixelFormat = KSystem::getPixelFormat(index);
        return 1;
    }

    virtual U32 glGetPixelFormat() {
        return pixelFormatIndex;
    }

    virtual bool setFocus() {
        return true;
    }

    std::string text;
    PixelFormat* pixelFormat;
    U32 pixelFormatIndex;
    void* openGlContext;
    U32 activated;
    U32 processId;
    U32 hwnd;
#ifdef BOXEDWINE_RECORDER
    U8* bits;
    U32 bitsSize;
#endif
    SDL_Texture* sdlTexture;
    int sdlTextureHeight;
    int sdlTextureWidth;
};

U32 KNativeWindow::defaultScreenWidth = 800;
U32 KNativeWindow::defaultScreenHeight = 600;
U32 KNativeWindow::defaultScreenBpp = 32;
bool KNativeWindow::windowUpdated = false;
U32 sdlCustomEvent;

class KNativeWindowSdl : public KNativeWindow, public std::enable_shared_from_this<KNativeWindowSdl> {
public:
    KNativeWindowSdl() : scaleX(100), scaleXOffset(0), scaleY(100), scaleYOffset(0), sdlDesktopWidth(0), sdlDesktopHeight(0), fullScreen(FULLSCREEN_NOTSET), vsync(VSYNC_DEFAULT), window(NULL), renderer(NULL), shutdownWindow(NULL), shutdownRenderer(NULL), currentContext(NULL), contextCount(0), windowIsGL(false), glWindowVersionMajor(0), windowIsHidden(false), timeToHideUI(0), timeWindowWasCreated(0)
#ifdef BOXEDWINE_RECORDER
        , screenCopyTexture(NULL)
#endif
    {
        width = KNativeWindow::defaultScreenWidth;
        height = KNativeWindow::defaultScreenHeight;
        bpp = KNativeWindow::defaultScreenBpp;
    }
    ~KNativeWindowSdl() {
        for (auto& n : this->cursors) {
            SDL_FreeCursor(n.second);
        }
        this->cursors.clear();
    }

    U32 width;
    U32 height;
    U32 bpp;
    U32 scaleX;
    U32 scaleXOffset;
    U32 scaleY;
    U32 scaleYOffset;
    U32 sdlDesktopWidth;
    U32 sdlDesktopHeight;
    std::string scaleQuality;
    U32 fullScreen;
    U32 vsync;

    SDL_Window* window;
    SDL_Renderer* renderer;
    SDL_Window* shutdownWindow;
    SDL_Renderer* shutdownRenderer;
    void* currentContext;
    BOXEDWINE_MUTEX sdlMutex;
    int contextCount;
    bool windowIsGL;
    U32 glWindowVersionMajor;
    bool windowIsHidden;
    U32 timeToHideUI;
    U32 timeWindowWasCreated;
    std::string delayedCreateWindowMsg; // the ui will watch for this message
    std::unordered_map<std::string, SDL_Cursor*> cursors;
    std::unordered_map<U32, std::shared_ptr<WndSdl>> hwndToWnd;
    BOXEDWINE_MUTEX hwndToWndMutex;

    void screenResized(KThread* thread);

    virtual void screenChanged(KThread* thread, U32 width, U32 height, U32 bpp) {
        this->width = width;
        this->height = height;        
        if (this->bpp == bpp) {
            screenResized(thread);
        } else {
            this->bpp = bpp;
            DISPATCH_MAIN_THREAD_BLOCK_BEGIN
            displayChanged(thread);
            DISPATCH_MAIN_THREAD_BLOCK_END
        }        
    }
    virtual U32 screenWidth() {
        return width;
    }
    virtual U32 screenHeight() {
        return height;
    }
    virtual U32 screenBpp() {
        return bpp;
    }
    virtual bool getMousePos(int* x, int* y);
    virtual void setMousePos(int x, int y);

    virtual bool setCursor(char* moduleName, char* resourceName, int resource);
    virtual void createAndSetCursor(char* moduleName, char* resourceName, int resource, U8* and_bits, U8* xor_bits, int width, int height, int hotX, int hotY);

    virtual std::shared_ptr<Wnd> getWnd(U32 hwnd);
    virtual std::shared_ptr<Wnd> createWnd(KThread* thread, U32 processId, U32 hwnd, U32 windowRect, U32 clientRect);
    virtual void bltWnd(KThread* thread, U32 hwnd, U32 bits, S32 xOrg, S32 yOrg, U32 width, U32 height, U32 rect);
    virtual void bltWnd(KThread* thread, std::shared_ptr<Wnd> w, U8* bytes, U32 pitch, U32 bpp, U32 width, U32 height);
    virtual void drawAllWindows(KThread* thread, U32 hWnd, int count);
    virtual void drawWindow(std::shared_ptr<Wnd> w);
    virtual void setTitle(const std::string& title);

    virtual U32 getGammaRamp(U32 ramp);

    virtual U32 glCreateContext(KThread* thread, std::shared_ptr<Wnd> wnd, int major, int minor, int profile, int flags);
    virtual void glDeleteContext(KThread* thread, U32 contextId);
    virtual U32 glMakeCurrent(KThread* thread, U32 arg);
    virtual U32 glShareLists(KThread* thread, U32 srcContext, U32 destContext);
    virtual void glSwapBuffers(KThread* thread);
    virtual void glUpdateContextForThread(KThread* thread);
    virtual void preOpenGLCall(U32 index);

    virtual bool partialScreenShot(std::string filepath, U32 x, U32 y, U32 w, U32 h, U32* crc);
    virtual bool screenShot(std::string filepath, U32* crc);    

    virtual bool waitForEvent(U32 ms);
    virtual bool processEvents();

    virtual int mouseMove(int x, int y, bool relative);
    virtual int mouseWheel(int amount, int x, int y);
    virtual int mouseButton(U32 down, U32 button, int x, int y);
    virtual int key(U32 key, U32 down);

    std::shared_ptr<WndSdl> getWndFromPoint(int x, int y);
    std::shared_ptr<WndSdl> getFirstVisibleWnd();
    bool handlSdlEvent(SDL_Event* e);
    void destroyScreen(KThread* thread);
    void preDrawWindow();
    void displayChanged(KThread* thread);
    KThreadGlContext* getGlContextByIdInUnknownThread(const std::shared_ptr<KProcess>& process, U32 id);
    std::shared_ptr<WndSdl> getWndSdl(U32 hwnd);        
    std::string getCursorName(char* moduleName, char* resourceName, int resource);
#ifdef BOXEDWINE_RECORDER
    SDL_Texture* screenCopyTexture;
    virtual void pushWindowSurface();
    virtual void popWindowSurface();
    virtual void drawRectOnPushedSurfaceAndDisplay(U32 x, U32 y, U32 w, U32 h, U8 r, U8 g, U8 b, U8 a);
    virtual void processCustomEvents(std::function<bool(bool isKeyDown, int key, bool isF11)> onKey, std::function<bool(bool isButtonDown, int button, int x, int y)> onMouseButton, std::function<bool(int x, int y)> onMouseMove);
#endif
    bool internalScreenShot(std::string filepath, SDL_Rect* r, U32* crc);
    bool isShutdownWindowIsOpen();
    void updateShutdownWindow();

private:
    void contextCreated();

    int xToScreen(int x);
    int xFromScreen(int x);
    int yToScreen(int y);
    int yFromScreen(int y);

    void checkMousePos(int& x, int& y);
};

static std::shared_ptr<KNativeWindowSdl> screen;

#define HIDE_UI_WINDOW_DELAY 1000

static int rel_mouse_sensitivity = 100;
static bool relativeMouse = false;

static int firstWindowCreated;

bool KNativeWindowSdl::waitForEvent(U32 ms) {
    if (isShutdownWindowIsOpen()) {
        ms = 100;
        updateShutdownWindow();
    }
    return SDL_WaitEventTimeout(NULL, ms) == 1;
}

bool KNativeWindowSdl::processEvents() {
    SDL_Event e;

    if (isShutdownWindowIsOpen()) {
        updateShutdownWindow();
    }
    while (SDL_PollEvent(&e) == 1) {
#ifdef BOXEDWINE_MULTI_THREADED
        if (e.type == sdlCustomEvent) {
            SdlCallback* callback = (SdlCallback*)e.user.data1;
            callback->result = (U32)callback->pfn();
            BOXEDWINE_CONDITION_LOCK(callback->cond);
            BOXEDWINE_CONDITION_SIGNAL(callback->cond);
            BOXEDWINE_CONDITION_UNLOCK(callback->cond);
        } else 
#endif
        
        if (!handlSdlEvent(&e)) {
            return false;
        }
    }
    return true;
}

void KNativeWindow::init(U32 cx, U32 cy, U32 bpp, int scaleX, int scaleY, const std::string& scaleQuality, U32 fullScreen, U32 vsync) {
    if (!sdlCustomEvent) {
        sdlCustomEvent = SDL_RegisterEvents(1);
    }

    screen = std::make_shared<KNativeWindowSdl>();

    KNativeWindow::defaultScreenWidth = cx;
    KNativeWindow::defaultScreenHeight = cy;
    KNativeWindow::defaultScreenBpp = bpp;
    screen->width = cx;
    screen->height = cy;
    screen->bpp = bpp;
    screen->scaleX = scaleX;
    screen->scaleY = scaleY;
    screen->scaleXOffset = 0;
    screen->scaleYOffset = 0;
    screen->scaleQuality = scaleQuality;
    screen->fullScreen = fullScreen;
    screen->vsync = vsync;
}

std::shared_ptr<Wnd> KNativeWindowSdl::getWnd(U32 hwnd) {
    BOXEDWINE_CRITICAL_SECTION_WITH_MUTEX(hwndToWndMutex);
    if (hwndToWnd.count(hwnd))
        return hwndToWnd[hwnd];
    return NULL;
}

std::shared_ptr<WndSdl> KNativeWindowSdl::getWndSdl(U32 hwnd) {
    BOXEDWINE_CRITICAL_SECTION_WITH_MUTEX(hwndToWndMutex);
    if (hwndToWnd.count(hwnd))
        return hwndToWnd[hwnd];
    return NULL;
}

std::shared_ptr<WndSdl> KNativeWindowSdl::getWndFromPoint(int x, int y) {
    BOXEDWINE_CRITICAL_SECTION_WITH_MUTEX(hwndToWndMutex);
    for (auto& n : hwndToWnd) {
        std::shared_ptr<WndSdl> wnd = n.second;
        if (x>=wnd->windowRect.left && x<=wnd->windowRect.right && y>=wnd->windowRect.top && y<=wnd->windowRect.bottom && wnd->surface) {
            return wnd;
        }
    }
    return NULL;
}

std::shared_ptr<WndSdl> KNativeWindowSdl::getFirstVisibleWnd() {
    BOXEDWINE_CRITICAL_SECTION_WITH_MUTEX(hwndToWndMutex);
    for (auto& n : hwndToWnd) {
        std::shared_ptr<WndSdl> wnd = n.second;
        if (wnd->sdlTextureWidth || wnd->openGlContext) {
            return wnd;
        }
    }
    return NULL;
}

static U32 nextGlId = 1;

void KNativeWindowSdl::destroyScreen(KThread* thread) {
#if !defined(BOXEDWINE_DISABLE_UI) && !defined(__TEST) && !defined(BOXEDWINE_MSVC)
    if (uiIsRunning()) {
        uiShutdown();
    }
#endif
    timeToHideUI = 0;
    timeWindowWasCreated = 0;
    windowIsHidden = false;
    delayedCreateWindowMsg = "";
    {
        BOXEDWINE_CRITICAL_SECTION_WITH_MUTEX(hwndToWndMutex);
        for (auto& n : hwndToWnd) {
            std::shared_ptr<WndSdl> wnd = n.second;
            if (wnd->sdlTexture) {
                SDL_DestroyTexture(wnd->sdlTexture);
                wnd->sdlTexture = NULL;
                wnd->sdlTextureHeight = 0;
                wnd->sdlTextureWidth = 0;
            }
        }
    }
    if (!thread) {
        // :TODO: should probably store all context in this file instead of in the threads
        if (currentContext) {
            BoxedwineGL::current->deleteContext(currentContext);
            contextCount=0;
        }
    } else {
        if (contextCount) {
            contextCount++; // prevent it from calling displayChanged
            for (U32 i=1;i<nextGlId;i++) {
                glDeleteContext(thread, i);
            }
            contextCount--;
        }    
    }
    if (contextCount) {
        kwarn("Not all OpenGL contexts were cleanly destroyed");
    }
    if (renderer) {
        SDL_DestroyRenderer(renderer);
        renderer = NULL;
    }
    // :TODO: what about other threads?
    if (thread) {
        thread->removeAllGlContexts();
    }
    if (shutdownRenderer) {
        SDL_DestroyRenderer(shutdownRenderer);
        shutdownRenderer = NULL;
    }
    if (shutdownWindow) {
        SDL_DestroyWindow(shutdownWindow);
        shutdownWindow = NULL;
    }
    if (window) {
        SDL_DestroyWindow(window);
        window = 0;
        windowIsGL = false;
        glWindowVersionMajor = 0;
    }   
    contextCount = 0;
}

void KNativeWindowSdl::preDrawWindow() {    
#if !defined(BOXEDWINE_DISABLE_UI) && !defined(__TEST)
    if (timeToHideUI && timeToHideUI < KSystem::getMilliesSinceStart()) {
        if (uiIsRunning()) {
            DISPATCH_MAIN_THREAD_BLOCK_BEGIN
            if (uiIsRunning()) {
                uiShutdown();
            }
            DISPATCH_MAIN_THREAD_BLOCK_END
        }
        timeToHideUI = 0;
        if (delayedCreateWindowMsg.length()) {
            klog(delayedCreateWindowMsg.c_str());
            delayedCreateWindowMsg = "";
        }
    }
#endif
    if (windowIsHidden) {
        int w = 0;
        int h = 0;
        SDL_GetWindowSize(window, &w, &h);
        if ((w >= 320 && h >= 240) || (timeWindowWasCreated + 2000 < KSystem::getMilliesSinceStart())) {
            DISPATCH_MAIN_THREAD_BLOCK_BEGIN
            SDL_ShowWindow(window);
            SDL_RaiseWindow(window);
            DISPATCH_MAIN_THREAD_BLOCK_END
            windowIsHidden = false;
            timeToHideUI = KSystem::getMilliesSinceStart() + HIDE_UI_WINDOW_DELAY;
        }
    }    
}

#if defined(BOXEDWINE_OPENGL_SDL) || defined(BOXEDWINE_OPENGL_ES)
void loadSdlExtensions();
void initSdlOpenGL();
#endif
#if defined(BOXEDWINE_OPENGL_OSMESA)
void loadMesaExtensions();
void initMesaOpenGL();
void shutdownMesaOpenGL();
#endif

void KNativeWindowSdl::glDeleteContext(KThread* thread, U32 contextId) {
    KThreadGlContext* threadContext = thread->getGlContextById(contextId);
    if (threadContext && threadContext->context) {
        BoxedwineGL::current->deleteContext(threadContext->context);
        thread->removeGlContextById(contextId);
        contextCount--;
        if (contextCount==0) {
            DISPATCH_MAIN_THREAD_BLOCK_BEGIN
            displayChanged(thread);
            DISPATCH_MAIN_THREAD_BLOCK_END
        }
    }
}

void KNativeWindowSdl::glUpdateContextForThread(KThread* thread) {
    if (thread->currentContext && thread->currentContext!=currentContext) {
        BoxedwineGL::current->makeCurrent(thread->currentContext, window);
        currentContext = thread->currentContext;        
    }
}

void printOpenGLInfo();
U32 KNativeWindowSdl::glMakeCurrent(KThread* thread, U32 arg) {
    KThreadGlContext* threadContext = thread->getGlContextById(arg);
    if (threadContext && threadContext->context) {
        if (BoxedwineGL::current->makeCurrent(threadContext->context, window)) {
            threadContext->hasBeenMadeCurrent = true;
            thread->currentContext = threadContext->context;
            currentContext = threadContext->context;
#if defined(BOXEDWINE_OPENGL_OSMESA)
            if (KSystem::openglType == OPENGL_TYPE_OSMESA) {
                loadMesaExtensions();
            } else
#endif
#if defined(BOXEDWINE_OPENGL_SDL) || defined(BOXEDWINE_OPENGL_ES)
            {
                loadSdlExtensions();
            }
#else
            {}
#endif
#ifdef BOXEDWINE_OPENGL
            static bool hasPrintedInfo = false;
            if (!hasPrintedInfo) {
                hasPrintedInfo = true;
                printOpenGLInfo();
            }
#endif
            return 1;
        } else {
            std::string lastError = BoxedwineGL::current->getLastError();
            klog("sdlMakeCurrent failed: %s", lastError.c_str());
        }
    } else if (arg == 0) {
        BoxedwineGL::current->makeCurrent(NULL, window);
        thread->currentContext = 0;
        currentContext = NULL;
        return 1;
    } else {
        kpanic("Tried to make an OpenGL context current for a different thread?");
    }
    return 0;
}

KThreadGlContext* KNativeWindowSdl::getGlContextByIdInUnknownThread(const std::shared_ptr<KProcess>& process, U32 id) {
    KThreadGlContext* result = NULL;

    process->iterateThreads([id, &result] (KThread* thread) {
        result = thread->getGlContextById(id);
        return result==NULL;
    });
    return result;
}

U32 KNativeWindowSdl::glShareLists(KThread* thread, U32 srcContext, U32 destContext) {
    KThreadGlContext* src = getGlContextByIdInUnknownThread(thread->process, srcContext);
    KThreadGlContext* dst = getGlContextByIdInUnknownThread(thread->process, destContext);

    if (BoxedwineGL::current->shareList(src, dst, window)) {
        return 1;
    }
    return 0;
}

#ifdef BOXEDWINE_OPENGL_SDL
U32 sdlCreateOpenglWindow_main_thread(KThread* thread, std::shared_ptr<WndSdl> wnd, int major, int minor, int profile, int flags) {
    DISPATCH_MAIN_THREAD_BLOCK_BEGIN_RETURN
    screen->destroyScreen(thread);

    firstWindowCreated = 1;
    SDL_GL_ResetAttributes();

    SDL_GL_SetAttribute(SDL_GL_RED_SIZE, wnd->pixelFormat->cRedBits);
    SDL_GL_SetAttribute(SDL_GL_GREEN_SIZE, wnd->pixelFormat->cGreenBits);
    SDL_GL_SetAttribute(SDL_GL_BLUE_SIZE, wnd->pixelFormat->cBlueBits);
    SDL_GL_SetAttribute(SDL_GL_ALPHA_SIZE, wnd->pixelFormat->cAlphaBits);

    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, wnd->pixelFormat->cDepthBits);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, wnd->pixelFormat->cStencilBits);

    SDL_GL_SetAttribute(SDL_GL_ACCUM_RED_SIZE, wnd->pixelFormat->cAccumRedBits);
    SDL_GL_SetAttribute(SDL_GL_ACCUM_GREEN_SIZE, wnd->pixelFormat->cAccumGreenBits);
    SDL_GL_SetAttribute(SDL_GL_ACCUM_BLUE_SIZE, wnd->pixelFormat->cAccumBlueBits);
    SDL_GL_SetAttribute(SDL_GL_ACCUM_ALPHA_SIZE, wnd->pixelFormat->cAccumAlphaBits);

    if (major) {
        if (major >= 3) {
            SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
        }
#ifdef BOXEDWINE_MSVC
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, major);
        SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, minor);
#endif
    }
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, (wnd->pixelFormat->dwFlags & K_PFD_DOUBLEBUFFER)?1:0);
    SDL_GL_SetAttribute(SDL_GL_ACCELERATED_VISUAL, (wnd->pixelFormat->dwFlags & K_PFD_GENERIC_FORMAT)?0:1);

    if (wnd->pixelFormat->dwFlags & K_PFD_SWAP_COPY) {
        kwarn("Boxedwine: pixel format swap copy not supported");
    }
    SDL_GL_SetAttribute(SDL_GL_SHARE_WITH_CURRENT_CONTEXT, 0); 
    firstWindowCreated = 1;

    SDL_DisplayMode dm;
    int sdlFlags = SDL_WINDOW_OPENGL;
    if (!KSystem::showWindowImmediately) {
        sdlFlags |= SDL_WINDOW_HIDDEN;
    }
    int cx = wnd->windowRect.right-wnd->windowRect.left;
    int cy = wnd->windowRect.bottom-wnd->windowRect.top;

    if (SDL_GetDesktopDisplayMode(0, &dm) == 0) {
        if (cx == dm.w && cy == dm.h) {
            sdlFlags|=SDL_WINDOW_BORDERLESS;
        }
    }   
    // until I figure out how to scale GL window
    screen->scaleX = 100;
    screen->scaleY = 100;
    screen->scaleXOffset = 0;
    screen->scaleYOffset = 0;
    screen->delayedCreateWindowMsg = "Creating Window for OpenGL: "+std::to_string(cx) + "x" + std::to_string(cy);
    fflush(stdout);
    screen->window = SDL_CreateWindow("OpenGL Window", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, cx, cy, sdlFlags);
    screen->windowIsHidden = !KSystem::showWindowImmediately;
    screen->timeToHideUI = KSystem::getMilliesSinceStart() + HIDE_UI_WINDOW_DELAY;
    screen->timeWindowWasCreated = KSystem::getMilliesSinceStart();

    if (!screen->window) {
        kwarn("Couldn't create window: %s", SDL_GetError());
        screen->displayChanged(thread);
        return 0;
    }
    screen->windowIsGL = true;
    screen->glWindowVersionMajor = major;
    return thread->id;
    DISPATCH_MAIN_THREAD_BLOCK_END
}
#endif

#include "../../tools/opengl/gldef.h"
void KNativeWindowSdl::preOpenGLCall(U32 index) {
    // The Breakdown requires this extra time check, I'm not sure what call it uses to actually draw on the screen
    if (index == XSwapBuffer || index == Finish || index == Flush || (screen->windowIsHidden && screen->timeWindowWasCreated + 1000 < KSystem::getMilliesSinceStart())) {
        screen->preDrawWindow();
    }
}

void KNativeWindowSdl::contextCreated() {
    BoxedwineGL::current->setSwapInterval(this->vsync);
}

// window needs to be on the main thread
// context needs to be on the current thread
U32 KNativeWindowSdl::glCreateContext(KThread* thread, std::shared_ptr<Wnd> w, int major, int minor, int profile, int flags) {
#if defined(BOXEDWINE_OPENGL_OSMESA)
    if (KSystem::openglType == OPENGL_TYPE_OSMESA) {
        initMesaOpenGL();
    }
    else
#endif
#if defined(BOXEDWINE_OPENGL_SDL) || defined(BOXEDWINE_OPENGL_ES)
    {
        initSdlOpenGL();
    }
#else
    {}
#endif
    U32 result = 1;
    std::shared_ptr<WndSdl> wnd = std::dynamic_pointer_cast<WndSdl>(w);
    if (windowIsGL && glWindowVersionMajor != major && KSystem::openglType != OPENGL_TYPE_OSMESA) {
        DISPATCH_MAIN_THREAD_BLOCK_BEGIN_RETURN
        screen->destroyScreen(thread);
        DISPATCH_MAIN_THREAD_BLOCK_END
    }
#ifdef BOXEDWINE_OPENGL_SDL
    if (!windowIsGL && KSystem::openglType != OPENGL_TYPE_OSMESA) {
        result = sdlCreateOpenglWindow_main_thread(thread, wnd, major, minor, profile, flags);
    }
#endif
    if (result) {
        void* context;
        int cx = wnd->windowRect.right - wnd->windowRect.left;
        int cy = wnd->windowRect.bottom - wnd->windowRect.top;

        // Mac requires this on the main thread, but Windows make current will fail if its not on the same thread as create context
#ifdef BOXEDWINE_MSVC
        context = BoxedwineGL::current->createContext(window, wnd, wnd->pixelFormat, cx, cy, major, minor, profile);
#else
        DISPATCH_MAIN_THREAD_BLOCK_BEGIN_WITH_ARG(&context COMMA this COMMA wnd COMMA cx COMMA cy COMMA major COMMA minor COMMA profile)
        context = BoxedwineGL::current->createContext(window, wnd, wnd->pixelFormat, cx, cy, major, minor, profile);
        DISPATCH_MAIN_THREAD_BLOCK_END
#endif
        if (!context) {
            std::string lastError = BoxedwineGL::current->getLastError();
            kwarn("Couldn't create context: %s", lastError.c_str());
            DISPATCH_MAIN_THREAD_BLOCK_BEGIN_RETURN
            displayChanged(thread);
            return 0;
            DISPATCH_MAIN_THREAD_BLOCK_END
        }
        contextCreated();
        result = nextGlId;
        thread->addGlContext(nextGlId++, context);
        contextCount++;
        if (!wnd->openGlContext)
            wnd->openGlContext = context;       
    }
    return result;
}

void KNativeWindowSdl::screenResized(KThread* thread) {
    DISPATCH_MAIN_THREAD_BLOCK_BEGIN
        if (contextCount) {
            int cx = screenWidth();
            int cy = screenHeight();            

            if (fullScreen != FULLSCREEN_NOTSET) {
                SDL_DisplayMode dm;
                if (SDL_GetDesktopDisplayMode(0, &dm) != 0)
                {
                    if (fullScreen == FULLSCREEN_STRETCH) {
                        scaleX = dm.w * 100 / cx;
                        scaleY = dm.h * 100 / cy;
                    } else if (fullScreen == FULLSCREEN_ASPECT) {
                        scaleX = dm.w * 100 / cx;
                        scaleY = dm.h * 100 / cy;
                        scaleXOffset = 0;
                        scaleYOffset = 0;
                        if (scaleY > scaleX) {
                            scaleY = scaleX;
                            scaleYOffset = (dm.h - cy * scaleY / 100) / 2;
                        } else if (scaleX > scaleY) {
                            scaleX = scaleY;
                            scaleXOffset = (dm.w - cx * scaleX / 100) / 2;
                        }
                    }
                    sdlDesktopHeight = dm.h;
                    sdlDesktopWidth = dm.w;
                }
            } else {
                if (scaleX != 100) {
                    cx = cx * scaleX / 100;
                    cy = cy * scaleY / 100;
                }
                SDL_SetWindowSize(window, cx, cy);
            }            
        } else {
            displayChanged(thread);
        }
    DISPATCH_MAIN_THREAD_BLOCK_END
}

void KNativeWindowSdl::displayChanged(KThread* thread) {
    BOXEDWINE_CRITICAL_SECTION_WITH_MUTEX(sdlMutex);
    firstWindowCreated = 1;
    if (contextCount) {
        // when the context is destroy, displayChanged will be called again
        return;
    }
    if (KSystem::videoEnabled) {       
        destroyScreen(thread);
        {
            BOXEDWINE_CRITICAL_SECTION_WITH_MUTEX(hwndToWndMutex);
            for (auto& n : hwndToWnd) {
                std::shared_ptr<WndSdl> wnd = n.second;
                wnd->openGlContext = 0;
            }
        }
        SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, scaleQuality.c_str());

        int cx = width*scaleX/100;
        int cy = height*scaleY/100;
        int flags = 0;

        if (!KSystem::showWindowImmediately) {
            flags |= SDL_WINDOW_HIDDEN;
        }

        SDL_DisplayMode dm;

        if (SDL_GetDesktopDisplayMode(0, &dm) != 0)
        {
            SDL_Log("SDL_GetDesktopDisplayMode failed: %s", SDL_GetError());
            fullScreen = FULLSCREEN_NOTSET;
        } else {
            sdlDesktopHeight = dm.h;
            sdlDesktopWidth = dm.w;
            if (fullScreen == FULLSCREEN_STRETCH) {
                cx = dm.w;
                cy = dm.h;
                flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
                scaleX = dm.w * 100 / width;
                scaleY = dm.h * 100 / height;
            } else if (fullScreen == FULLSCREEN_ASPECT) {
                cx = dm.w;
                cy = dm.h;
                flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
                scaleX = dm.w * 100 / width;
                scaleY = dm.h * 100 / height;
                scaleXOffset = 0;
                scaleYOffset = 0;
                if (scaleY > scaleX) {
                    scaleY = scaleX;
                    scaleYOffset = (dm.h - height * scaleY / 100) / 2;
                } else if (scaleX > scaleY) {
                    scaleX = scaleY;
                    scaleXOffset = (dm.w - width * scaleX / 100) / 2;
                }
            } else if (width == (U32)dm.w && height == (U32)dm.h) {
                flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
            }   
            if (cx > dm.w || cy > dm.h) {
                cx = dm.w;
                cy = dm.h;
                scaleX = dm.w * 100 / width;
                scaleY = dm.h * 100 / height;
                flags |= SDL_WINDOW_FULLSCREEN_DESKTOP;
            }
        }
        
        delayedCreateWindowMsg = "Creating Window: " + std::to_string(cx) + "x" + std::to_string(cy);
        fflush(stdout);
        window = SDL_CreateWindow("BoxedWine", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, cx, cy, flags);
        flags = 0;
        if (this->vsync != VSYNC_DISABLED) {
            flags |= SDL_RENDERER_PRESENTVSYNC;
        }
        renderer = SDL_CreateRenderer(window, -1, flags);
        windowIsHidden = !KSystem::showWindowImmediately;
        timeWindowWasCreated = KSystem::getMilliesSinceStart();
        windowIsGL = false;
    }
}

void KNativeWindowSdl::glSwapBuffers(KThread* thread) {
    preOpenGLCall(XSwapBuffer);
    BoxedwineGL::current->swapBuffer(window);
}

#if defined(BOXEDWINE_RECORDER) || ! defined(BOXEDWINE_64BIT_MMU)
static S8 sdlBuffer[1024*1024*4];
#endif

void KNativeWindowSdl::bltWnd(KThread* thread, U32 hwnd, U32 bits, S32 xOrg, S32 yOrg, U32 width, U32 height, U32 rect) {
    if (!firstWindowCreated) {
        DISPATCH_MAIN_THREAD_BLOCK_BEGIN
        displayChanged(thread);
        DISPATCH_MAIN_THREAD_BLOCK_END
    }
    
    BOXEDWINE_CRITICAL_SECTION_WITH_MUTEX(sdlMutex);
    std::shared_ptr<WndSdl> wnd = getWndSdl(hwnd);
    wRECT r;
    int bpp = screenBpp()==8?32:screenBpp();
    int pitch = (width*((bpp+7)/8)+3) & ~3;

    if (!renderer) {
        // final reality will draw its main start window while an OpenGL context is still going
        // half life uplink demo intro movie also needs this
        if (contextCount && !wnd->openGlContext) {
            BOXEDWINE_MUTEX_UNLOCK(sdlMutex);
            DISPATCH_MAIN_THREAD_BLOCK_BEGIN
            renderer = SDL_CreateRenderer(window, -1, 0);	
            DISPATCH_MAIN_THREAD_BLOCK_END
            BOXEDWINE_MUTEX_LOCK(sdlMutex);
            wnd = getWndSdl(hwnd); // just in case it changed when we gave up the lock
        }
    }
    preDrawWindow();
    r.readRect(rect);    
    if (wnd)
    {
        SDL_Texture *sdlTexture = NULL;
        
        if (wnd->sdlTexture) {
            sdlTexture = (SDL_Texture*)wnd->sdlTexture;
            if (sdlTexture && (((U32)wnd->sdlTextureHeight) != height || ((U32)wnd->sdlTextureWidth) != width)) {
                SDL_DestroyTexture(sdlTexture);
                wnd->sdlTexture = NULL;
                sdlTexture = NULL;
            }
        }
        if (!sdlTexture) {
            U32 format = SDL_PIXELFORMAT_ARGB8888;
            if (bpp == 16) {
                format = SDL_PIXELFORMAT_RGB565;
            } else if (bpp == 15) {
                format = SDL_PIXELFORMAT_RGB555;
            }
            if (KSystem::videoEnabled && renderer) {
                sdlTexture = SDL_CreateTexture(renderer, format, SDL_TEXTUREACCESS_STREAMING, width, height);
                wnd->sdlTexture = sdlTexture;
            }
            wnd->sdlTextureHeight = height;
            wnd->sdlTextureWidth = width;
        }
        if (!thread->memory->isValidReadAddress(bits, height*pitch)) {
            return;
        }
#ifndef BOXEDWINE_64BIT_MMU        
        for (U32 y = 0; y < height; y++) {
            memcopyToNative(bits+(height-y-1)*pitch, sdlBuffer+y*pitch, pitch);
        } 
#endif
        if (screenBpp()!=32) {
            // SDL_ConvertPixels(width, height, )
        }
#ifdef BOXEDWINE_RECORDER
        if (Recorder::instance || Player::instance) {
            U32 toCopy = pitch*height;
            if (wnd->bitsSize<toCopy) {
                if (wnd->bits) {
                    delete[] wnd->bits;
                }
                wnd->bits = new U8[toCopy];
                wnd->bitsSize = toCopy;
            }
#ifdef BOXEDWINE_64BIT_MMU
            for (U32 y = 0; y < height; y++) {
                memcopyToNative(bits+(height-y-1)*pitch, sdlBuffer+y*pitch, pitch);
            } 
#endif
            memcpy(wnd->bits, sdlBuffer, toCopy);
        }
#endif        
        if (KSystem::videoEnabled && renderer) {
#ifdef BOXEDWINE_64BIT_MMU
            SDL_UpdateTexture(sdlTexture, NULL, getNativeAddress(KThread::currentThread()->process->memory, bits), pitch);
#else
            SDL_UpdateTexture(sdlTexture, NULL, sdlBuffer, pitch);
#endif
        }
    }
}

void KNativeWindowSdl::bltWnd(KThread* thread, std::shared_ptr<Wnd> w, U8* bytes, U32 pitch, U32 bpp, U32 width, U32 height) {
    if (!firstWindowCreated) {
        DISPATCH_MAIN_THREAD_BLOCK_BEGIN
            displayChanged(thread);
        DISPATCH_MAIN_THREAD_BLOCK_END
    }

    BOXEDWINE_CRITICAL_SECTION_WITH_MUTEX(sdlMutex);
    std::shared_ptr<WndSdl> wnd = std::dynamic_pointer_cast<WndSdl>(w);

    preDrawWindow();
    SDL_Texture* sdlTexture = NULL;

    if (wnd->sdlTexture) {
        sdlTexture = (SDL_Texture*)wnd->sdlTexture;
        if (sdlTexture && (((U32)wnd->sdlTextureHeight) != height || ((U32)wnd->sdlTextureWidth) != width)) {
            SDL_DestroyTexture(sdlTexture);
            wnd->sdlTexture = NULL;
            sdlTexture = NULL;
        }
    }
    if (!sdlTexture) {
        U32 format = SDL_PIXELFORMAT_ARGB8888;
        if (bpp == 16) {
            format = SDL_PIXELFORMAT_RGB565;
        }
        else if (bpp == 15) {
            format = SDL_PIXELFORMAT_RGB555;
        }
        if (KSystem::videoEnabled && renderer) {
            sdlTexture = SDL_CreateTexture(renderer, format, SDL_TEXTUREACCESS_STREAMING, width, height);
            wnd->sdlTexture = sdlTexture;
        }
        wnd->sdlTextureHeight = height;
        wnd->sdlTextureWidth = width;
    }
#ifdef BOXEDWINE_RECORDER
    if (Recorder::instance || Player::instance) {
        U32 toCopy = pitch * height;
        if (wnd->bitsSize < toCopy) {
            if (wnd->bits) {
                delete[] wnd->bits;
            }
            wnd->bits = new U8[toCopy];
            wnd->bitsSize = toCopy;
        }
        memcpy(wnd->bits, bytes, toCopy);
    }
#endif        
    if (KSystem::videoEnabled && renderer) {
        SDL_UpdateTexture(sdlTexture, NULL, bytes, pitch);
    }
}

void KNativeWindowSdl::setTitle(const std::string& title) {
    if (window)
        SDL_SetWindowTitle(window, title.c_str());
}

#ifdef BOXEDWINE_RECORDER
U8* recorderBuffer;
U32 recorderBufferSize;
#endif

void KNativeWindowSdl::drawWindow(std::shared_ptr<Wnd> w) {
    if (KSystem::videoEnabled && renderer) {        
        std::shared_ptr<WndSdl> wnd = std::dynamic_pointer_cast<WndSdl>(w);
        DISPATCH_MAIN_THREAD_BLOCK_BEGIN
        BOXEDWINE_CRITICAL_SECTION_WITH_MUTEX(sdlMutex);
        SDL_SetRenderDrawColor(renderer, 58, 110, 165, 255);
        SDL_RenderClear(renderer);
        if (wnd && wnd->sdlTextureWidth && wnd->sdlTexture) {
            SDL_Rect dstrect;
            dstrect.x = wnd->windowRect.left * (int)scaleX / 100 + scaleXOffset;
            dstrect.y = wnd->windowRect.top * (int)scaleY / 100 + scaleYOffset;
            dstrect.w = wnd->sdlTextureWidth * (int)scaleX / 100;
            dstrect.h = wnd->sdlTextureHeight * (int)scaleY / 100;
            SDL_RenderCopy(renderer, wnd->sdlTexture, NULL, &dstrect);
        }
        if (scaleXOffset) {
            SDL_Rect rect;
            rect.x = 0;
            rect.w = scaleXOffset;
            rect.y = 0;
            rect.h = sdlDesktopHeight;
            SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
            SDL_RenderFillRect(renderer, &rect);
            rect.x = sdlDesktopWidth - scaleXOffset;
            SDL_RenderFillRect(renderer, &rect);
        }
        SDL_RenderPresent(renderer);
        DISPATCH_MAIN_THREAD_BLOCK_END
    }
}

void KNativeWindowSdl::drawAllWindows(KThread* thread, U32 hWnd, int count) {
    if (KSystem::skipFrameFPS) {
        static U64 lastUpdate=0;
        static U64 diff = 100000 / KSystem::skipFrameFPS;
        U64 now = KSystem::getMicroCounter();
        if (now - lastUpdate < diff) {
            return;
        }
        lastUpdate = now;
    }
    BOXEDWINE_CRITICAL_SECTION_WITH_MUTEX(sdlMutex);
    if (KSystem::videoEnabled && (!renderer || (contextCount && lastGlCallTime+1000> KSystem::getMilliesSinceStart()))) {
        // don't let window drawing and opengl drawing fight and clobber each other, if OpenGL was active in the last second, then don't draw the window
        return;
    }
#ifdef BOXEDWINE_RECORDER
    if (Recorder::instance || Player::instance) {
        int bpp = screenBpp()==8?32:screenBpp();
        S32 bytesPerPixel = (bpp+7)/8;
        S32 recorderPitch = (width*((bpp+7)/8)+3) & ~3;
        if (recorderPitch*height>recorderBufferSize) {
            if (recorderBuffer) {
                delete[] recorderBuffer;
            }
            recorderBuffer = new U8[recorderPitch*height];
            recorderBufferSize = recorderPitch*height;
        }
        if (bpp==32) {
            U32* pixel = (U32*)recorderBuffer;
            for (U32 i=0;i<height*width;i++, pixel++) {
                *pixel = 165 | (110 << 8) | (58 << 16) | (255 << 24);
            }
        } else {
            memset(recorderBuffer, 0, recorderBufferSize);
        }        
        for (int i=count-1;i>=0;i--) {
            std::shared_ptr<WndSdl> wnd = getWndSdl(readd(hWnd+i*4));
            if (wnd && wnd->sdlTextureWidth) {
                int width = wnd->sdlTextureWidth;
                int height = wnd->sdlTextureHeight;
                S32 top = wnd->windowRect.top;
                S32 left = wnd->windowRect.left;
                S32 srcTopAdjust = 0;
                S32 srcLeftAdjust = 0;

                if (top<0) {
                    srcTopAdjust = -top;
                    top = 0;
                }
                if (top>=(S32)screenHeight())
                    continue;
                if (left>=(S32)screenWidth())
                    continue;
                if (left<0) {
                    srcLeftAdjust = -left;
                    left = 0;
                }
                if (top+height>(S32)screenHeight())
                    height = screenHeight()-top;
                int pitch = (width*((bpp+7)/8)+3) & ~3;
                if (left+width>(S32)screenWidth())
                    width = screenWidth() - left;       
                int copyPitch = (width*((bpp+7)/8)+3) & ~3;
                for (int y=0;y<height;y++) {
                    S32 offset = recorderPitch*(y+top)+(left*bytesPerPixel);
                    if (offset<0 || offset+copyPitch>(S32)recorderBufferSize || copyPitch<0) {
                        kpanic("script recorder overwrote memory when copying screen");
                    }
                    memcpy(recorderBuffer+offset, wnd->bits+pitch*(y+srcTopAdjust)+(srcLeftAdjust*bytesPerPixel), copyPitch);
                }
            }        	
        }
    }
#endif
    if (KSystem::videoEnabled && renderer) {
        U32 threadId = KThread::currentThread()->id;
        DISPATCH_MAIN_THREAD_BLOCK_BEGIN
        KThread* thread = KSystem::getThreadById(threadId);
        if (thread) {
            ChangeThread t(thread);
            SDL_SetRenderDrawColor(renderer, 58, 110, 165, 255 );
            SDL_RenderClear(renderer);
            for (int i=count-1;i>=0;i--) {
                std::shared_ptr<WndSdl> wnd = getWndSdl(readd(hWnd+i*4));
                if (wnd && wnd->sdlTextureWidth && wnd->sdlTexture) {
                    SDL_Rect dstrect;
                    dstrect.x = wnd->windowRect.left*(int)scaleX/100 + scaleXOffset;
                    dstrect.y = wnd->windowRect.top*(int)scaleY/100 + scaleYOffset;
                    dstrect.w = wnd->sdlTextureWidth*(int)scaleX/100;
                    dstrect.h = wnd->sdlTextureHeight*(int)scaleY/100;
#ifdef BOXEDWINE_64BIT_MMU
                    SDL_RenderCopyEx(renderer, wnd->sdlTexture, NULL, &dstrect, 0, NULL, SDL_FLIP_VERTICAL);
#else
                    SDL_RenderCopy(renderer, wnd->sdlTexture, NULL, &dstrect);
#endif
                }
            }
            if (scaleXOffset) {                
                SDL_Rect rect;
                rect.x = 0;
                rect.w = scaleXOffset;
                rect.y = 0;
                rect.h = sdlDesktopHeight;
                SDL_SetRenderDrawColor(renderer, 0, 0, 0, 255);
                SDL_RenderFillRect(renderer, &rect);
                rect.x = sdlDesktopWidth - scaleXOffset;
                SDL_RenderFillRect(renderer, &rect);
            }
        }
        SDL_RenderPresent(renderer);
        DISPATCH_MAIN_THREAD_BLOCK_END
    }
    KNativeWindow::windowUpdated = true;
}

std::shared_ptr<Wnd> KNativeWindowSdl::createWnd(KThread* thread, U32 processId, U32 hwnd, U32 windowRect, U32 clientRect) {
    std::shared_ptr<WndSdl> wnd = std::make_shared<WndSdl>();
    wnd->windowRect.readRect(windowRect);
    wnd->clientRect.readRect(clientRect);
    wnd->processId = processId;
    wnd->hwnd = hwnd;
    BOXEDWINE_CRITICAL_SECTION_WITH_MUTEX(hwndToWndMutex);
    hwndToWnd[hwnd] = wnd;
    return wnd;
}

void WndSdl::destroy() {
    BOXEDWINE_CRITICAL_SECTION_WITH_MUTEX(screen->hwndToWndMutex);
    screen->hwndToWnd.erase(hwnd);
}

void WndSdl::show(bool bShow) {
    if (!bShow) {
        if (this->sdlTexture) {
            SDL_DestroyTexture(this->sdlTexture);
            this->sdlTexture = NULL;
        }
        this->sdlTextureHeight = 0;
        this->sdlTextureWidth = 0;
    }
}

U32 KNativeWindowSdl::getGammaRamp(U32 ramp) {
    U16 r[256];
    U16 g[256];
    U16 b[256];

    if (KSystem::videoEnabled) {
        if (SDL_GetWindowGammaRamp(window, r, g, b)==0) {
            int i;
            for (i=0;i<256;i++) {
                writew(ramp+i*2, r[i]);
                writew(ramp+i*2+512, g[i]);
                writew(ramp+i*2+1024, b[i]);
            }
            return 1;
        }
    }
    return 0;
}

/*
typedef struct tagMOUSEINPUT
{
    LONG    dx;
    LONG    dy;
    DWORD   mouseData;
    DWORD   dwFlags;
    DWORD   time;
    ULONG_PTR dwExtraInfo;
} MOUSEINPUT, *PMOUSEINPUT, *LPMOUSEINPUT;

typedef struct tagKEYBDINPUT
{
    WORD    wVk;
    WORD    wScan;
    DWORD   dwFlags;
    DWORD   time;
    ULONG_PTR dwExtraInfo;
} KEYBDINPUT, *PKEYBDINPUT, *LPKEYBDINPUT;

typedef struct tagHARDWAREINPUT
{
    DWORD   uMsg;
    WORD    wParamL;
    WORD    wParamH;
} HARDWAREINPUT, *PHARDWAREINPUT, *LPHARDWAREINPUT;

#define INPUT_MOUSE     0
#define INPUT_KEYBOARD  1
#define INPUT_HARDWARE  2

typedef struct tagINPUT
{
    DWORD type;
    union
    {
        MOUSEINPUT      mi;
        KEYBDINPUT      ki;
        HARDWAREINPUT   hi;
    } DUMMYUNIONNAME;
} INPUT, *PINPUT, *LPINPUT;
*/

#define MOUSEEVENTF_MOVE            0x0001
#define MOUSEEVENTF_LEFTDOWN        0x0002
#define MOUSEEVENTF_LEFTUP          0x0004
#define MOUSEEVENTF_RIGHTDOWN       0x0008
#define MOUSEEVENTF_RIGHTUP         0x0010
#define MOUSEEVENTF_MIDDLEDOWN      0x0020
#define MOUSEEVENTF_MIDDLEUP        0x0040
#define MOUSEEVENTF_XDOWN           0x0080
#define MOUSEEVENTF_XUP             0x0100
#define MOUSEEVENTF_WHEEL           0x0800
#define MOUSEEVENTF_HWHEEL          0x1000
#define MOUSEEVENTF_MOVE_NOCOALESCE 0x2000
#define MOUSEEVENTF_VIRTUALDESK     0x4000
#define MOUSEEVENTF_ABSOLUTE        0x8000

void writeLittleEndian_4(U8* buffer, U32 value) {
    buffer[0] = (U8)value;
    buffer[1] = (U8)(value >> 8);
    buffer[2] = (U8)(value >> 16);
    buffer[3] = (U8)(value >> 24);
}

void writeLittleEndian_2(U8* buffer, U16 value) {
    buffer[0] = (U8)value;
    buffer[1] = (U8)(value >> 8);
}

static int lastX;
static int lastY;

int KNativeWindowSdl::xToScreen(int x) {
    return x * (int)scaleX / 100 + (int)scaleXOffset;
}

int KNativeWindowSdl::xFromScreen(int x) {
    return (x - (int)scaleXOffset) * 100 / (int)scaleX;
}

int KNativeWindowSdl::yToScreen(int y) {
    return y * (int)scaleY / 100 + (int)scaleYOffset;
}

int KNativeWindowSdl::yFromScreen(int y) {
    return (y - (int)scaleYOffset) * 100 / (int)scaleY;
}

void KNativeWindowSdl::checkMousePos(int& x, int& y) {
    bool warp = false;
    if (x < 0) {
        x = 0;
        warp = true;
    }
    if (x >= (int)width) {
        x = (int)width - 1;
        warp = true;
    }
    if (y < 0) {
        y = 0;
        warp = true;
    }
    if (y >= (int)height) {
        y = (int)height;
        warp = true;
    }
    if (warp) {
        int scaledX = xToScreen(x);
        int scaledY = yToScreen(y);
        SDL_WarpMouseInWindow(window, scaledX, scaledY);
    }
}

void KNativeWindowSdl::setMousePos(int x, int y) {
    x = xToScreen(x);
    y = yToScreen(y);
    DISPATCH_MAIN_THREAD_BLOCK_BEGIN
    SDL_WarpMouseInWindow(window, x, y);
    DISPATCH_MAIN_THREAD_BLOCK_END
}

int KNativeWindowSdl::mouseMove(int x, int y, bool relative) {
    BOXEDWINE_CRITICAL_SECTION_WITH_MUTEX(hwndToWndMutex);

    x = xFromScreen(x);
    y = yFromScreen(y);
    
    checkMousePos(x, y);

    lastX = x;
    lastY = y;

    if (!hwndToWnd.size())
        return 0;
    std::shared_ptr<WndSdl> wnd = getWndFromPoint(x, y);
    if (!wnd)
        wnd = getFirstVisibleWnd();
    if (wnd) {
        std::shared_ptr<KProcess> process = KSystem::getProcess(wnd->processId);
        if (process) {
            KFileDescriptor* fd = process->getFileDescriptor(process->eventQueueFD);
            if (fd) {
                U8 buffer[28];

                writeLittleEndian_4(buffer, 0); // INPUT_MOUSE
                writeLittleEndian_4(buffer+4, x); // dx
                writeLittleEndian_4(buffer+8, y); // dy
                writeLittleEndian_4(buffer+12, 0); // mouseData
                writeLittleEndian_4(buffer+16,  (relative?MOUSEEVENTF_MOVE:MOUSEEVENTF_MOVE|MOUSEEVENTF_ABSOLUTE)); // dwFlags
                writeLittleEndian_4(buffer+20, KSystem::getMilliesSinceStart()); // time
                writeLittleEndian_4(buffer+24, 0); // dwExtraInfo

                KUnixSocketObject::unixsocket_write_native_nowait(fd->kobject, buffer, 28);
            }
        }
    }
    return 1;
}

int KNativeWindowSdl::mouseWheel(int amount, int x, int y) {
    BOXEDWINE_CRITICAL_SECTION_WITH_MUTEX(hwndToWndMutex);

    if (!hwndToWnd.size())
        return 0;

    x = xFromScreen(x);
    y = yFromScreen(y);

    checkMousePos(x, y);

    std::shared_ptr<WndSdl> wnd = getWndFromPoint(x, y);
    if (!wnd)
        wnd = getFirstVisibleWnd();
    if (wnd) {
        std::shared_ptr<KProcess> process = KSystem::getProcess(wnd->processId);
        if (process) {
            KFileDescriptor* fd = process->getFileDescriptor(process->eventQueueFD);
            if (fd) {
                U8 buffer[28];

                writeLittleEndian_4(buffer, 0); // INPUT_MOUSE
                writeLittleEndian_4(buffer+4, x); // dx
                writeLittleEndian_4(buffer+8, y); // dy
                writeLittleEndian_4(buffer+12, amount); // mouseData
                writeLittleEndian_4(buffer+16, MOUSEEVENTF_WHEEL | MOUSEEVENTF_ABSOLUTE | MOUSEEVENTF_MOVE); // dwFlags
                writeLittleEndian_4(buffer+20, KSystem::getMilliesSinceStart()); // time
                writeLittleEndian_4(buffer+24, 0); // dwExtraInfo

                KUnixSocketObject::unixsocket_write_native_nowait(fd->kobject, buffer, 28);
            }
        }
    }
    return 1;
}

int KNativeWindowSdl::mouseButton(U32 down, U32 button, int x, int y) {
    BOXEDWINE_CRITICAL_SECTION_WITH_MUTEX(hwndToWndMutex);
    
    if (!hwndToWnd.size())
        return 0;

    x = xFromScreen(x);
    y = yFromScreen(y);

    checkMousePos(x, y);

    std::shared_ptr<WndSdl> wnd = getWndFromPoint(x, y);
    if (!wnd)
        wnd = getFirstVisibleWnd();
    if (wnd) {
        std::shared_ptr<KProcess> process = KSystem::getProcess(wnd->processId);
        if (process) {
            KFileDescriptor* fd = process->getFileDescriptor(process->eventQueueFD);
            if (fd) {
                U32 flags = MOUSEEVENTF_MOVE|MOUSEEVENTF_ABSOLUTE;
                U8 buffer[28];

                if (down) {
                    switch (button) {
                        case 0: flags |= MOUSEEVENTF_LEFTDOWN; break;
                        case 1: flags |= MOUSEEVENTF_RIGHTDOWN; break;
                        case 2: flags |= MOUSEEVENTF_MIDDLEDOWN; break;
                    }
                } else {
                    switch (button) {
                        case 0: flags |= MOUSEEVENTF_LEFTUP; break;
                        case 1: flags |= MOUSEEVENTF_RIGHTUP; break;
                        case 2: flags |= MOUSEEVENTF_MIDDLEUP; break;
                    }
                }
                writeLittleEndian_4(buffer, 0); // INPUT_MOUSE
                writeLittleEndian_4(buffer+4, x); // dx
                writeLittleEndian_4(buffer+8, y); // dy
                writeLittleEndian_4(buffer+12, 0); // mouseData
                writeLittleEndian_4(buffer+16, flags); // dwFlags
                writeLittleEndian_4(buffer+20, KSystem::getMilliesSinceStart()); // time
                writeLittleEndian_4(buffer+24, 0); // dwExtraInfo

                KUnixSocketObject::unixsocket_write_native_nowait(fd->kobject, buffer, 28);
            }
        }
    }
    return 1;
}

std::string KNativeWindowSdl::getCursorName(char* moduleName, char* resourceName, int resource) {
    std::string result;
    if (moduleName) {
        result += moduleName;
    }
    result += ";";
    if (resourceName && strlen(resourceName)) {
        result += resourceName;
    } else {
        result += toHexString(resource);
    }
    return result;
}

bool KNativeWindowSdl::setCursor(char* moduleName, char* resourceName, int resource) {
    if (!moduleName && !resourceName && !resource) {
        DISPATCH_MAIN_THREAD_BLOCK_BEGIN
        SDL_ShowCursor(0);
        DISPATCH_MAIN_THREAD_BLOCK_END
        return 1;
    } else {
        std::string name = getCursorName(moduleName, resourceName, resource);
        if (cursors.count(name) && !relativeMouse) {
            SDL_Cursor* cursor = cursors[name];
            if (!cursor)
                return 0;
            DISPATCH_MAIN_THREAD_BLOCK_BEGIN
            SDL_ShowCursor(1);
            SDL_SetCursor(cursor);
            DISPATCH_MAIN_THREAD_BLOCK_END
            return 1;
        }        
    }
    return 0;
}

void KNativeWindowSdl::createAndSetCursor(char* moduleName, char* resourceName, int resource, U8* and_bits, U8* xor_bits, int width, int height, int hotX, int hotY) {
    //int byteCount = (width+31) / 31 * 4 * height;
    int dst,src,y, x;
    U8 data_bits[64*64/8];
    U8 mask_bits[64*64/8];
    U32 srcPitch = (width+31)/32*4;
    U32 dstPitch = (width+7)/8;

    // AND | XOR | Windows cursor pixel | SDL
    // --------------------------------------
    // 0  |  0  | black                 | transparent
    // 0  |  1  | white                 | White
    // 1  |  0  | transparent           | Inverted color if possible, black if not
    // 1  |  1  | invert                | Black

    // 0 0 -> 1 1
    // 0 1 -> 0 1
    // 1 0 -> 0 0
    // 1 1 -> 1 0
    for (y=0;y<height;y++) {
        dst = dstPitch*y;
        src = srcPitch*y;

        for (x=0;x<(width+7)/8;src++,dst++,x++) {
            int j;

            data_bits[dst] = 0;
            mask_bits[dst] = 0;
            for (j=0;j<8;j++) {
                U8 aBit = (and_bits[src] >> j) & 0x1;
                U8 xBit = (xor_bits[src] >> j) & 0x1;

                if (aBit && xBit) {
                    xBit = 0;
                } else if (aBit && !xBit) {
                    aBit = 0;
                } else if (!aBit && xBit) {
                
                } else if (!aBit && !xBit) {
                    aBit = 1;
                    xBit = 1;
                }
                if (aBit)
                    data_bits[dst] |= (1 << j);

                if (xBit)
                    mask_bits[dst] |= (1 << j);
            }
        }
    }

    DISPATCH_MAIN_THREAD_BLOCK_BEGIN
    SDL_Cursor* cursor = SDL_CreateCursor(data_bits, mask_bits, width, height, hotX, hotY);
    if (cursor) {
        std::string name = getCursorName(moduleName, resourceName, resource);
        cursors[name] = cursor;
        SDL_SetCursor(cursor);
    }
    DISPATCH_MAIN_THREAD_BLOCK_END
}

#define SDLK_NUMLOCK SDL_SCANCODE_NUMLOCKCLEAR
#define SDLK_SCROLLOCK SDLK_SCROLLLOCK
#define SDLK_KP0 SDLK_KP_0
#define SDLK_KP1 SDLK_KP_1
#define SDLK_KP2 SDLK_KP_2
#define SDLK_KP3 SDLK_KP_3
#define SDLK_KP4 SDLK_KP_4
#define SDLK_KP5 SDLK_KP_5
#define SDLK_KP6 SDLK_KP_6
#define SDLK_KP7 SDLK_KP_7
#define SDLK_KP8 SDLK_KP_8
#define SDLK_KP9 SDLK_KP_9

int KNativeWindowSdl::key(U32 key, U32 down) {
    BOXEDWINE_CRITICAL_SECTION_WITH_MUTEX(hwndToWndMutex);
    
    if (!hwndToWnd.size())
        return 0;
    std::shared_ptr<WndSdl> wnd = getFirstVisibleWnd();
    if (wnd) {
        std::shared_ptr<KProcess> process = KSystem::getProcess(wnd->processId);
        if (process) {
            KFileDescriptor* fd = process->getFileDescriptor(process->eventQueueFD);
            if (fd) {
                U16 vKey = 0;
                U16 scan = 0;
                U8 buffer[28];

                U32 flags = 0;
                if (!down) 
                    flags|=BOXED_KEYEVENTF_KEYUP;

                switch (key) {
                case SDLK_ESCAPE:
                    vKey = BOXED_VK_ESCAPE;
                    scan = 0x01;
                    break;
                case SDLK_1:
                    vKey = '1';
                    scan = 0x02;
                    break;
                case SDLK_2:
                    vKey = '2';
                    scan = 0x03;
                    break;
                case SDLK_3:
                    vKey = '3';
                    scan = 0x04;
                    break;
                case SDLK_4:
                    vKey = '4';
                    scan = 0x05;
                    break;
                case SDLK_5:
                    vKey = '5';
                    scan = 0x06;
                    break;
                case SDLK_6:
                    vKey = '6';
                    scan = 0x07;
                    break;
                case SDLK_7:
                    vKey = '7';
                    scan = 0x08;
                    break;
                case SDLK_8:
                    vKey = '8';
                    scan = 0x09;
                    break;
                case SDLK_9:
                    vKey = '9';
                    scan = 0x0a;
                    break;
                case SDLK_0:
                    vKey = '0';
                    scan = 0x0b;
                    break;
                case SDLK_MINUS:
                    vKey = BOXED_VK_OEM_MINUS;
                    scan = 0x0c;
                    break;
                case SDLK_EQUALS:
                    vKey = BOXED_VK_OEM_PLUS;
                    scan = 0x0d;
                    break;
                case SDLK_BACKSPACE:
                    vKey = BOXED_VK_BACK;
                    scan = 0x0e;
                    break;
                case SDLK_TAB:
                    vKey = BOXED_VK_TAB;
                    scan = 0x0f;
                    break;
                case SDLK_q:
                    vKey = 'Q';
                    scan = 0x10;
                    break;
                case SDLK_w:
                    vKey = 'W';
                    scan = 0x11;
                    break;
                case SDLK_e:
                    vKey = 'E';
                    scan = 0x12;
                    break;
                case SDLK_r:
                    vKey = 'R';
                    scan = 0x13;
                    break;
                case SDLK_t:
                    vKey = 'T';
                    scan = 0x14;
                    break;
                case SDLK_y:
                    vKey = 'Y';
                    scan = 0x15;
                    break;
                case SDLK_u:
                    vKey = 'U';
                    scan = 0x16;
                    break;
                case SDLK_i:
                    vKey = 'I';
                    scan = 0x17;
                    break;
                case SDLK_o:
                    vKey = 'O';
                    scan = 0x18;
                    break;
                case SDLK_p:
                    vKey = 'P';
                    scan = 0x19;
                    break;
                case SDLK_LEFTBRACKET:
                    vKey = BOXED_VK_OEM_4;
                    scan = 0x1a;
                    break;
                case SDLK_RIGHTBRACKET:
                    vKey = BOXED_VK_OEM_6;
                    scan = 0x1b;
                    break;
                case SDLK_RETURN:
                    vKey = BOXED_VK_RETURN;
                    scan = 0x1c;
                    break;
                case SDLK_LCTRL:
                    vKey = BOXED_VK_LCONTROL;
                    scan = 0x1d;
                    break;
                case SDLK_RCTRL:
                    vKey = BOXED_VK_RCONTROL;
                    scan = 0x11d;
                    break;
                case SDLK_a:
                    vKey = 'A';
                    scan = 0x1e;
                    break;
                case SDLK_s:
                    vKey = 'S';
                    scan = 0x1f;
                    break;
                case SDLK_d:
                    vKey = 'D';
                    scan = 0x20;
                    break;
                case SDLK_f:
                    vKey = 'F';
                    scan = 0x21;
                    break;
                case SDLK_g:
                    vKey = 'G';
                    scan = 0x22;
                    break;
                case SDLK_h:
                    vKey = 'H';
                    scan = 0x23;
                    break;
                case SDLK_j:
                    vKey = 'J';
                    scan = 0x24;
                    break;
                case SDLK_k:
                    vKey = 'K';
                    scan = 0x25;
                    break;
                case SDLK_l:
                    vKey = 'L';
                    scan = 0x26;
                    break;
                case SDLK_SEMICOLON:
                    vKey = BOXED_VK_OEM_1;
                    scan = 0x27;
                    break;
                case SDLK_QUOTE:
                    vKey = BOXED_VK_OEM_7;
                    scan = 0x28;
                    break;
                case SDLK_BACKQUOTE:
                    vKey = BOXED_VK_OEM_3;
                    scan = 0x29;
                    break;
                case SDLK_LSHIFT:
                    vKey = BOXED_VK_LSHIFT;
                    scan = 0x2a;
                    break;
                case SDLK_RSHIFT:
                    vKey = BOXED_VK_RSHIFT;
                    scan = 0x36;
                    break;
                case SDLK_BACKSLASH:
                    vKey = BOXED_VK_OEM_5;
                    scan = 0x2b;
                    break;
                case SDLK_z:
                    vKey = 'Z';
                    scan = 0x2c;
                    break;
                case SDLK_x:
                    vKey = 'X';
                    scan = 0x2d;
                    break;
                case SDLK_c:
                    vKey = 'C';
                    scan = 0x2e;
                    break;
                case SDLK_v:
                    vKey = 'V';
                    scan = 0x2f;
                    break;
                case SDLK_b:
                    vKey = 'B';
                    scan = 0x30;
                    break;
                case SDLK_n:
                    vKey = 'N';
                    scan = 0x31;
                    break;
                case SDLK_m:
                    vKey = 'M';
                    scan = 0x32;
                    break;
                case SDLK_COMMA:
                    vKey = BOXED_VK_OEM_COMMA;
                    scan = 0x33;
                    break;
                case SDLK_PERIOD:
                    vKey = BOXED_VK_OEM_PERIOD;
                    scan = 0x34;
                    break;
                case SDLK_SLASH:
                    vKey = BOXED_VK_OEM_2;
                    scan = 0x35;
                    break;
                case SDLK_LALT:
                    vKey = BOXED_VK_LMENU;
                    scan = 0x38;
                    break;
                case SDLK_RALT:
                    vKey = BOXED_VK_RMENU;
                    scan = 0x138;
                    break;
                case SDLK_SPACE:
                    vKey = BOXED_VK_SPACE;
                    scan = 0x39;
                    break;
                case SDLK_CAPSLOCK:
                    vKey = BOXED_VK_CAPITAL;
                    scan = 0x3a;
                    break;
                case SDLK_F1:
                    vKey = BOXED_VK_F1;
                    scan = 0x3b;
                    break;
                case SDLK_F2:
                    vKey = BOXED_VK_F2;
                    scan = 0x3c;
                    break;
                case SDLK_F3:
                    vKey = BOXED_VK_F3;
                    scan = 0x3d;
                    break;
                case SDLK_F4:
                    vKey = BOXED_VK_F4;
                    scan = 0x3e;
                    break;
                case SDLK_F5:
                    vKey = BOXED_VK_F5;
                    scan = 0x3f;
                    break;
                case SDLK_F6:
                    vKey = BOXED_VK_F6;
                    scan = 0x40;
                    break;
                case SDLK_F7:
                    vKey = BOXED_VK_F7;
                    scan = 0x41;
                    break;
                case SDLK_F8:
                    vKey = BOXED_VK_F8;
                    scan = 0x42;
                    break;
                case SDLK_F9:
                    vKey = BOXED_VK_F9;
                    scan = 0x43;
                    break;
                case SDLK_F10:
                    vKey = BOXED_VK_F10;
                    scan = 0x44;
                    break;
                case SDLK_NUMLOCK:
                    vKey = BOXED_VK_NUMLOCK;
                    break;
                case SDLK_SCROLLOCK:
                    vKey = BOXED_VK_SCROLL;
                    break;
                case SDLK_F11:
                    vKey = BOXED_VK_F11;
                    scan = 0x57;
                    break;
                case SDLK_F12:
                    vKey = BOXED_VK_F12;
                    scan = 0x58;
                    break;
                case SDLK_HOME:
                    vKey = BOXED_VK_HOME;
                    scan = 0x147;
                    break;
                case SDLK_UP:
                    vKey = BOXED_VK_UP;
                    scan = 0x148;
                    break;
                case SDLK_PAGEUP:
                    vKey = BOXED_VK_PRIOR;
                    scan = 0x149;
                    break;
                case SDLK_LEFT:
                    vKey = BOXED_VK_LEFT;
                    scan = 0x14b;
                    break;
                case SDLK_RIGHT:
                    vKey = BOXED_VK_RIGHT;
                    scan = 0x14d;
                    break;
                case SDLK_END:
                    vKey = BOXED_VK_END;
                    scan = 0x14f;
                    break;
                case SDLK_DOWN:
                    vKey = BOXED_VK_DOWN;
                    scan = 0x150;
                    break;
                case SDLK_PAGEDOWN:
                    vKey = BOXED_VK_NEXT;
                    scan = 0x151;
                    break;
                case SDLK_INSERT:
                    vKey = BOXED_VK_INSERT;
                    scan = 0x152;
                    break;
                case SDLK_DELETE:
                    vKey = BOXED_VK_DELETE;
                    scan = 0x153;
                    break;
                case SDLK_PAUSE:
                    vKey = BOXED_VK_PAUSE;
                    scan = 0x154; // :TODO: is this right?
                    break;
                case SDLK_KP0:
                    scan = 0x52;
                    break;
                case SDLK_KP1:
                    vKey = BOXED_VK_END;
                    scan = 0x4F;
                    break;
                case SDLK_KP2:
                    vKey = BOXED_VK_DOWN;
                    scan = 0x50;
                    break;
                case SDLK_KP3:
                    vKey = BOXED_VK_NEXT;
                    scan = 0x51;
                    break;
                case SDLK_KP4:
                    vKey = BOXED_VK_LEFT;
                    scan = 0x4B;
                    break;
                case SDLK_KP5:
                    scan = 0x4C;
                    break;
                case SDLK_KP6:
                    vKey = BOXED_VK_RIGHT;
                    scan = 0x4D;
                    break;
                case SDLK_KP7:
                    vKey = BOXED_VK_HOME;
                    scan = 0x47;
                    break;
                case SDLK_KP8:
                    vKey = BOXED_VK_UP;
                    scan = 0x48;
                    break;
                case SDLK_KP9:
                    vKey = BOXED_VK_PRIOR;
                    scan = 0x49;
                    break;
                case SDLK_KP_PERIOD:
                    vKey = BOXED_VK_DECIMAL;
                    scan = 0x53;
                    break;
                case SDLK_KP_DIVIDE:
                    vKey = BOXED_VK_DIVIDE;
                    scan = 0x135;
                    break;
                case SDLK_KP_MULTIPLY:
                    vKey = BOXED_VK_MULTIPLY;
                    scan = 0x137;
                    break;
                case SDLK_KP_MINUS:
                    scan = 0x4A;
                    break;
                case SDLK_KP_PLUS:
                    vKey = BOXED_VK_ADD;
                    scan = 0x4E;
                    break;
                case SDLK_KP_ENTER:
                    vKey = BOXED_VK_RETURN;
                    scan = 0x11C;
                    break;

                default:
                    kdebug("Unhandled key: %d", key);
                    return 1;
                }
                if (scan & 0x100)               
                    flags |= BOXED_KEYEVENTF_EXTENDEDKEY;

                writeLittleEndian_4(buffer, 1); // INPUT_KEYBOARD
                writeLittleEndian_2(buffer+4, vKey); // wVk
                writeLittleEndian_2(buffer+6, scan & 0xFF); // wScan
                writeLittleEndian_4(buffer+8, flags); // dwFlags
                writeLittleEndian_4(buffer+12, KSystem::getMilliesSinceStart()); // time
                writeLittleEndian_4(buffer+16, 0); // dwExtraInfo
                writeLittleEndian_4(buffer+20, 0); // pad
                writeLittleEndian_4(buffer+24, 0); // pad

                KUnixSocketObject::unixsocket_write_native_nowait(fd->kobject, buffer, 28);
            }
        }
    }
    return 1;
}

bool KNativeWindowSdl::getMousePos(int* x, int* y) {
#ifdef BOXEDWINE_RECORDER
    if (Player::instance) {
        *x = lastX;
        *y = lastY;
        return 0;
    }
#endif
    unsigned int result = SDL_GetMouseState(x, y);
    
    *x = xFromScreen(*x);
    *y = yFromScreen(*y);

    checkMousePos(*x, *y);

    return result;
}


#ifdef BOXEDWINE_RECORDER

void KNativeWindowSdl::pushWindowSurface() {
    SDL_Surface* src = SDL_GetWindowSurface(window);
    SDL_Rect r;
    U32 depth;

    if (screenBpp()==15)
        depth = 15;
    else if (screenBpp()==16)
        depth = 16;
    else
        depth = 32;
    r.x = 0;
    r.y = 0;
    r.w = src->w;
    r.h = src->h;

    screenCopyTexture = SDL_CreateTexture(renderer, SDL_PIXELFORMAT_ARGB8888, SDL_TEXTUREACCESS_STREAMING, src->w, src->h);
    U32 len = src->w * src->h * src->format->BytesPerPixel;
    U8* pixels = new unsigned char[len];

    if (!SDL_RenderReadPixels(renderer, &src->clip_rect, src->format->format, pixels, src->w * src->format->BytesPerPixel)) {
        SDL_UpdateTexture(screenCopyTexture, NULL, pixels, src->w * src->format->BytesPerPixel);
    }
    delete[] pixels;
}

void KNativeWindowSdl::popWindowSurface() {
    SDL_Surface* dst = SDL_GetWindowSurface(window);
    SDL_Rect rect;
    rect.x = 0;
    rect.y = 0;
    rect.w = dst->w;
    rect.h = dst->h;    
        
    SDL_RenderCopy(renderer, screenCopyTexture, NULL, &rect);	
    SDL_RenderPresent(renderer);

    SDL_DestroyTexture(screenCopyTexture);
    screenCopyTexture = NULL;
}

void KNativeWindowSdl::drawRectOnPushedSurfaceAndDisplay(U32 x, U32 y, U32 w, U32 h, U8 r, U8 g, U8 b, U8 a) {
    SDL_Surface* dst = SDL_GetWindowSurface(window);
    SDL_Rect rect;
    rect.x = 0;
    rect.y = 0;
    rect.w = dst->w;
    rect.h = dst->h;    
        
    SDL_RenderCopy(renderer, screenCopyTexture, NULL, &rect);	

    rect.x = x;
    rect.y = y;
    rect.w = w;
    rect.h = h;

    SDL_SetRenderDrawColor(renderer, r, g, b, a);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_RenderFillRect(renderer, &rect);
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_NONE);

    SDL_RenderPresent(renderer);
}

int getMouseButtonFromEvent(SDL_Event* e) {
    if (e->button.button == SDL_BUTTON_LEFT) {
        return 0;
    } else if (e->button.button == SDL_BUTTON_MIDDLE) {
        return 2;
    } else if (e->button.button == SDL_BUTTON_RIGHT) {
        return 1;
    }
    return 0;
}

void KNativeWindowSdl::processCustomEvents(std::function<bool(bool isKeyDown, int key, bool isF11)> onKey, std::function<bool(bool isButtonDown, int button, int x, int y)> onMouseButton, std::function<bool(int x, int y)> onMouseMove) {
    SDL_Event e;
    while (SDL_WaitEvent(&e)) {
        if (e.type == SDL_KEYUP) {
            if (!onKey(false, e.key.keysym.sym, e.key.keysym.sym == SDLK_F11)) {
                return;
            }
        } else if (e.type == SDL_MOUSEBUTTONDOWN) {
            if (!onMouseButton(true, getMouseButtonFromEvent(&e), e.motion.x, e.motion.y)) {
                return;
            }
        } else if (e.type == SDL_MOUSEBUTTONUP) {
            if (!onMouseButton(false, getMouseButtonFromEvent(&e), e.motion.x, e.motion.y)) {
                return;
            }
        } else if (e.type == SDL_MOUSEMOTION) {
            if (!onMouseMove(e.motion.x, e.motion.y)) {
                return;
            }
        }
#ifdef BOXEDWINE_MULTI_THREADED
        else if (e.type == sdlCustomEvent) {
            SdlCallback* callback = (SdlCallback*)e.user.data1;
            callback->result = (U32)callback->pfn();
            BOXEDWINE_CONDITION_LOCK(callback->cond);
            BOXEDWINE_CONDITION_SIGNAL(callback->cond);
            BOXEDWINE_CONDITION_UNLOCK(callback->cond);
        }
#endif
    }
}

#endif

bool KNativeWindowSdl::internalScreenShot(std::string filepath, SDL_Rect* r, U32* crc) {
#ifdef BOXEDWINE_RECORDER
     if (!recorderBuffer) {
        if (filepath.length()) {
            klog("failed to save screenshot, %s, because recorderBuffer was NULL", filepath.c_str());
        }
        return false;
    }
    U8* pixels = NULL;
    SDL_Surface* s = NULL;
    U32 rMask=0;
    U32 gMask=0;
    U32 bMask=0;
    int bpp = screenBpp()==8?32:screenBpp();

    if (bpp==32) {
        rMask = 0x00FF0000;
        gMask = 0x0000FF00;
        bMask = 0x000000FF;
    } else if (bpp==16) {
        rMask = 0xF800;
        gMask = 0x07E0;
        bMask = 0x001F;
    } else {
        kpanic("Unhandled bpp for screen shot: %d", bpp);
    }
    if (r) {
        int inPitch = (screenWidth()*((bpp+7)/8)+3) & ~3;
        int outPitch = (r->w*((bpp+7)/8)+3) & ~3;        
        U32 bytesPerPixel = (bpp+7)/8;

        pixels = new unsigned char[outPitch*r->h];

        for (int y=0;y<r->h;y++) {
            memcpy(pixels+y*outPitch, recorderBuffer+(y+r->y)*inPitch+(r->x*bytesPerPixel), outPitch);
        }
        s = SDL_CreateRGBSurfaceFrom(pixels, r->w, r->h, bpp, outPitch, rMask, gMask, bMask, 0);
        if (crc) {
            U32 len = outPitch*r->h;
            *crc = crc32b(pixels, len);
        }
    } else {               
        int pitch = (screenWidth()*((bpp+7)/8)+3) & ~3;
        s = SDL_CreateRGBSurfaceFrom(recorderBuffer, screenWidth(), screenHeight(), bpp, pitch, rMask, gMask, bMask, 0);
        if (crc) {
            U32 len = pitch*screenHeight();
            *crc = crc32b(recorderBuffer, len);
        }
    }

    if (!s) {
        klog("sdlScreenshot: %s", SDL_GetError());
        delete[] pixels;
        return false;
    }
    if (filepath.length()) {
        SDL_SaveBMP(s, filepath.c_str());
    }    
    if (s) {
        SDL_FreeSurface(s);
    }
    if (pixels) {
        delete[] pixels;
    }
    return true;
#else
    return false;
#endif
}

bool KNativeWindowSdl::partialScreenShot(std::string filepath, U32 x, U32 y, U32 w, U32 h, U32* crc) {
    SDL_Rect r;
    r.x = x;
    r.y = y;
    r.w = w;
    r.h = h;
    return internalScreenShot(filepath, &r, crc);
}

bool KNativeWindowSdl::screenShot(std::string filepath, U32* crc) {
    return internalScreenShot(filepath, NULL, crc);

}

#define SDLK_NUMLOCK SDL_SCANCODE_NUMLOCKCLEAR
#define SDLK_SCROLLOCK SDLK_SCROLLLOCK

static U32 translate(U32 key) {
    switch (key) {
        case SDLK_ESCAPE:
            return K_KEY_ESC;
        case SDLK_1:
            return K_KEY_1;
        case SDLK_2:
            return K_KEY_2;
        case SDLK_3:
            return K_KEY_3;
        case SDLK_4:
            return K_KEY_4;
        case SDLK_5:
            return K_KEY_5;
        case SDLK_6:
            return K_KEY_6;
        case SDLK_7:
            return K_KEY_7;
        case SDLK_8:
            return K_KEY_8;
        case SDLK_9:
            return K_KEY_9;
        case SDLK_0:
            return K_KEY_0;
        case SDLK_MINUS:
            return K_KEY_MINUS;
        case SDLK_EQUALS:
            return K_KEY_EQUAL;
        case SDLK_BACKSPACE:
            return K_KEY_BACKSPACE;
        case SDLK_TAB:
            return K_KEY_TAB;
        case SDLK_q:
            return K_KEY_Q;
        case SDLK_w:
            return K_KEY_W;
        case SDLK_e:
            return K_KEY_E;
        case SDLK_r:
            return K_KEY_R;
        case SDLK_t:
            return K_KEY_T;
        case SDLK_y:
            return K_KEY_Y;
        case SDLK_u:
            return K_KEY_U;
        case SDLK_i:
            return K_KEY_I;
        case SDLK_o:
            return K_KEY_O;
        case SDLK_p:
            return K_KEY_P;
        case SDLK_LEFTBRACKET:
            return K_KEY_LEFTBRACE;
        case SDLK_RIGHTBRACKET:
            return K_KEY_RIGHTBRACE;
        case SDLK_RETURN:
            return K_KEY_ENTER;
        case SDLK_LCTRL:
            return K_KEY_LEFTCTRL;
        case SDLK_RCTRL:
            return K_KEY_RIGHTCTRL;
        case SDLK_a:
            return K_KEY_A;
        case SDLK_s:
            return K_KEY_S;
        case SDLK_d:
            return K_KEY_D;
        case SDLK_f:
            return K_KEY_F;
        case SDLK_g:
            return K_KEY_G;
        case SDLK_h:
            return K_KEY_H;
        case SDLK_j:
            return K_KEY_J;
        case SDLK_k:
            return K_KEY_K;
        case SDLK_l:
            return K_KEY_L;
        case SDLK_SEMICOLON:
            return K_KEY_SEMICOLON;
        case SDLK_QUOTE:
            return K_KEY_APOSTROPHE;
        case SDLK_BACKQUOTE:
            return K_KEY_GRAVE;
        case SDLK_LSHIFT:
            return K_KEY_LEFTSHIFT;
        case SDLK_RSHIFT:
            return K_KEY_RIGHTSHIFT;
        case SDLK_BACKSLASH:
            return K_KEY_BACKSLASH;
        case SDLK_z:
            return K_KEY_Z;
        case SDLK_x:
            return K_KEY_X;
        case SDLK_c:
            return K_KEY_C;
        case SDLK_v:
            return K_KEY_V;
        case SDLK_b:
            return K_KEY_B;
        case SDLK_n:
            return K_KEY_N;
        case SDLK_m:
            return K_KEY_M;
        case SDLK_COMMA:
            return K_KEY_COMMA;
        case SDLK_PERIOD:
            return K_KEY_DOT;
        case SDLK_SLASH:
            return K_KEY_SLASH;
        case SDLK_LALT:
             return K_KEY_LEFTALT;
        case SDLK_RALT:
            return K_KEY_RIGHTALT;
        case SDLK_SPACE:
            return K_KEY_SPACE;
        case SDLK_CAPSLOCK:
            return K_KEY_CAPSLOCK;
        case SDLK_F1:
            return K_KEY_F1;
        case SDLK_F2:
            return K_KEY_F2;
        case SDLK_F3:
            return K_KEY_F3;
        case SDLK_F4:
            return K_KEY_F4;
        case SDLK_F5:
            return K_KEY_F5;
        case SDLK_F6:
            return K_KEY_F6;
        case SDLK_F7:
            return K_KEY_F7;
        case SDLK_F8:
            return K_KEY_F8;
        case SDLK_F9:
            return K_KEY_F9;
        case SDLK_F10:
            return K_KEY_F10;
        case SDLK_NUMLOCK:
            return K_KEY_NUMLOCK;
        case SDLK_SCROLLOCK:
            return K_KEY_SCROLLLOCK;
        case SDLK_F11:
            return K_KEY_F11;
        case SDLK_F12:
            return K_KEY_F12;
        case SDLK_HOME:
            return K_KEY_HOME;
        case SDLK_UP:
            return K_KEY_UP;
        case SDLK_PAGEUP:
            return K_KEY_PAGEUP;
        case SDLK_LEFT:
            return K_KEY_LEFT;
        case SDLK_RIGHT:
            return K_KEY_RIGHT;
        case SDLK_END:
            return K_KEY_END;
        case SDLK_DOWN:
            return K_KEY_DOWN;
        case SDLK_PAGEDOWN:
            return K_KEY_PAGEDOWN;
        case SDLK_INSERT:
            return K_KEY_INSERT;
        case SDLK_DELETE:
            return K_KEY_DELETE;
        case SDLK_PAUSE:
            return K_KEY_PAUSE;
        default:
            kdebug("Unhandled key: %d", key);
            return 0;
    }
}

bool KNativeWindowSdl::isShutdownWindowIsOpen() {
    return shutdownWindow!=NULL;
}
    
void KNativeWindowSdl::updateShutdownWindow() {
    int i = 100 - (KSystem::killTime-KSystem::getMilliesSinceStart())/100 + 1;
    SDL_Rect rect;
    rect.x = 10;
    rect.y = 90;
    rect.w = (320 - 20) * i / 100;
    rect.h = 20;
    SDL_RenderFillRect(shutdownRenderer, &rect);
    SDL_RenderPresent(shutdownRenderer);
}
    
static U64 lastEvent;

bool KNativeWindowSdl::handlSdlEvent(SDL_Event* e) {
#ifdef BOXEDWINE_RECORDER
    if (Player::instance) {
        if (e->type == SDL_QUIT) {
            return false;
        }
        return true;
    }
#endif    
    if (e->type == SDL_QUIT) {
        KThread::setCurrentThread(NULL);
        std::shared_ptr<KProcess> p = KSystem::getProcess(10);
        if (p && !KSystem::shutingDown) {
            // Give the system 10 seconds to try and shutdown cleanly, this is so wineserver can flush registry changes            
#ifndef __EMSCRIPTEN__
            shutdownWindow = SDL_CreateWindow("Shutting Down ...", SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED, 320, 120, SDL_WINDOW_SHOWN | SDL_WINDOW_ALWAYS_ON_TOP);
            shutdownRenderer  = SDL_CreateRenderer(shutdownWindow, -1, 0);
            SDL_SetRenderDrawColor(shutdownRenderer, 0, 0, 0, 255);
            SDL_RenderClear(shutdownRenderer);
            SDL_RenderPresent(shutdownRenderer);
            SDL_SetRenderDrawColor(shutdownRenderer, 255, 255, 0, 255);
            KSystem::killTime = KSystem::getMilliesSinceStart()+10000;
            updateShutdownWindow();
#if defined (BOXEDWINE_MULTI_THREADED) && !defined (__TEST)
            runInBackgroundThread([p]() {
                p->killAllThreads();
                KSystem::eraseProcess(p->id);
            });
#else
            p->killAllThreads();
            KSystem::eraseProcess(p->id);
#endif
            return true;
#endif
        }
        return false;
    } else if (e->type == SDL_MOUSEMOTION) {         
        if (KSystem::pollRate) {
            if (lastEvent + (1000000 / KSystem::pollRate) > KSystem::getMicroCounter()) {
                return true;
            }
            lastEvent = KSystem::getMicroCounter();
        }
        BOXEDWINE_RECORDER_HANDLE_MOUSE_MOVE(e->motion.x, e->motion.y);
        if (relativeMouse) {
            if (!mouseMove((e->motion.x-screenWidth()/2)*rel_mouse_sensitivity/100, (e->motion.y-screenHeight()/2)*rel_mouse_sensitivity/100, true)) {
                onMouseMove((e->motion.x-screenWidth()/2)*rel_mouse_sensitivity/100, (e->motion.y-screenHeight()/2)*rel_mouse_sensitivity/100, true);                
            }      
            SDL_WarpMouseInWindow(window, screenWidth()/2, screenHeight()/2);
        } else {
            if (!mouseMove(e->motion.x, e->motion.y, false)) {
                onMouseMove(e->motion.x, e->motion.y, false);
            }
        }
        
    } else if (e->type == SDL_MOUSEBUTTONDOWN) {
#ifdef BOXEDWINE_MULTI_THREADED
        if (KSystem::pollRate) {
            while (lastEvent + (1000000 / KSystem::pollRate) > KSystem::getMicroCounter()) {
                SDL_Delay(1);
            }
            lastEvent = KSystem::getMicroCounter();
        }
#endif        
        if (e->button.button==SDL_BUTTON_LEFT) {
            BOXEDWINE_RECORDER_HANDLE_MOUSE_BUTTON_DOWN(0, e->motion.x, e->motion.y);
            if (!mouseButton(1, 0, (relativeMouse?0:e->motion.x), (relativeMouse?0:e->motion.y)))
                onMouseButtonDown(0);
        } else if (e->button.button == SDL_BUTTON_MIDDLE) {
            BOXEDWINE_RECORDER_HANDLE_MOUSE_BUTTON_DOWN(2, e->motion.x, e->motion.y);
            if (!mouseButton(1, 2, (relativeMouse?0:e->motion.x), (relativeMouse?0:e->motion.y)))
                onMouseButtonDown(2);
        } else if (e->button.button == SDL_BUTTON_RIGHT) {
            BOXEDWINE_RECORDER_HANDLE_MOUSE_BUTTON_DOWN(1, e->motion.x, e->motion.y);
            if (!mouseButton(1, 1, (relativeMouse?0:e->motion.x), (relativeMouse?0:e->motion.y)))
                onMouseButtonDown(1);
        }
    } else if (e->type == SDL_MOUSEBUTTONUP) {
#ifdef BOXEDWINE_MULTI_THREADED
        if (KSystem::pollRate) {
            while (lastEvent + (1000000 / KSystem::pollRate) > KSystem::getMicroCounter()) {
                SDL_Delay(1);
            }
            lastEvent = KSystem::getMicroCounter();
        }
#endif        
        if (e->button.button==SDL_BUTTON_LEFT) {
            BOXEDWINE_RECORDER_HANDLE_MOUSE_BUTTON_UP(0, e->motion.x, e->motion.y);
            if (!mouseButton(0, 0, (relativeMouse?0:e->motion.x), (relativeMouse?0:e->motion.y)))
                onMouseButtonUp(0);
        } else if (e->button.button == SDL_BUTTON_MIDDLE) {
            BOXEDWINE_RECORDER_HANDLE_MOUSE_BUTTON_UP(2, e->motion.x, e->motion.y);
            if (!mouseButton(0, 2, (relativeMouse?0:e->motion.x), (relativeMouse?0:e->motion.y)))
                onMouseButtonUp(2);
        } else if (e->button.button == SDL_BUTTON_RIGHT) {
            BOXEDWINE_RECORDER_HANDLE_MOUSE_BUTTON_UP(1, e->motion.x, e->motion.y);
            if (!mouseButton(0, 1, (relativeMouse?0:e->motion.x), (relativeMouse?0:e->motion.y)))
                onMouseButtonUp(1);
        }
    } else if (e->type == SDL_MOUSEWHEEL) {
#ifdef BOXEDWINE_MULTI_THREADED
        if (KSystem::pollRate) {
            while (lastEvent + (1000000 / KSystem::pollRate) > KSystem::getMicroCounter()) {
                SDL_Delay(1);
            }
            lastEvent = KSystem::getMicroCounter();
        }
#endif
        // Handle up/down mouse wheel movements
        int x, y;
        SDL_GetMouseState(&x, &y);
        if (!mouseWheel(e->wheel.y*80, (relativeMouse?0:x), (relativeMouse?0:y))) {
            onMouseWheel(e->wheel.y);
        }
    } else if (e->type == SDL_KEYDOWN) {
#ifdef BOXEDWINE_MULTI_THREADED
        if (KSystem::pollRate) {
            while (lastEvent + (1000000 / KSystem::pollRate) > KSystem::getMicroCounter()) {
                SDL_Delay(1);
            }
            lastEvent = KSystem::getMicroCounter();
        }
#endif        
        if (!BOXEDWINE_RECORDER_HANDLE_KEY_DOWN(e->key.keysym.sym, e->key.keysym.sym == SDLK_F11)) {
            if (e->key.keysym.sym==SDLK_SCROLLOCK) {
                KSystem::printStacks();
            }
            else if (!key(e->key.keysym.sym, 1)) {
                onKeyDown(translate(e->key.keysym.sym));
            }
/*
            // I'm leaving this relative mouse code in for now since it might be useful later
            // Currently Wine will automatically handle it
            if (e->key.keysym.sym==SDLK_RETURN && (SDL_GetModState() & KMOD_RALT)) {                
                if (relativeMouse) {
                    SDL_SetWindowGrab(sdlWindow, SDL_FALSE);
                    relativeMouse = false;
                } else {
                    SDL_SetCursor(NULL);
                    SDL_SetWindowGrab(sdlWindow, SDL_FALSE);
                    SDL_WarpMouseInWindow(sdlWindow, screenCx/2, screenCy/2);
                    relativeMouse = true;
                }                
            }
*/
        }
    } else if (e->type == SDL_KEYUP) {
#ifdef BOXEDWINE_MULTI_THREADED
        if (KSystem::pollRate) {
            while (lastEvent + (1000000 / KSystem::pollRate) > KSystem::getMicroCounter()) {
                SDL_Delay(1);
            }
            lastEvent = KSystem::getMicroCounter();
        }
#endif
        if (!BOXEDWINE_RECORDER_HANDLE_KEY_UP(e->key.keysym.sym, e->key.keysym.sym == SDLK_F11)) {
            if (!key(e->key.keysym.sym, 0)) {
                onKeyUp(translate(e->key.keysym.sym));
            }
        }
    }
#ifdef BOXEDWINE_EXPERIMENTAL_FRAME_BUFFER
    else if (e->type == SDL_WINDOWEVENT) {
        BOXEDWINE_CRITICAL_SECTION_WITH_MUTEX(hwndToWndMutex);
        if (!hwndToWnd.size())
            flipFBNoCheck();
    }
#endif
    return true;
}

std::shared_ptr<KNativeWindow> KNativeWindow::getNativeWindow() {
    return screen;    
}

void KNativeWindow::shutdown() {
    {
        BOXEDWINE_CRITICAL_SECTION_WITH_MUTEX(screen->sdlMutex);
        screen->destroyScreen(NULL);
        
    }
    screen = NULL;
#if defined(BOXEDWINE_OPENGL_OSMESA)
    shutdownMesaOpenGL();
#endif
}
