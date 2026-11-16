#include <ncl_gl/loader.h>

#include <cstddef>

#include <SDL3/SDL.h>
#include <glad/glad.h>

void* ncl_gl::LoadProc(const char* name) {
    return reinterpret_cast<void*>(SDL_GL_GetProcAddress(name));
}

namespace {
bool g_LoadDone;
bool g_LoadOk;
} // namespace

bool ncl_gl::EnsureLoaded() {
    if (g_LoadDone) {
        return g_LoadOk;
    }

    return Reload();
}

bool ncl_gl::Reload() {
    g_LoadDone = true;
    // GLAD skips feature loaders that the new context does not advertise. Clear
    // optional core FBO slots before reloading so an older context cannot retain
    // entry points from a previous driver's newer context.
    glBindFramebuffer = nullptr;
    glDeleteFramebuffers = nullptr;
    glGenFramebuffers = nullptr;
    glCheckFramebufferStatus = nullptr;
    glFramebufferTexture2D = nullptr;
    glFramebufferRenderbuffer = nullptr;
    glBindRenderbuffer = nullptr;
    glDeleteRenderbuffers = nullptr;
    glGenRenderbuffers = nullptr;
    glRenderbufferStorage = nullptr;
    glGetFramebufferAttachmentParameteriv = nullptr;
    glGetRenderbufferParameteriv = nullptr;
    glIsFramebuffer = nullptr;
    glIsRenderbuffer = nullptr;
    glGenerateMipmap = nullptr;
    glBlitFramebuffer = nullptr;
    glRenderbufferStorageMultisample = nullptr;
    g_LoadOk = gladLoadGLLoader(reinterpret_cast<GLADloadproc>(LoadProc)) != 0;
    if (g_LoadOk && GLAD_GL_EXT_framebuffer_object) {
        // macOS's compatibility context exposes EXT FBOs at OpenGL 2.1. Their
        // signatures and framebuffer enums match the core operations used here.
        if (!glBindFramebuffer) {
            glBindFramebuffer = glBindFramebufferEXT;
        }
        if (!glDeleteFramebuffers) {
            glDeleteFramebuffers = glDeleteFramebuffersEXT;
        }
        if (!glGenFramebuffers) {
            glGenFramebuffers = glGenFramebuffersEXT;
        }
        if (!glCheckFramebufferStatus) {
            glCheckFramebufferStatus = glCheckFramebufferStatusEXT;
        }
        if (!glFramebufferTexture2D) {
            glFramebufferTexture2D = glFramebufferTexture2DEXT;
        }
        if (!glFramebufferRenderbuffer) {
            glFramebufferRenderbuffer = glFramebufferRenderbufferEXT;
        }
        if (!glBindRenderbuffer) {
            glBindRenderbuffer = glBindRenderbufferEXT;
        }
        if (!glDeleteRenderbuffers) {
            glDeleteRenderbuffers = glDeleteRenderbuffersEXT;
        }
        if (!glGenRenderbuffers) {
            glGenRenderbuffers = glGenRenderbuffersEXT;
        }
        if (!glRenderbufferStorage) {
            glRenderbufferStorage = glRenderbufferStorageEXT;
        }
        if (!glGetFramebufferAttachmentParameteriv) {
            glGetFramebufferAttachmentParameteriv = glGetFramebufferAttachmentParameterivEXT;
        }
        if (!glGetRenderbufferParameteriv) {
            glGetRenderbufferParameteriv = glGetRenderbufferParameterivEXT;
        }
        if (!glIsFramebuffer) {
            glIsFramebuffer = glIsFramebufferEXT;
        }
        if (!glIsRenderbuffer) {
            glIsRenderbuffer = glIsRenderbufferEXT;
        }
        if (!glGenerateMipmap) {
            glGenerateMipmap = glGenerateMipmapEXT;
        }
    }
    if (g_LoadOk && !glBlitFramebuffer && GLAD_GL_EXT_framebuffer_blit) {
        glBlitFramebuffer = glBlitFramebufferEXT;
    }
    if (g_LoadOk && !glRenderbufferStorageMultisample && GLAD_GL_EXT_framebuffer_multisample) {
        glRenderbufferStorageMultisample = glRenderbufferStorageMultisampleEXT;
    }

    return g_LoadOk;
}

bool ncl_gl::IsLoaded() {
    return g_LoadDone && g_LoadOk;
}
