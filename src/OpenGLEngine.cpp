#define MINIAUDIO_IMPLEMENTATION
#include "external/miniaudio.h"

#include "OpenGLEngine.hpp"
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include "Enemy.hpp"
#include <glm/gtc/type_ptr.hpp>
#include <algorithm>
#include <cstddef>
#include <fstream>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <utility>
#include <vector>

// ── Helpery ──
static GLuint compileShader(GLenum type, const std::string& source) {
    const GLuint shader = glCreateShader(type);
    if (shader == 0) {
        throw std::runtime_error("OpenGL could not create a shader.");
    }

    const char* sourcePtr = source.c_str();
    glShaderSource(shader, 1, &sourcePtr, nullptr);
    glCompileShader(shader);

    GLint compiled = GL_FALSE;
    glGetShaderiv(shader, GL_COMPILE_STATUS, &compiled);
    if (compiled == GL_FALSE) {
        char log[1024]{};
        glGetShaderInfoLog(shader, static_cast<GLsizei>(sizeof(log)), nullptr, log);
        glDeleteShader(shader);
        throw std::runtime_error(std::string("Shader compile error: ") + log);
    }

    return shader;
}

static GLuint makeShader(const char* vertexSource, const char* fragmentSource) {
    const GLuint vertexShader = compileShader(GL_VERTEX_SHADER, vertexSource);
    GLuint fragmentShader = 0;
    GLuint program = 0;

    try {
        fragmentShader = compileShader(GL_FRAGMENT_SHADER, fragmentSource);
        program = glCreateProgram();
        if (program == 0) {
            throw std::runtime_error("OpenGL could not create a shader program.");
        }

        glAttachShader(program, vertexShader);
        glAttachShader(program, fragmentShader);
        glLinkProgram(program);

        GLint linked = GL_FALSE;
        glGetProgramiv(program, GL_LINK_STATUS, &linked);
        if (linked == GL_FALSE) {
            char log[1024]{};
            glGetProgramInfoLog(program, static_cast<GLsizei>(sizeof(log)), nullptr, log);
            throw std::runtime_error(std::string("Shader link error: ") + log);
        }
    } catch (...) {
        if (program != 0) {
            glDeleteProgram(program);
        }
        if (fragmentShader != 0) {
            glDeleteShader(fragmentShader);
        }
        glDeleteShader(vertexShader);
        throw;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);
    return program;
}

static void makeCubeVAO(GLuint& vao, GLuint& vbo) {
    struct V { glm::vec3 pos, nrm; glm::vec2 uv; };
    const float h = 0.5f;
    std::vector<V> verts = {
        // +X
        {{ h,-h,-h}, { 1,0,0}, {0,0}}, {{ h,-h, h}, { 1,0,0}, {1,0}}, {{ h, h, h}, { 1,0,0}, {1,1}},
        {{ h,-h,-h}, { 1,0,0}, {0,0}}, {{ h, h, h}, { 1,0,0}, {1,1}}, {{ h, h,-h}, { 1,0,0}, {0,1}},
        // -X
        {{-h,-h, h}, {-1,0,0}, {0,0}}, {{-h,-h,-h}, {-1,0,0}, {1,0}}, {{-h, h,-h}, {-1,0,0}, {1,1}},
        {{-h,-h, h}, {-1,0,0}, {0,0}}, {{-h, h,-h}, {-1,0,0}, {1,1}}, {{-h, h, h}, {-1,0,0}, {0,1}},
        // +Y
        {{-h, h, h}, {0, 1,0}, {0,0}}, {{ h, h, h}, {0, 1,0}, {1,0}}, {{ h, h,-h}, {0, 1,0}, {1,1}},
        {{-h, h, h}, {0, 1,0}, {0,0}}, {{ h, h,-h}, {0, 1,0}, {1,1}}, {{-h, h,-h}, {0, 1,0}, {0,1}},
        // -Y
        {{-h,-h,-h}, {0,-1,0}, {0,0}}, {{ h,-h,-h}, {0,-1,0}, {1,0}}, {{ h,-h, h}, {0,-1,0}, {1,1}},
        {{-h,-h,-h}, {0,-1,0}, {0,0}}, {{ h,-h, h}, {0,-1,0}, {1,1}}, {{-h,-h, h}, {0,-1,0}, {0,1}},
        // +Z
        {{-h,-h, h}, {0,0, 1}, {0,0}}, {{ h,-h, h}, {0,0, 1}, {1,0}}, {{ h, h, h}, {0,0, 1}, {1,1}},
        {{-h,-h, h}, {0,0, 1}, {0,0}}, {{ h, h, h}, {0,0, 1}, {1,1}}, {{-h, h, h}, {0,0, 1}, {0,1}},
        // -Z
        {{ h,-h,-h}, {0,0,-1}, {0,0}}, {{-h,-h,-h}, {0,0,-1}, {1,0}}, {{-h, h,-h}, {0,0,-1}, {1,1}},
        {{ h,-h,-h}, {0,0,-1}, {0,0}}, {{-h, h,-h}, {0,0,-1}, {1,1}}, {{ h, h,-h}, {0,0,-1}, {0,1}},
    };
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, verts.size() * sizeof(V), verts.data(), GL_STATIC_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(V), (void*)offsetof(V, pos));
    glEnableVertexAttribArray(1); glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(V), (void*)offsetof(V, nrm));
    glEnableVertexAttribArray(2); glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(V), (void*)offsetof(V, uv));
    glBindVertexArray(0);
}

static void makeQuadVAO(GLuint& vao, GLuint& vbo) {
    float verts[] = { -1,-1,  1,-1,  1, 1,  -1,-1,  1, 1, -1, 1 };
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glBindVertexArray(vao);
    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(verts), verts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0); glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2*sizeof(float), (void*)0);
    glBindVertexArray(0);
}

