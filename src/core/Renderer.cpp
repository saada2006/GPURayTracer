#include "Renderer.h"
#include "OpenGL.h"

#include <stdio.h>
#include <iostream>

void DebugMessageCallback(GLenum source, GLenum type, GLuint id,
    GLenum severity, GLsizei length,
    const GLchar* msg, const void* data)
{
    char* _source;
    char* _type;
    char* _severity;

    switch (source) {
    case GL_DEBUG_SOURCE_API:
        _source = "API";
        break;

    case GL_DEBUG_SOURCE_WINDOW_SYSTEM:
        _source = "WINDOW SYSTEM";
        break;

    case GL_DEBUG_SOURCE_SHADER_COMPILER:
        _source = "SHADER COMPILER";
        break;

    case GL_DEBUG_SOURCE_THIRD_PARTY:
        _source = "THIRD PARTY";
        break;

    case GL_DEBUG_SOURCE_APPLICATION:
        _source = "APPLICATION";
        break;

    case GL_DEBUG_SOURCE_OTHER:
        _source = "UNKNOWN";
        break;

    default:
        _source = "UNKNOWN";
        break;
    }

    switch (type) {
    case GL_DEBUG_TYPE_ERROR:
        _type = "ERROR";
        break;

    case GL_DEBUG_TYPE_DEPRECATED_BEHAVIOR:
        _type = "DEPRECATED BEHAVIOR";
        break;

    case GL_DEBUG_TYPE_UNDEFINED_BEHAVIOR:
        _type = "UDEFINED BEHAVIOR";
        break;

    case GL_DEBUG_TYPE_PORTABILITY:
        _type = "PORTABILITY";
        break;

    case GL_DEBUG_TYPE_PERFORMANCE:
        _type = "PERFORMANCE";
        break;

    case GL_DEBUG_TYPE_OTHER:
        _type = "OTHER";
        break;

    case GL_DEBUG_TYPE_MARKER:
        _type = "MARKER";
        break;

    default:
        _type = "UNKNOWN";
        break;
    }

    switch (severity) {
    case GL_DEBUG_SEVERITY_HIGH:
        _severity = "HIGH";
        break;

    case GL_DEBUG_SEVERITY_MEDIUM:
        _severity = "MEDIUM";
        break;

    case GL_DEBUG_SEVERITY_LOW:
        _severity = "LOW";
        break;

    case GL_DEBUG_SEVERITY_NOTIFICATION:
        // Sorry, but I don't care and I also don't want you cluterring my log
        return;
        _severity = "NOTIFICATION";
        break;

    default:
        _severity = "UNKNOWN";
        break;
    }

    printf("%d: %s of %s severity, raised from %s: %s\n",
        id, _type, _severity, _source, msg);
}

void Renderer::Initialize(Window* Window, const char* scenePath) {
    bindedWindow = Window;
    viewportWidth = bindedWindow->Width;
    viewportHeight = bindedWindow->Height;

    static bool OpenGLLoaded = false;
    if (!OpenGLLoaded) {
        glewInit();
        OpenGLLoaded = true;
    }

    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClearDepth(1.0f);
    glClearStencil(0xFF);

    glEnable(GL_DEBUG_OUTPUT);
    glDebugMessageCallback(DebugMessageCallback, NULL);

    scene.LoadScene(scenePath);

    float quad[] = {
    -1.0f,  1.0f,
    -1.0f, -1.0f,
     1.0f, -1.0f,

    -1.0f,  1.0f,
     1.0f, -1.0f,
     1.0f,  1.0f
    };

    quadBuf.CreateBinding(BUFFER_TARGET_ARRAY);
    quadBuf.UploadData(sizeof(quad), quad);

    screenQuad.CreateBinding();
    screenQuad.CreateStream(0, 2, 2 * sizeof(float));
    //

    presentShader.CompileFiles("present/Present.vert", "present/Present.frag");

    colorTexture.CreateBinding();
    colorTexture.LoadData(GL_RGBA16F, GL_RGBA, GL_FLOAT, viewportWidth, viewportHeight, nullptr);


    rayDepth.CreateBinding();
    rayDepth.LoadData(GL_R32F, GL_RED, GL_FLOAT, viewportWidth, viewportHeight, nullptr);

    rayOrigin.CreateBinding();
    rayOrigin.LoadData(GL_RGBA16F, GL_RGB, GL_HALF_FLOAT, viewportWidth, viewportHeight, nullptr);

    rayDirection.CreateBinding();
    rayDirection.LoadData(GL_RGBA16F, GL_RGB, GL_HALF_FLOAT, viewportWidth, viewportHeight, nullptr);

    genRays.CompileFile("kernel/raygen/FiniteAperture.comp");
    closestHit.CompileFile("kernel/intersect/Closest.comp");

    colorTexture.BindImageUnit(0, GL_RGBA16F);
    rayOrigin.BindImageUnit(1, GL_RGBA16F);
    rayDirection.BindImageUnit(2, GL_RGBA16F);
    rayDepth.BindImageUnit(3, GL_R32F);

    colorTexture.BindTextureUnit(4, GL_TEXTURE_2D);

    genRays.CreateBinding();
    genRays.LoadInteger("RayOrigin", 1);
    genRays.LoadInteger("RayDirection", 2);

    closestHit.CreateBinding();
    closestHit.LoadInteger("ColorOutput", 0);
    closestHit.LoadInteger("RayOrigin", 1);
    closestHit.LoadInteger("RayDirection", 2);
    closestHit.LoadInteger("IntersectionDepth", 3);

    closestHit.LoadShaderStorageBuffer("Samplers", scene.textureHandlesBuf);
    closestHit.LoadShaderStorageBuffer("vertexBuf", scene.vertexBuf);
    closestHit.LoadShaderStorageBuffer("indexBuf", scene.indexBuf);
    closestHit.LoadShaderStorageBuffer("nodes", scene.bvh.nodes);
    closestHit.LoadShaderStorageBuffer("leaves", scene.bvh.leaves);


    presentShader.CreateBinding();
    presentShader.LoadInteger("ColorTexture", 4);


    frameCounter = 0;
}

void Renderer::CleanUp(void) {
    // I hope the scene destructor frees its resources
    genRays.Free();
    closestHit.Free();
    presentShader.Free();

    colorTexture.Free();
    rayDepth.Free();
    rayOrigin.Free();
    rayDirection.Free();

    screenQuad.Free();
    quadBuf.Free();
}

void Renderer::RenderFrame(const Camera& camera) {
    // We only need to clear the ray depth; the color, ray origin, and ray dir will be overwritten
    const float kClear = 1e20f;
    glClearTexImage(rayDepth.GetHandle(), 0, GL_RED, GL_FLOAT, &kClear);

    genRays.CreateBinding();
    genRays.LoadCamera("Camera", camera);

    glDispatchCompute(viewportWidth / 8, viewportHeight / 8, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT);

    closestHit.CreateBinding();
    closestHit.LoadInteger("Frame", frameCounter++);

    glDispatchCompute(viewportWidth / 8, viewportHeight / 8, 1);
    glMemoryBarrier(GL_SHADER_IMAGE_ACCESS_BARRIER_BIT | GL_TEXTURE_FETCH_BARRIER_BIT);
}

void Renderer::Present() {
    presentShader.CreateBinding();
    glDrawArrays(GL_TRIANGLES, 0, 6);
}