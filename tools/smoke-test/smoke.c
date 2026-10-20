/*
 * MM-PS5 smoke test: checks the pieces OpenEnroth needs on a native PS5 title.
 *
 * - SDL2 video through the ps5-opengl G19 driver, with the same context request
 *   OpenEnroth makes when built with OE_GL33 (3.3 core, 24-bit depth, 8-bit stencil).
 * - Runtime GLSL compilation and drawing.
 * - DualSense through SDL's GameController API: buttons, axes, rumble, light bar.
 *
 * Everything is drawn on screen, so the result can be read off the TV. The same
 * lines also go to stdout, which the runtime shim sends to /download0.
 */
#include <SDL.h>
#include <GL/glcorearb.h>
#include <stdarg.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

#include "stb_easy_font.h"

#define GL_FUNCTIONS(X) \
    X(PFNGLGETSTRINGPROC, glGetString) \
    X(PFNGLGETINTEGERVPROC, glGetIntegerv) \
    X(PFNGLGETERRORPROC, glGetError) \
    X(PFNGLVIEWPORTPROC, glViewport) \
    X(PFNGLCLEARCOLORPROC, glClearColor) \
    X(PFNGLCLEARPROC, glClear) \
    X(PFNGLENABLEPROC, glEnable) \
    X(PFNGLBLENDFUNCPROC, glBlendFunc) \
    X(PFNGLCREATESHADERPROC, glCreateShader) \
    X(PFNGLSHADERSOURCEPROC, glShaderSource) \
    X(PFNGLCOMPILESHADERPROC, glCompileShader) \
    X(PFNGLGETSHADERIVPROC, glGetShaderiv) \
    X(PFNGLGETSHADERINFOLOGPROC, glGetShaderInfoLog) \
    X(PFNGLCREATEPROGRAMPROC, glCreateProgram) \
    X(PFNGLATTACHSHADERPROC, glAttachShader) \
    X(PFNGLLINKPROGRAMPROC, glLinkProgram) \
    X(PFNGLGETPROGRAMIVPROC, glGetProgramiv) \
    X(PFNGLGETPROGRAMINFOLOGPROC, glGetProgramInfoLog) \
    X(PFNGLUSEPROGRAMPROC, glUseProgram) \
    X(PFNGLGENVERTEXARRAYSPROC, glGenVertexArrays) \
    X(PFNGLBINDVERTEXARRAYPROC, glBindVertexArray) \
    X(PFNGLGENBUFFERSPROC, glGenBuffers) \
    X(PFNGLBINDBUFFERPROC, glBindBuffer) \
    X(PFNGLBUFFERDATAPROC, glBufferData) \
    X(PFNGLENABLEVERTEXATTRIBARRAYPROC, glEnableVertexAttribArray) \
    X(PFNGLVERTEXATTRIBPOINTERPROC, glVertexAttribPointer) \
    X(PFNGLDRAWARRAYSPROC, glDrawArrays)

#define DECLARE(type, name) static type p_##name;
GL_FUNCTIONS(DECLARE)
#undef DECLARE

typedef struct {
    float x, y;
    uint8_t r, g, b, a;
} Vertex;

#define MAX_VERTICES 120000
static Vertex vertices[MAX_VERTICES];
static int vertexCount;
static int screenW, screenH;

static char statusLines[16][160];
static int statusCount;

static void status(const char *format, ...) {
    va_list args;
    if (statusCount >= 16)
        return;
    va_start(args, format);
    vsnprintf(statusLines[statusCount], sizeof(statusLines[statusCount]), format, args);
    va_end(args);
    printf("[mmps5-smoke] %s\n", statusLines[statusCount]);
    statusCount++;
}

static void pushVertex(float x, float y, const uint8_t c[4]) {
    if (vertexCount >= MAX_VERTICES)
        return;
    vertices[vertexCount].x = x / (float)screenW * 2.0f - 1.0f;
    vertices[vertexCount].y = 1.0f - y / (float)screenH * 2.0f;
    vertices[vertexCount].r = c[0];
    vertices[vertexCount].g = c[1];
    vertices[vertexCount].b = c[2];
    vertices[vertexCount].a = c[3];
    vertexCount++;
}

static void pushRect(float x, float y, float w, float h, const uint8_t c[4]) {
    pushVertex(x, y, c);
    pushVertex(x + w, y, c);
    pushVertex(x + w, y + h, c);
    pushVertex(x, y, c);
    pushVertex(x + w, y + h, c);
    pushVertex(x, y + h, c);
}