static GLuint makeTexture(const Image& image) {
    if (image.w <= 0 || image.h <= 0 || image.rgba.empty()) {
        return 0;
    }

    GLuint texture = 0;
    glGenTextures(1, &texture);
    if (texture == 0) {
        return 0;
    }

    glBindTexture(GL_TEXTURE_2D, texture);
    glTexImage2D(
        GL_TEXTURE_2D,
        0,
        GL_SRGB8_ALPHA8,
        image.w,
        image.h,
        0,
        GL_RGBA,
        GL_UNSIGNED_BYTE,
        image.rgba.data()
    );
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);
    glBindTexture(GL_TEXTURE_2D, 0);
    return texture;
}

// ── Shadery ──
static const char* SKYBOX_VS = R"(
#version 330 core
layout(location=0) in vec2 aPos;
out vec2 vNDC;
void main() { vNDC = aPos; gl_Position = vec4(aPos, 0.0, 1.0); }
)";

static const char* SKYBOX_FS = R"(
#version 330 core
in vec2 vNDC;
out vec4 FragColor;
uniform mat4 uInvViewProj;
uniform vec3 uCamPos;
uniform sampler2D uBg;
void main() {
    vec4 ndc = vec4(vNDC, 0.0, 1.0);
    vec4 world = uInvViewProj * ndc;
    vec3 dir = normalize(world.xyz / world.w - uCamPos);
    float yaw   = atan(dir.x, dir.z);
    float pitch = asin(clamp(dir.y, -1.0, 1.0));
    vec2 uv = vec2(yaw / (2.0 * 3.14159265) + 0.5, 0.5 - pitch / 3.14159265);
    FragColor = vec4(texture(uBg, uv).rgb, 1.0);
}
)";

static const char* SCENE_VS = R"(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNrm;
layout(location=2) in vec2 aUV;

uniform mat4 uVP;
uniform vec3 uOffset;
uniform vec3 uScale;
uniform vec4 uColor;

out vec3 vColor;
out vec3 vWorldPos;
out vec3 vNormal;
out vec2 vUV;
out float vChecker;

void main() {
    vec3 world = uOffset + aPos * uScale;
    vWorldPos = world;
    vNormal = aNrm;
    vChecker = uColor.a;
    vColor = uColor.rgb;
    
    if (abs(vNormal.y) > 0.5) {
        vUV = world.xz * 0.25;
    } else {
        if (abs(vNormal.x) > 0.5) vUV = world.zy * 0.25;
        else vUV = world.xy * 0.25;
    }
    
    gl_Position = uVP * vec4(world, 1.0);
}
)";


static const char* SCENE_FS = R"(
#version 330 core

in vec3 vColor;
in vec3 vWorldPos;
in vec3 vNormal;
in vec2 vUV;
in float vChecker;

out vec4 FragColor;

uniform sampler2D uFloorTex;
uniform sampler2D uWallTex;

void main() {
    bool isFloor = vChecker > 0.5;

    vec3 texColor = isFloor
        ? texture(uFloorTex, vUV).rgb
        : texture(uWallTex, vUV).rgb;

    // Odrzucamy pomarańczowe przebarwienia tekstury ścian,
    // ale zachowujemy szczegóły materiału.
    if (!isFloor) {
        float orange = smoothstep(
            texColor.b * 1.15 + 0.04,
            texColor.b * 1.55 + 0.12,
            texColor.r
        );

        texColor = mix(
            texColor,
            vec3(dot(texColor, vec3(0.299, 0.587, 0.114))),
            orange
        );
    }

    // Zabezpieczenie przed pustymi lub prawie czarnymi teksturami.
    if (dot(texColor, vec3(0.333)) < 0.025) {
        texColor = isFloor
            ? vec3(0.32, 0.34, 0.36)
            : vec3(0.12, 0.14, 0.16);
    }

    vec3 N = normalize(vNormal);
    vec3 L = normalize(vec3(-0.4, 0.85, 0.3));

    float diffuse = max(dot(N, L), 0.0);

    // Łagodne oświetlenie, zamiast jednolitego koloru kostek.
    float ambient = isFloor ? 0.42 : 0.32;
    float lighting = ambient + diffuse * 0.48;

    // Delikatne przyciemnienie dolnych części ścian.
    float heightShade = isFloor
        ? 1.0
        : mix(0.72, 1.0, smoothstep(-0.5, 2.0, vWorldPos.y));

    vec3 color = texColor * lighting * heightShade;

    // Subtelny chłodny odcień pasujący do Vcust Engine.
    color *= vec3(0.94, 0.98, 1.02);

    FragColor = vec4(color, 1.0);
}
)";


static const char* UI_VS = R"(
#version 330 core
layout(location=0) in vec2 aPos;
out vec2 vUV;
uniform vec2 uCenter;
uniform vec2 uHalfSize;
uniform float uMode;
uniform float uArrowCos;
uniform float uArrowSin;
const vec2 QUAD[6]  = vec2[](vec2(-1,-1), vec2(1,-1), vec2(1,1), vec2(-1,-1), vec2(1,1), vec2(-1,1));
const vec2 ARROW[3] = vec2[](vec2(0.0,-0.14), vec2(-0.09,0.10), vec2(0.09,0.10));
void main() {
    vec2 c;
    if (uMode < 0.5) {
        c = QUAD[gl_VertexID]; // Minimapa
    } else if (uMode < 1.5) {
        vec2 a = ARROW[gl_VertexID]; // Strzałka
        c = vec2(uArrowCos * a.x + uArrowSin * a.y, -uArrowSin * a.x + uArrowCos * a.y);
    } else {
        c = QUAD[gl_VertexID]; // Celownik (Crosshair)
    }
    vUV = c * 0.5;
    
    // Dla celownika (uMode == 2.0) używamy stałej pozycji (0,0) i małego rozmiaru
    if (uMode > 1.5) {
        gl_Position = vec4(c * 0.06, 0.0, 1.0); // Rysujemy na środku ekranu
    } else {
        gl_Position = vec4(uCenter.x + c.x * uHalfSize.x, uCenter.y - c.y * uHalfSize.y, 0.0, 1.0);
    }
}
)";


static const char* UI_FS = R"(
#version 330 core
in vec2 vUV;
out vec4 FragColor;

uniform sampler2D uMap;
uniform float uMode;
uniform vec2 uPlayerXZ;
uniform float uMapCos;
uniform float uMapSin;
uniform float uWorldRadius;
uniform vec2 uMapOrigin;
uniform float uMapWorldSize;

void main() {
    // CROSSHAIR — minimalistyczny błękit w stylu Portal
    if (uMode > 1.5) {
        // Obsługa obu typowych zakresów UV.
        vec2 p = abs(vUV);

        bool verticalArm =
            p.x < 0.035 &&
            p.y > 0.10 &&
            p.y < 0.65;

        bool horizontalArm =
            p.y < 0.035 &&
            p.x > 0.10 &&
            p.x < 0.65;

        if (verticalArm || horizontalArm) {
            FragColor = vec4(0.38, 0.80, 1.0, 1.0);
        } else {
            discard;
        }

        return;
    }

    // Kształt okrągłej minimapy
    if (length(vUV) > 0.5) discard;

    // Znacznik gracza
    if (uMode > 0.5) {
        FragColor = vec4(0.30, 0.76, 1.0, 1.0);
        return;
    }

    // Tekstura mapy
    vec2 l = vUV * 2.0 * uWorldRadius;
    vec2 w = uPlayerXZ + vec2(
        uMapCos * l.x + uMapSin * l.y,
        -uMapSin * l.x + uMapCos * l.y
    );

    vec2 t = (w - uMapOrigin) / uMapWorldSize;
    vec4 tex = texture(uMap, clamp(t, 0.0, 1.0));

    float inside =
        (all(greaterThanEqual(t, vec2(0.0))) &&
         all(lessThanEqual(t, vec2(1.0))))
        ? tex.a : 0.0;

    vec3 col = mix(vec3(0.04, 0.05, 0.07), tex.rgb, inside);
    col = mix(col, vec3(0.9), smoothstep(0.455, 0.47, length(vUV)));

    FragColor = vec4(col, 0.85);
}
)";


static const char* GUN_VS = R"(
#version 330 core
layout(location=0) in vec3 aPos;
layout(location=1) in vec3 aNrm;
layout(location=2) in vec2 aUV;

uniform mat4 uProj;
uniform mat4 uView;      
uniform vec3 uGunOffset; 
uniform mat4 uGunRot;    

out vec3 vNormal;
out vec2 vUV;

void main() {
    vec4 localPos = uGunRot * vec4(aPos * 0.001, 1.0);
    vec4 viewPos = localPos + vec4(uGunOffset, 0.0);
    gl_Position = uProj * viewPos;
    vNormal = mat3(uView) * mat3(uGunRot) * aNrm;
    vUV = aUV;
}
)";

static const char* GUN_FS = R"(
#version 330 core
in vec3 vNormal;
in vec2 vUV;
out vec4 FragColor;
uniform sampler2D uTex;
uniform float uFlash; 

void main() {
    vec3 texColor = texture(uTex, vUV).rgb;
    FragColor = vec4(texColor * (0.8 + uFlash * 2.0), 1.0);
}
)";

static const char* MUZZLE_VS = R"(
#version 330 core
layout(location=0) in vec3 aPos;
out vec2 vUV;
uniform mat4 uProj;
uniform vec3 uOffset;
uniform float uScale;

void main() {
    // aPos to wierzchołki trójkąta (-1,-1), (1,-1), (0,1)
    // Przekazujemy je jako UV (zakres -1 do 1)
    vUV = aPos.xy; 
    
    // Przesuwamy i skalujemy wierzchołki
    vec3 worldPos = uOffset + aPos * uScale;
    gl_Position = uProj * vec4(worldPos, 1.0);
}
)";

static const char* MUZZLE_FS = R"(
#version 330 core
in vec2 vUV; // Przekazujemy UV z wierzchołka, żeby wiedzieć, gdzie jesteśmy w trójkącie
out vec4 FragColor;

void main() {
    // vUV będzie w zakresie od -1 do 1 dla trójkąta (dzięki vertex shaderowi)
    // Liczymy odległość od środka (0,0)
    float dist = length(vUV);
    
    // Tworzymy bloom: im bliżej środka, tym jaśniej
    float intensity = exp(-dist * 4.0); // Funkcja wykładnicza daje miękkie przejście
    
    // Kolor: od białego w środku do pomarańczowego na brzegach
    vec3 color = mix(vec3(1.0, 0.4, 0.1), vec3(1.0, 1.0, 0.8), intensity);
    
    // Alfa zależy od intensywności (żeby brzegi były przezroczyste)
    FragColor = vec4(color * intensity, intensity);
}
)";

static const char* MENU_VS = R"(
#version 330 core
layout(location=0) in vec2 aPos;
uniform vec2 uCenter;
uniform vec2 uHalfSize;
out vec2 vUV;
void main() {
    vUV = aPos * 0.5 + 0.5;
    gl_Position = vec4(uCenter + aPos * uHalfSize, 0.0, 1.0);
}
)";

static const char* MENU_FS = R"(
#version 330 core
in vec2 vUV;
out vec4 FragColor;
uniform sampler2D uTexture;
uniform vec4 uColor;
uniform float uUseTex;
void main() {
    if (uUseTex > 0.5) {
        // Odwrócenie osi Y — stbi ładuje od góry, OpenGL oczekuje od dołu
        vec2 uv = vec2(vUV.x, 1.0 - vUV.y);
        FragColor = texture(uTexture, uv);
    } else {
        FragColor = uColor;
    }
}
)";