static void pushText(float x, float y, float scale, const uint8_t c[4], const char *text) {
    static char quads[99999];
    char copy[512];
    int count;
    SDL_strlcpy(copy, text, sizeof(copy));
    count = stb_easy_font_print(0, 0, copy, NULL, quads, sizeof(quads));
    for (int i = 0; i < count; i++) {
        /* stb_easy_font emits 4 vertices per quad: float x, y, z then 4 color bytes. */
        const float *q0 = (const float *)(quads + (i * 4 + 0) * 16);
        const float *q2 = (const float *)(quads + (i * 4 + 2) * 16);
        pushRect(x + q0[0] * scale, y + q0[1] * scale, (q2[0] - q0[0]) * scale, (q2[1] - q0[1]) * scale, c);
    }
}

static GLuint compile(GLenum type, const char *source) {
    GLuint shader = p_glCreateShader(type);
    GLint ok = 0;
    char log[1024] = {0};
    p_glShaderSource(shader, 1, &source, NULL);
    p_glCompileShader(shader);
    p_glGetShaderiv(shader, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        p_glGetShaderInfoLog(shader, sizeof(log), NULL, log);
        status("Shader compile FAILED: %.120s", log);
        return 0;
    }
    return shader;
}

static const char *vertexSource =
    "#version 330 core\n"
    "layout (location = 0) in vec2 aPos;\n"
    "layout (location = 1) in vec4 aColor;\n"
    "out vec4 vColor;\n"
    "void main() { vColor = aColor; gl_Position = vec4(aPos, 0.0, 1.0); }\n";

static const char *fragmentSource =
    "#version 330 core\n"
    "in vec4 vColor;\n"
    "out vec4 fragColor;\n"
    "void main() { fragColor = vColor; }\n";

static const uint8_t WHITE[4] = {235, 235, 235, 255};
static const uint8_t GREY[4] = {90, 90, 100, 255};
static const uint8_t DIM[4] = {40, 40, 52, 255};
static const uint8_t GREEN[4] = {80, 220, 120, 255};
static const uint8_t RED[4] = {240, 80, 80, 255};
static const uint8_t YELLOW[4] = {255, 200, 87, 255};
static const uint8_t BLUE[4] = {90, 160, 255, 255};

static void drawStick(float cx, float cy, float radius, Sint16 ax, Sint16 ay, const char *label) {
    float px = cx + (float)ax / 32767.0f * radius;
    float py = cy + (float)ay / 32767.0f * radius;
    pushRect(cx - radius, cy - radius, radius * 2, radius * 2, DIM);
    pushRect(cx - 1, cy - radius, 2, radius * 2, GREY);
    pushRect(cx - radius, cy - 1, radius * 2, 2, GREY);
    pushRect(px - 14, py - 14, 28, 28, YELLOW);
    pushText(cx - radius, cy + radius + 10, 3, WHITE, label);
}

static void drawTrigger(float x, float y, float w, float h, Sint16 value, const char *label) {
    float fill = (float)value / 32767.0f * h;
    pushRect(x, y, w, h, DIM);
    pushRect(x, y + h - fill, w, fill, BLUE);
    pushText(x, y + h + 10, 3, WHITE, label);
}

int main(int argc, char **argv) {
    SDL_Window *window = NULL;
    SDL_GLContext context = NULL;
    SDL_GameController *pad = NULL;
    SDL_DisplayMode mode;
    GLuint program, vao, vbo, vs, fs;
    GLint linked = 0, major = 0, minor = 0, depthBits = -1, stencilBits = -1;
    Uint64 lastTick, fpsTick;
    unsigned frames = 0, fpsFrames = 0;
    float fps = 0;
    char padLine[200] = "Controller: none connected (press PS button / reconnect)";
    char featureLine[200] = "";
    char actionLine[200] = "R3: rumble test    L3: cycle light bar";
    int ledIndex = 0;
    GLenum lastError = GL_NO_ERROR;
    (void)argc;
    (void)argv;

    SDL_SetMainReady();
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_GAMECONTROLLER | SDL_INIT_TIMER) < 0) {
        printf("[mmps5-smoke] SDL_Init failed: %s\n", SDL_GetError());
        return 1;
    }
    SDL_GetDesktopDisplayMode(0, &mode);
    status("SDL %d.%d.%d video driver '%s', display %dx%d @ %d Hz", SDL_MAJOR_VERSION, SDL_MINOR_VERSION,
           SDL_PATCHLEVEL, SDL_GetCurrentVideoDriver(), mode.w, mode.h, mode.refresh_rate);

    /* Same request as OpenEnroth's OpenGLRenderer with OE_GL33. */
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MAJOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_MINOR_VERSION, 3);
    SDL_GL_SetAttribute(SDL_GL_CONTEXT_PROFILE_MASK, SDL_GL_CONTEXT_PROFILE_CORE);
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 24);
    SDL_GL_SetAttribute(SDL_GL_STENCIL_SIZE, 8);

    window = SDL_CreateWindow("MM-PS5 smoke test", SDL_WINDOWPOS_UNDEFINED, SDL_WINDOWPOS_UNDEFINED, mode.w, mode.h,
                              SDL_WINDOW_OPENGL);
    if (!window) {
        printf("[mmps5-smoke] SDL_CreateWindow failed: %s\n", SDL_GetError());
        return 1;
    }
    context = SDL_GL_CreateContext(window);
    if (!context) {
        printf("[mmps5-smoke] SDL_GL_CreateContext failed: %s\n", SDL_GetError());
        return 1;
    }