OpenGLEngine::OpenGLEngine(
    const std::string& title,
    uint32_t width,
    uint32_t height,
    std::filesystem::path shaderDir
)
    : shaderDir_(std::move(shaderDir)) {
    if (!glfwInit()) {
        throw std::runtime_error("glfwInit failed");
    }

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    window_ = glfwCreateWindow(
        static_cast<int>(width),
        static_cast<int>(height),
        title.c_str(),
        nullptr,
        nullptr
    );
    if (!window_) {
        glfwTerminate();
        throw std::runtime_error("glfwCreateWindow failed");
    }

    glfwMakeContextCurrent(window_);
    if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) {
        glfwDestroyWindow(window_);
        window_ = nullptr;
        glfwTerminate();
        throw std::runtime_error("glad load failed");
    }

    glfwSwapInterval(1);
    setupGL();

    if (ma_engine_init(nullptr, &audioEngine_) == MA_SUCCESS) {
        audioInitialized_ = true;
        const auto soundPath =
            (shaderDir_.parent_path() / "content" / "sounds" / "shot.wav").string();

        if (ma_sound_init_from_file(
                &audioEngine_, soundPath.c_str(), 0, nullptr, nullptr, &shotSound_
            ) == MA_SUCCESS) {
            shotSoundInitialized_ = true;
        } else {
            std::cerr << "Nie można wczytać dźwięku: " << soundPath << '\n';
        }
    } else {
        std::cerr << "Nie można zainicjalizować silnika audio.\n";
    }
}

OpenGLEngine::~OpenGLEngine() {
    // All GL resources must be released while the context still exists.
    if (window_) {
        glfwMakeContextCurrent(window_);
    }

    if (audioInitialized_) {
        if (shotSoundInitialized_) {
            ma_sound_uninit(&shotSound_);
            shotSoundInitialized_ = false;
        }
        ma_engine_uninit(&audioEngine_);
        audioInitialized_ = false;
    }

    for (auto& button : menuButtons_) {
        if (button.textureID != 0) {
            const GLuint texture = static_cast<GLuint>(button.textureID);
            glDeleteTextures(1, &texture);
            button.textureID = 0;
        }
    }

    const GLuint textures[] = {
        bgTex_, mapTex_, floorTex_, wallTex_, menuBgTex_, gunTexture_
    };
    for (const GLuint texture : textures) {
        if (texture != 0) {
            glDeleteTextures(1, &texture);
        }
    }

    const GLuint programs[] = {
        skyboxShader_, sceneShader_, uiShader_, gunShader_, muzzleShader_, menuShader_
    };
    for (const GLuint program : programs) {
        if (program != 0) {
            glDeleteProgram(program);
        }
    }

    const GLuint vertexArrays[] = {skyboxVAO_, cubeVAO_, quadVAO_, muzzleVAO_};
    for (const GLuint vertexArray : vertexArrays) {
        if (vertexArray != 0) {
            glDeleteVertexArrays(1, &vertexArray);
        }
    }

    const GLuint buffers[] = {skyboxVBO_, cubeVBO_, quadVBO_, muzzleVBO_};
    for (const GLuint buffer : buffers) {
        if (buffer != 0) {
            glDeleteBuffers(1, &buffer);
        }
    }

    // Model's destructor runs after this destructor body; zero the handles first
    // so it does not try to delete them after the GLFW context has been destroyed.
    if (gunModel_.ebo != 0) {
        glDeleteBuffers(1, &gunModel_.ebo);
        gunModel_.ebo = 0;
    }
    if (gunModel_.vbo != 0) {
        glDeleteBuffers(1, &gunModel_.vbo);
        gunModel_.vbo = 0;
    }
    if (gunModel_.vao != 0) {
        glDeleteVertexArrays(1, &gunModel_.vao);
        gunModel_.vao = 0;
    }
    gunModel_.indexCount = 0;

    if (window_) {
        glfwDestroyWindow(window_);
        window_ = nullptr;
    }
    glfwTerminate();
}

void OpenGLEngine::setupGL() {
    skyboxShader_ = makeShader(SKYBOX_VS, SKYBOX_FS);
    sceneShader_  = makeShader(SCENE_VS, SCENE_FS);
    uiShader_     = makeShader(UI_VS, UI_FS);
    gunShader_    = makeShader(GUN_VS, GUN_FS);
    muzzleShader_ = makeShader(MUZZLE_VS, MUZZLE_FS);
    menuShader_ = makeShader(MENU_VS, MENU_FS);

    makeQuadVAO(skyboxVAO_, skyboxVBO_);
    makeQuadVAO(quadVAO_, quadVBO_);
    makeCubeVAO(cubeVAO_, cubeVBO_);

    // --- NOWY BLOK DLA MUZZLE FLASH ---
    glGenVertexArrays(1, &muzzleVAO_);
    glGenBuffers(1, &muzzleVBO_);
    glBindVertexArray(muzzleVAO_);
    glBindBuffer(GL_ARRAY_BUFFER, muzzleVBO_);
    
    // Używamy QUADA (6 wierzchołków) zamiast trójkąta, żeby bloom był okrągły
    float muzzleVerts[] = {
        -1.0f, -1.0f, 0.0f, 
         1.0f, -1.0f, 0.0f, 
         1.0f,  1.0f, 0.0f, 
        -1.0f, -1.0f, 0.0f, 
         1.0f,  1.0f, 0.0f, 
        -1.0f,  1.0f, 0.0f
    };
    glBufferData(GL_ARRAY_BUFFER, sizeof(muzzleVerts), muzzleVerts, GL_STATIC_DRAW);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void*)0);
    glBindVertexArray(0);
    
}

void OpenGLEngine::setSceneAssets(const Image& bg, const Image& map, const Image& floorTex, const Image& wallTex) {
    if (bgTex_) glDeleteTextures(1, &bgTex_);
    if (mapTex_) glDeleteTextures(1, &mapTex_);
    if (floorTex_) glDeleteTextures(1, &floorTex_);
    if (wallTex_) glDeleteTextures(1, &wallTex_);
    
    bgTex_ = makeTexture(bg);
    mapTex_ = makeTexture(map);
    floorTex_ = floorTex.rgba.empty() ? 0 : makeTexture(floorTex);
    wallTex_ = wallTex.rgba.empty() ? 0 : makeTexture(wallTex);
}

void OpenGLEngine::drawFrame(const Scene& scene, const glm::vec3& playerPos,
                             float playerYaw, const Minimap& minimap,
                             const Camera& camera, bool isShooting,
                             const std::vector<Enemy>& enemies) {
    int w, h;
    glfwGetFramebufferSize(window_, &w, &h);
    if (w == 0 || h == 0) { glfwWaitEvents(); return; }
    glViewport(0, 0, w, h);
    glClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    drawSkybox(camera, w, h);
    drawScene(scene, camera, playerPos);

    // --- RYSOWANIE WROGÓW (tuż po świecie 3D, przed bronią i UI) ---
    glEnable(GL_DEPTH_TEST);
    for (const auto& enemy : enemies) {
        enemy.draw(*this, camera);
    }

    drawGun(camera, w, h, isShooting);
    drawMinimap(scene, minimap, minimap.build(scene, playerPos, playerYaw, camera.yaw, w, h));
    renderHealthbar(w, h);
}

void OpenGLEngine::drawSkybox(const Camera& camera, int w, int h) {
    glDisable(GL_DEPTH_TEST);
    glDepthMask(GL_FALSE);
    glUseProgram(skyboxShader_);

    const float aspect = float(w) / float(h);
    glm::mat4 invVP = glm::inverse(camera.projection(aspect) * camera.view());
    glUniformMatrix4fv(glGetUniformLocation(skyboxShader_, "uInvViewProj"), 1, GL_FALSE, glm::value_ptr(invVP));
    glUniform3fv(glGetUniformLocation(skyboxShader_, "uCamPos"), 1, glm::value_ptr(camera.eyePosition()));
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, bgTex_);
    glUniform1i(glGetUniformLocation(skyboxShader_, "uBg"), 0);

    glBindVertexArray(quadVAO_);
    glDrawArrays(GL_TRIANGLES, 0, 6);
    glBindVertexArray(0);

    glDepthMask(GL_TRUE);
    glEnable(GL_DEPTH_TEST);
}

void OpenGLEngine::drawScene(const Scene& scene, const Camera& camera, const glm::vec3& playerPos) {
    glUseProgram(sceneShader_);
    int w, h;
    glfwGetFramebufferSize(window_, &w, &h);
    const float aspect = float(w) / float(h);
    const glm::mat4 vp = camera.projection(aspect) * camera.view();
    glUniformMatrix4fv(glGetUniformLocation(sceneShader_, "uVP"), 1, GL_FALSE, glm::value_ptr(vp));

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, floorTex_);
    glUniform1i(glGetUniformLocation(sceneShader_, "uFloorTex"), 0);
    
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, wallTex_);
    glUniform1i(glGetUniformLocation(sceneShader_, "uWallTex"), 1);

    auto drawCube = [&](const glm::vec3& mn, const glm::vec3& mx, const glm::vec4& color) {
        glm::vec3 center = (mn + mx) * 0.5f;
        glm::vec3 scale = (mx - mn);

        glUniform3fv(glGetUniformLocation(sceneShader_, "uOffset"), 1, glm::value_ptr(center));
        glUniform3fv(glGetUniformLocation(sceneShader_, "uScale"), 1, glm::value_ptr(scale));
        glUniform4fv(glGetUniformLocation(sceneShader_, "uColor"),  1, glm::value_ptr(color));
        glDrawArrays(GL_TRIANGLES, 0, 36);
    };

    glBindVertexArray(cubeVAO_);
    
    for (const Aabb& f : scene.floors) {
        drawCube({f.min.x, f.min.y - 0.02f, f.min.z},
                 {f.max.x, f.min.y,        f.max.z},
                 {0.3f, 0.3f, 0.3f, 1.0f});
    }
    
    if (scene.floors.empty()) {
        drawCube({scene.bounds.min.x, scene.bounds.min.y - 0.02f, scene.bounds.min.z},
                 {scene.bounds.max.x, scene.bounds.min.y,        scene.bounds.max.z},
                 {0.3f, 0.3f, 0.3f, 1.0f});
    }
             
    for (const Aabb& w : scene.walls)
        drawCube(w.min, w.max, {0.72f, 0.28f, 0.22f, 0.0f});
        
    if (!camera.firstPerson) {
        drawCube({playerPos.x - 0.25f, playerPos.y,        playerPos.z - 0.25f},
                 {playerPos.x + 0.25f, playerPos.y + 1.8f, playerPos.z + 0.25f},
                 {0.95f, 0.50f, 0.15f, 0.0f});
    }
    glBindVertexArray(0);
}