#define LOAD(type, name) p_##name = (type)SDL_GL_GetProcAddress(#name);
    GL_FUNCTIONS(LOAD)
#undef LOAD
#define CHECK(type, name) if (!p_##name) { printf("[mmps5-smoke] missing GL function %s\n", #name); return 1; }
    GL_FUNCTIONS(CHECK)
#undef CHECK

    SDL_GL_GetDrawableSize(window, &screenW, &screenH);
    p_glGetIntegerv(GL_MAJOR_VERSION, &major);
    p_glGetIntegerv(GL_MINOR_VERSION, &minor);
    SDL_GL_GetAttribute(SDL_GL_DEPTH_SIZE, &depthBits);
    SDL_GL_GetAttribute(SDL_GL_STENCIL_SIZE, &stencilBits);
    status("GL_VERSION  %s", (const char *)p_glGetString(GL_VERSION));
    status("GL_RENDERER %s", (const char *)p_glGetString(GL_RENDERER));
    status("GLSL        %s", (const char *)p_glGetString(GL_SHADING_LANGUAGE_VERSION));
    status("Context: requested 3.3 core, got %d.%d; depth %d stencil %d; drawable %dx%d", major, minor, depthBits,
           stencilBits, screenW, screenH);
    status("Swap interval 1: %s   Swap interval -1 (adaptive): %s", SDL_GL_SetSwapInterval(1) == 0 ? "ok" : "FAIL",
           SDL_GL_SetSwapInterval(-1) == 0 ? "ok" : "rejected");
    SDL_GL_SetSwapInterval(1);

    vs = compile(GL_VERTEX_SHADER, vertexSource);
    fs = compile(GL_FRAGMENT_SHADER, fragmentSource);
    program = p_glCreateProgram();
    p_glAttachShader(program, vs);
    p_glAttachShader(program, fs);
    p_glLinkProgram(program);
    p_glGetProgramiv(program, GL_LINK_STATUS, &linked);
    status("GLSL 330 shader compile + link: %s", (vs && fs && linked) ? "ok" : "FAILED");

    p_glGenVertexArrays(1, &vao);
    p_glBindVertexArray(vao);
    p_glGenBuffers(1, &vbo);
    p_glBindBuffer(GL_ARRAY_BUFFER, vbo);
    p_glEnableVertexAttribArray(0);
    p_glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex), (void *)0);
    p_glEnableVertexAttribArray(1);
    p_glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(Vertex), (void *)(2 * sizeof(float)));
    p_glEnable(GL_BLEND);
    p_glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    p_glViewport(0, 0, screenW, screenH);

    lastTick = fpsTick = SDL_GetTicks64();
    for (;;) {
        SDL_Event event;
        while (SDL_PollEvent(&event)) {
            if (event.type == SDL_CONTROLLERDEVICEADDED && !pad) {
                pad = SDL_GameControllerOpen(event.cdevice.which);
                if (pad) {
                    char *mapping = SDL_GameControllerMapping(pad);
                    snprintf(padLine, sizeof(padLine), "Controller: '%s'  type %d  player %d",
                             SDL_GameControllerName(pad), (int)SDL_GameControllerGetType(pad),
                             SDL_GameControllerGetPlayerIndex(pad));
                    snprintf(featureLine, sizeof(featureLine),
                             "Touchpads %d   gyro %s   accel %s   rumble %s   LED %s",
                             SDL_GameControllerGetNumTouchpads(pad),
                             SDL_GameControllerHasSensor(pad, SDL_SENSOR_GYRO) ? "yes" : "no",
                             SDL_GameControllerHasSensor(pad, SDL_SENSOR_ACCEL) ? "yes" : "no",
                             SDL_GameControllerHasRumble(pad) ? "yes" : "no",
                             SDL_GameControllerHasLED(pad) ? "yes" : "no");
                    printf("[mmps5-smoke] %s\n[mmps5-smoke] %s\n[mmps5-smoke] mapping: %s\n", padLine, featureLine,
                           mapping ? mapping : "(none)");
                    SDL_free(mapping);
                }
            } else if (event.type == SDL_CONTROLLERDEVICEREMOVED && pad &&
                       event.cdevice.which == SDL_JoystickInstanceID(SDL_GameControllerGetJoystick(pad))) {
                SDL_GameControllerClose(pad);
                pad = NULL;
                snprintf(padLine, sizeof(padLine), "Controller: disconnected");
                featureLine[0] = 0;
            } else if (event.type == SDL_CONTROLLERBUTTONDOWN && pad) {
                printf("[mmps5-smoke] button down %s\n",
                       SDL_GameControllerGetStringForButton((SDL_GameControllerButton)event.cbutton.button));
                if (event.cbutton.button == SDL_CONTROLLER_BUTTON_RIGHTSTICK) {
                    int result = SDL_GameControllerRumble(pad, 0xC000, 0x6000, 400);
                    snprintf(actionLine, sizeof(actionLine), "Rumble request returned %d %s", result,
                             result == 0 ? "" : SDL_GetError());
                } else if (event.cbutton.button == SDL_CONTROLLER_BUTTON_LEFTSTICK) {
                    static const Uint8 colors[4][3] = {{255, 0, 0}, {0, 255, 0}, {0, 80, 255}, {255, 200, 87}};
                    int result;
                    ledIndex = (ledIndex + 1) % 4;
                    result = SDL_GameControllerSetLED(pad, colors[ledIndex][0], colors[ledIndex][1], colors[ledIndex][2]);
                    snprintf(actionLine, sizeof(actionLine), "Light bar color %d request returned %d %s", ledIndex,
                             result, result == 0 ? "" : SDL_GetError());
                }
            }
        }

        vertexCount = 0;
        pushText(60, 40, 5, YELLOW, "MM-PS5 SMOKE TEST");
        pushText(60, 100, 3, GREY, "Close with the PS button > Close Application");
        for (int i = 0; i < statusCount; i++)
            pushText(60, 150 + i * 36, 3, WHITE, statusLines[i]);

        {
            char line[200];
            snprintf(line, sizeof(line), "Frame %u   %.1f FPS   last GL error 0x%04x", frames, fps, lastError);
            pushText(60, 150 + statusCount * 36, 3, lastError == GL_NO_ERROR ? GREEN : RED, line);
        }
        pushText(60, 560, 3, pad ? GREEN : RED, padLine);
        pushText(60, 596, 3, WHITE, featureLine);
        pushText(60, 632, 3, YELLOW, actionLine);

        if (pad) {
            drawStick(220, 850, 120, SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTX),
                      SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_LEFTY), "Left stick");
            drawStick(560, 850, 120, SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_RIGHTX),
                      SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_RIGHTY), "Right stick");
            drawTrigger(760, 730, 50, 240, SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_TRIGGERLEFT), "L2");
            drawTrigger(860, 730, 50, 240, SDL_GameControllerGetAxis(pad, SDL_CONTROLLER_AXIS_TRIGGERRIGHT), "R2");
            for (int b = 0; b < SDL_CONTROLLER_BUTTON_MAX; b++) {
                const char *name = SDL_GameControllerGetStringForButton((SDL_GameControllerButton)b);
                int down = SDL_GameControllerGetButton(pad, (SDL_GameControllerButton)b);
                float bx = 1000 + (float)(b % 5) * 175;
                float by = 680 + (float)(b / 5) * 70;
                pushRect(bx, by, 165, 56, down ? GREEN : DIM);
                pushText(bx + 8, by + 20, 2, down ? DIM : WHITE, name ? name : "?");
            }
        }

        p_glClearColor(0.07f, 0.07f, 0.10f + 0.05f * (float)((frames / 30) % 2), 1.0f);
        p_glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        p_glUseProgram(program);
        p_glBindVertexArray(vao);
        p_glBindBuffer(GL_ARRAY_BUFFER, vbo);
        p_glBufferData(GL_ARRAY_BUFFER, (GLsizeiptr)(vertexCount * (int)sizeof(Vertex)), vertices, GL_STREAM_DRAW);
        p_glDrawArrays(GL_TRIANGLES, 0, vertexCount);
        {
            GLenum error = p_glGetError();
            if (error != GL_NO_ERROR)
                lastError = error;
        }
        SDL_GL_SwapWindow(window);

        frames++;
        fpsFrames++;
        lastTick = SDL_GetTicks64();
        if (lastTick - fpsTick >= 1000) {
            fps = (float)fpsFrames * 1000.0f / (float)(lastTick - fpsTick);
            fpsFrames = 0;
            fpsTick = lastTick;
        }
    }
}