void OpenGLEngine::drawGun(const Camera& camera, int w, int h, bool isShooting) {
    // 1. Czyścimy bufor głębi, żeby broń zawsze była na wierzchu
    glClear(GL_DEPTH_BUFFER_BIT);
    glUseProgram(gunShader_);

    const double now = glfwGetTime();
    const float dt = lastFrameTime_ > 0.0
        ? std::clamp(static_cast<float>(now - lastFrameTime_), 0.0f, 0.05f)
        : (1.0f / 60.0f);
    lastFrameTime_ = now;

    // 2. Logika strzelania (Fire Rate - "piu piu piu")
    if (isShooting && fireCooldown_ <= 0.0f) {
        shotPulse_ = 1.0f;        
        fireCooldown_ = 0.1f;     // 10 strzałów na sekundę
        if (shotSoundInitialized_) {
            ma_sound_seek_to_pcm_frame(&shotSound_, 0);
            ma_sound_start(&shotSound_);
        }
    }
    if (fireCooldown_ > 0.0f) fireCooldown_ -= dt;
    
    // Wygaszanie impulsu (błysku)
    if (shotPulse_ > 0.0f) {
        shotPulse_ -= dt * 10.0f; 
        if (shotPulse_ < 0.0f) shotPulse_ = 0.0f;
    }

    // 3. Płynny recoil (Smooth Recoil)
    float targetRecoil = shotPulse_ > 0.5f ? 1.0f : 0.0f; 
    float speed = targetRecoil > currentRecoil_ ? 30.0f : 15.0f; 
    currentRecoil_ += (targetRecoil - currentRecoil_) * speed * dt;

    float recoilZ = currentRecoil_ * 0.08f; 
    float recoilY = currentRecoil_ * 0.03f;
    
    // Błysk (jasność broni) - tylko gdy strzelamy
    float flash = shotPulse_ > 0.0f ? 0.5f : 0.0f; 

    // 4. Macierze
    const float aspect = float(w) / float(h);
    glm::mat4 proj = camera.projection(aspect);
    glm::mat4 view = glm::mat4(glm::mat3(camera.view()));

    glUniformMatrix4fv(glGetUniformLocation(gunShader_, "uProj"), 1, GL_FALSE, glm::value_ptr(proj));
    glUniformMatrix4fv(glGetUniformLocation(gunShader_, "uView"), 1, GL_FALSE, glm::value_ptr(view));

    // Przesunięcie i obrót broni
    glUniform3f(glGetUniformLocation(gunShader_, "uGunOffset"), 0.25f, -0.25f + recoilY, -0.5f + recoilZ); 
    glm::mat4 gunRot = glm::rotate(glm::mat4(1.0f), glm::radians(180.f), glm::vec3(0.f, 1.f, 0.f)); 
    gunRot = glm::rotate(gunRot, glm::radians(currentRecoil_ * 3.0f), glm::vec3(1.f, 0.f, 0.f));
    glUniformMatrix4fv(glGetUniformLocation(gunShader_, "uGunRot"), 1, GL_FALSE, glm::value_ptr(gunRot));
    
    // Efekt rozjaśnienia broni
    glUniform1f(glGetUniformLocation(gunShader_, "uFlash"), flash);

    // 5. Rysowanie modelu broni
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, gunTexture_);
    glUniform1i(glGetUniformLocation(gunShader_, "uTex"), 0);

    glBindVertexArray(gunModel_.vao);
    glDrawElements(GL_TRIANGLES, gunModel_.indexCount, GL_UNSIGNED_INT, 0);
    glBindVertexArray(0);

    // 6. Rysowanie Muzzle Flash (Bloom)
    if (shotPulse_ > 0.0f) {
        glUseProgram(muzzleShader_);
        
        // Włączamy blending dla poświaty (efekt świecenia)
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE); // Additive blending
        
        // Pozycja błysku (przy lufie)
        glm::vec3 muzzleOffset = glm::vec3(0.25f, -0.15f + recoilY, -0.8f + recoilZ);
        glUniform3fv(glGetUniformLocation(muzzleShader_, "uOffset"), 1, glm::value_ptr(muzzleOffset));
        
        // Skalowanie błysku (rośnie i maleje)
        float flashScale = 0.03f + (shotPulse_ * 0.04f); 
        glUniform1f(glGetUniformLocation(muzzleShader_, "uScale"), flashScale);
        glUniformMatrix4fv(glGetUniformLocation(muzzleShader_, "uProj"), 1, GL_FALSE, glm::value_ptr(proj));

        glBindVertexArray(muzzleVAO_);
        glDisable(GL_CULL_FACE);
        
        // Rysujemy QUAD (6 wierzchołków)!
        glDrawArrays(GL_TRIANGLES, 0, 6);
        
        glDisable(GL_BLEND); // Wyłączamy blending po narysowaniu
        glBindVertexArray(0);
    }
}

void OpenGLEngine::setGunAssets(const std::string& objPath, const Image& texImage) {
    gunModel_ = Model();
    if (!gunModel_.load(objPath)) {
        std::cerr << "Failed to load gun model: " << objPath << '\n';
    }
    if (gunTexture_) glDeleteTextures(1, &gunTexture_);
    gunTexture_ = makeTexture(texImage);
}

void OpenGLEngine::drawMinimap(const Scene&, const Minimap&, const UiPush& radar) {
    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    // Aktywujemy jawnie jednostkę tekstury 0 przed użyciem UI
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, mapTex_);

    glUseProgram(uiShader_);

    // --- RYSOWANIE MINIMAPY ---
    glUniform2fv(glGetUniformLocation(uiShader_, "uCenter"), 1, glm::value_ptr(radar.center));
    glUniform2fv(glGetUniformLocation(uiShader_, "uHalfSize"), 1, glm::value_ptr(radar.halfSize));
    glUniform1f(glGetUniformLocation(uiShader_, "uArrowCos"), radar.arrowCos);
    glUniform1f(glGetUniformLocation(uiShader_, "uArrowSin"), radar.arrowSin);
    glUniform2fv(glGetUniformLocation(uiShader_, "uPlayerXZ"), 1, glm::value_ptr(radar.playerXZ));
    glUniform1f(glGetUniformLocation(uiShader_, "uMapCos"), radar.mapCos);
    glUniform1f(glGetUniformLocation(uiShader_, "uMapSin"), radar.mapSin);
    glUniform1f(glGetUniformLocation(uiShader_, "uWorldRadius"), radar.worldRadius);
    glUniform2fv(glGetUniformLocation(uiShader_, "uMapOrigin"), 1, glm::value_ptr(radar.mapOrigin));
    glUniform1f(glGetUniformLocation(uiShader_, "uMapWorldSize"), radar.mapWorldSize);
    glUniform1i(glGetUniformLocation(uiShader_, "uMap"), 0);

    // Tło minimapy (uMode = 0)
    glUniform1f(glGetUniformLocation(uiShader_, "uMode"), 0.0f);
    glBindVertexArray(quadVAO_);
    glDrawArrays(GL_TRIANGLES, 0, 6);

    // Strzałka gracza (uMode = 1)
    glUniform1f(glGetUniformLocation(uiShader_, "uMode"), 1.0f);
    glDrawArrays(GL_TRIANGLES, 0, 6); 

    // --- RYSOWANIE CELOWNIKA (CROSSHAIR) ---
    glUniform1f(glGetUniformLocation(uiShader_, "uMode"), 2.0f);
    glDrawArrays(GL_TRIANGLES, 0, 6); 

    glBindVertexArray(0);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}


void OpenGLEngine::renderHealthbar(int w, int h) {
    if (w <= 0 || h <= 0) return;

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    glUseProgram(menuShader_);
    glBindVertexArray(quadVAO_);

    // Vcust HUD: Half-Life 1 x Portal
    constexpr float marginX = 0.045f;
    constexpr float marginY = 0.065f;
    constexpr float barHalfW = 0.22f;
    constexpr float barHalfH = 0.010f;

    const float barCenterX = -1.0f + marginX + barHalfW;
    const float barCenterY = -1.0f + marginY;

    const float healthNorm =
        glm::clamp(health / 100.0f, 0.0f, 1.0f);

    glUniform1f(
        glGetUniformLocation(menuShader_, "uUseTex"),
        0.0f
    );

    // Ciemne, półprzezroczyste tło.
    glUniform2f(
        glGetUniformLocation(menuShader_, "uCenter"),
        barCenterX, barCenterY
    );

    glUniform2f(
        glGetUniformLocation(menuShader_, "uHalfSize"),
        barHalfW, barHalfH
    );

    glUniform4f(
        glGetUniformLocation(menuShader_, "uColor"),
        0.015f, 0.035f, 0.055f, 0.82f
    );

    glDrawArrays(GL_TRIANGLES, 0, 6);

    // Błękitne wypełnienie zależne od zdrowia.
    if (healthNorm > 0.001f) {
        const float fillHalfW = barHalfW * healthNorm;
        const float fillCenterX =
            -1.0f + marginX + fillHalfW;

        glUniform2f(
            glGetUniformLocation(menuShader_, "uCenter"),
            fillCenterX, barCenterY
        );

        glUniform2f(
            glGetUniformLocation(menuShader_, "uHalfSize"),
            fillHalfW, barHalfH * 0.65f
        );

        glUniform4f(
            glGetUniformLocation(menuShader_, "uColor"),
            0.25f, 0.72f, 1.0f, 0.95f
        );

        glDrawArrays(GL_TRIANGLES, 0, 6);
    }

    // Cienka linia Aperture nad paskiem.
    glUniform2f(
        glGetUniformLocation(menuShader_, "uCenter"),
        barCenterX, barCenterY + barHalfH * 0.72f
    );

    glUniform2f(
        glGetUniformLocation(menuShader_, "uHalfSize"),
        barHalfW, 0.0012f
    );

    glUniform4f(
        glGetUniformLocation(menuShader_, "uColor"),
        0.42f, 0.84f, 1.0f, 0.75f
    );

    glDrawArrays(GL_TRIANGLES, 0, 6);

    
    // 3. Licznik HP — cyfry siedmiosegmentowe w błękicie Portal
    glUniform1f(glGetUniformLocation(menuShader_, "uUseTex"), 0.0f);

    auto drawHpRect = [&](float x, float y, float halfW, float halfH) {
        glUniform2f(glGetUniformLocation(menuShader_, "uCenter"), x, y);
        glUniform2f(glGetUniformLocation(menuShader_, "uHalfSize"), halfW, halfH);
        glUniform4f(glGetUniformLocation(menuShader_, "uColor"),
                    0.38f, 0.80f, 1.0f, 1.0f);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    };

    // Segmenty: góra, prawy-góra, prawy-dół, dół,
    // lewy-dół, lewy-góra, środek.
    const bool digits[10][7] = {
        {1,1,1,1,1,1,0}, // 0
        {0,1,1,0,0,0,0}, // 1
        {1,1,0,1,1,0,1}, // 2
        {1,1,1,1,0,0,1}, // 3
        {0,1,1,0,0,1,1}, // 4
        {1,0,1,1,0,1,1}, // 5
        {1,0,1,1,1,1,1}, // 6
        {1,1,1,0,0,0,0}, // 7
        {1,1,1,1,1,1,1}, // 8
        {1,1,1,1,0,1,1}  // 9
    };

    auto drawHpDigit = [&](int digit, float x, float y) {
        const float hw = 0.011f;
        const float hh = 0.003f;
        const float vx = 0.013f;
        const float vy = 0.010f;
        const float vw = 0.003f;
        const float vh = 0.009f;

        const bool* s = digits[digit];

        if (s[0]) drawHpRect(x,      y + 0.020f, hw, hh);
        if (s[1]) drawHpRect(x + vx, y + vy,     vw, vh);
        if (s[2]) drawHpRect(x + vx, y - vy,     vw, vh);
        if (s[3]) drawHpRect(x,      y - 0.020f, hw, hh);
        if (s[4]) drawHpRect(x - vx, y - vy,     vw, vh);
        if (s[5]) drawHpRect(x - vx, y + vy,     vw, vh);
        if (s[6]) drawHpRect(x,      y,          hw, hh);
    };

    const int hp = static_cast<int>(
        glm::clamp(health, 0.0f, 100.0f)
    );

    const std::string hpText = std::to_string(hp);
    const float hpY = barCenterY;
    const float hpStartX = -0.5f;
    const float hpSpacing = 0.036f;

    for (size_t i = 0; i < hpText.size(); ++i) {
        drawHpDigit(
            hpText[i] - '0',
            hpStartX + static_cast<float>(i) * hpSpacing,
            hpY
        );
    }

    glBindVertexArray(0);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

void OpenGLEngine::drawEnemyCube(const glm::vec3& min, const glm::vec3& max, const glm::vec4& color, const Camera& camera) {
    glUseProgram(sceneShader_);
    int w, h;
    glfwGetFramebufferSize(window_, &w, &h);
    const float aspect = float(w) / float(h);
    const glm::mat4 vp = camera.projection(aspect) * camera.view();
    glUniformMatrix4fv(glGetUniformLocation(sceneShader_, "uVP"), 1, GL_FALSE, glm::value_ptr(vp));

    glm::vec3 center = (min + max) * 0.5f;
    glm::vec3 scale = (max - min);
    glUniform3fv(glGetUniformLocation(sceneShader_, "uOffset"), 1, glm::value_ptr(center));
    glUniform3fv(glGetUniformLocation(sceneShader_, "uScale"), 1, glm::value_ptr(scale));
    glUniform4fv(glGetUniformLocation(sceneShader_, "uColor"), 1, glm::value_ptr(color));

    glBindVertexArray(cubeVAO_);
    glDrawArrays(GL_TRIANGLES, 0, 36);
    glBindVertexArray(0);
}

void OpenGLEngine::drawMenu() {
    int w = 0, h = 0;
    glfwGetFramebufferSize(window_, &w, &h);
    if (w <= 0 || h <= 0) return;

    glDisable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glUseProgram(menuShader_);

    // Proporcje względem okna GLFW (NDC); halfSize.x korygowany pod aspect,
    // żeby przyciski wyglądały tak samo na laptopie i szerokim monitorze.
    const float aspect = float(w) / float(h);
    const float refAspect = 16.0f / 9.0f;
    const float xScale = refAspect / aspect;

    // Rysowanie tła (pełny ekran w NDC)
    if (menuBgTex_) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, menuBgTex_);
        glUniform1i(glGetUniformLocation(menuShader_, "uTexture"), 0);
        glUniform1f(glGetUniformLocation(menuShader_, "uUseTex"), 1.0f);
        glUniform2f(glGetUniformLocation(menuShader_, "uCenter"), 0.0f, 0.0f);
        glUniform2f(glGetUniformLocation(menuShader_, "uHalfSize"), 1.0f, 1.0f);
        glBindVertexArray(quadVAO_);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    } else {
        glClearColor(0.1f, 0.1f, 0.1f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT);
    }

    for (const auto& btn : menuButtons_) {
        if (btn.textureID != 0) {
            // Rysowanie z teksturą
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, btn.textureID);
            glUniform1i(glGetUniformLocation(menuShader_, "uTexture"), 0);
            glUniform1f(glGetUniformLocation(menuShader_, "uUseTex"), 1.0f);
        } else {
            // Rysowanie z kolorem
            glUniform1f(glGetUniformLocation(menuShader_, "uUseTex"), 0.0f);
            glUniform4fv(glGetUniformLocation(menuShader_, "uColor"), 1, glm::value_ptr(btn.color));
        }
        
        // Ustawiamy pozycję i rozmiar
        glUniform2f(glGetUniformLocation(menuShader_, "uCenter"), btn.center.x * xScale, btn.center.y);
        glUniform2f(glGetUniformLocation(menuShader_, "uHalfSize"), btn.halfSize.x * xScale, btn.halfSize.y);
        
        glBindVertexArray(quadVAO_);
        glDrawArrays(GL_TRIANGLES, 0, 6);
    }

    glBindVertexArray(0);
    glDisable(GL_BLEND);
    glEnable(GL_DEPTH_TEST);
}

void OpenGLEngine::loadMenu(const std::string& path) {
    for (auto& button : menuButtons_) {
        if (button.textureID != 0) {
            const GLuint texture = static_cast<GLuint>(button.textureID);
            glDeleteTextures(1, &texture);
            button.textureID = 0;
        }
    }
    menuButtons_.clear();
    std::ifstream file(path);
    if (!file) return;
    
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::istringstream ss(line);
        std::string type;
        ss >> type;

        if (type == "background") {
            std::string texPath;
            ss >> texPath;
            std::filesystem::path p(path);
            std::string fullPath = (p.parent_path() / texPath).string();
            Image bg = loadImageRGBA(fullPath);
            if (!bg.rgba.empty()) {
                if (menuBgTex_) glDeleteTextures(1, &menuBgTex_);
                menuBgTex_ = makeTexture(bg);
            }
        } else if (type == "button") {
            MenuButton btn;
            std::string quotedText;
            ss >> quotedText;
            
            if (!quotedText.empty() && quotedText.front() == '"') {
                while (quotedText.back() != '"' && !ss.eof()) {
                    std::string part;
                    ss >> part;
                    quotedText += " " + part;
                }
                btn.text = quotedText.substr(1, quotedText.length() - (quotedText.length() > 1 ? 2 : 1));
            } else {
                btn.text = quotedText;
            }

            // Wczytujemy współrzędne
            ss >> btn.center.x >> btn.center.y >> btn.halfSize.x >> btn.halfSize.y;

            // TERAZ deklarujemy i wczytujemy resztę linii
            std::string rest;
            std::getline(ss, rest);

            // Teraz parsujemy akcję
            if (rest.find("action:new_game") != std::string::npos) btn.action = "new_game";
            else if (rest.find("action:quit") != std::string::npos) btn.action = "quit";

            // Teraz parsujemy kolor
            if (rest.find("color:") != std::string::npos) {
                size_t cpos = rest.find("color:") + 6;
                std::istringstream css(rest.substr(cpos));
                css >> btn.color.r >> btn.color.g >> btn.color.b >> btn.color.a;
            } else {
                btn.color = glm::vec4(0.5f, 0.5f, 0.5f, 1.0f);
            }

            // TERAZ parsujemy teksturę (kolejność nie ma znaczenia, byle po deklaracji rest)
            if (rest.find("texture:") != std::string::npos) {
                size_t tpos = rest.find("texture:") + 8;
                std::string texPath = rest.substr(tpos);
                while (!texPath.empty() && (texPath.back() == ' ' || texPath.back() == '\r')) texPath.pop_back();
                
                std::filesystem::path p(path);
                std::string fullPath = (p.parent_path() / texPath).string();
                Image texImg = loadImageRGBA(fullPath);
                if (!texImg.rgba.empty()) {
                    btn.textureID = makeTexture(texImg);
                }
            }

            menuButtons_.push_back(btn);
        }
    }
}

bool OpenGLEngine::shouldClose() const {
    return glfwWindowShouldClose(window_);
}
