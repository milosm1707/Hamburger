#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <cmath>
#include <vector>
#include <chrono>
#include <thread>
#include "../Header/Util.h"

enum GameState { MENU, COOKING, ASSEMBLING, FINISHED };
enum Ingredient { BUN_BOT, PATTY, KETCHUP, MUSTARD, PICKLE, ONION, LETTUCE, CHEESE, TOMATO, BUN_TOP, ING_COUNT };

struct Vec3 { float x, y, z; };
struct Vec4 { float x, y, z, w; };
struct Mat4 { float m[16]; };
struct Mesh { unsigned vao = 0, vbo = 0; int vertexCount = 0; };
struct Model { Mesh mesh{}; unsigned texture = 0; bool useTexture = false, ownsMesh = true; };
struct Puddle { float x, y, z; bool isKetchup, active; };

GameState state = MENU;
int scrW = 1920, scrH = 1080, curIng = BUN_BOT, processed = 0, puddleCount = 0;
float cookProg = 0.0f, pattyCol[3] = { 0.8f, 0.5f, 0.4f };
bool falling = false, dripping = false, inBurger[ING_COUNT] = { false };
bool depthEnabled = true, cullEnabled = true, lightOn = true;
bool cameraActive = false, firstMouse = true;
Vec3 pattyPos{ 0.0f, 0.6f, 0.3f }, ingPos{ 0.0f, 0.7f, 0.2f }, fallPos{ 0.0f, 0.0f, 0.0f };
Vec3 camPos{ 0.0f, 1.2f, 2.8f }, lightPos{ 1.5f, 2.5f, 2.0f }, lightColor{ 1.0f, 0.9f, 0.8f };
int fallType = -1;
float camYaw = -90.0f, camPitch = -15.0f;
double lastMouseX = 0.0, lastMouseY = 0.0;
Puddle puddles[20];
unsigned tButton, tDone, tStudentInfo, tTable, tStove, tPlate, tKetchup, tMustard;
unsigned quadVAO = 0, quadVBO = 0, shader2D = 0, shader3D = 0;
int studentTexW = 0, studentTexH = 0;
float studentW = 0.0f, studentH = 0.0f;

float clampf(float v, float minV, float maxV) { return fmax(minV, fmin(maxV, v)); }
Vec3 addVec3(Vec3 a, Vec3 b) { return { a.x + b.x, a.y + b.y, a.z + b.z }; }
Vec3 subVec3(Vec3 a, Vec3 b) { return { a.x - b.x, a.y - b.y, a.z - b.z }; }
Vec3 mulVec3(Vec3 a, float s) { return { a.x * s, a.y * s, a.z * s }; }
float dotVec3(Vec3 a, Vec3 b) { return a.x * b.x + a.y * b.y + a.z * b.z; }
Vec3 crossVec3(Vec3 a, Vec3 b) { return { a.y * b.z - a.z * b.y, a.z * b.x - a.x * b.z, a.x * b.y - a.y * b.x }; }
Vec3 normalizeVec3(Vec3 v) {
    float len = std::sqrt(dotVec3(v, v));
    return len <= 0.00001f ? Vec3{ 0,0,0 } : Vec3{ v.x / len, v.y / len, v.z / len };
}

Mat4 mat4Identity() { Mat4 r{}; r.m[0] = r.m[5] = r.m[10] = r.m[15] = 1.0f; return r; }
Mat4 mat4Mul(const Mat4& a, const Mat4& b) {
    Mat4 r{};
    for (int col = 0; col < 4; col++)
        for (int row = 0; row < 4; row++)
            r.m[col * 4 + row] = a.m[row] * b.m[col * 4] + a.m[4 + row] * b.m[col * 4 + 1] +
            a.m[8 + row] * b.m[col * 4 + 2] + a.m[12 + row] * b.m[col * 4 + 3];
    return r;
}
Mat4 mat4Translate(Vec3 t) { Mat4 r = mat4Identity(); r.m[12] = t.x; r.m[13] = t.y; r.m[14] = t.z; return r; }
Mat4 mat4Scale(Vec3 s) { Mat4 r{}; r.m[0] = s.x; r.m[5] = s.y; r.m[10] = s.z; r.m[15] = 1.0f; return r; }
Mat4 mat4RotateX(float rad) {
    Mat4 r = mat4Identity(); float c = cosf(rad), s = sinf(rad);
    r.m[5] = c; r.m[6] = s; r.m[9] = -s; r.m[10] = c; return r;
}
Mat4 mat4Perspective(float fov, float aspect, float nearZ, float farZ) {
    Mat4 r{}; float f = 1.0f / tanf(fov * 0.5f);
    r.m[0] = f / aspect; r.m[5] = f; r.m[10] = (farZ + nearZ) / (nearZ - farZ);
    r.m[11] = -1.0f; r.m[14] = (2.0f * farZ * nearZ) / (nearZ - farZ); return r;
}
Mat4 mat4LookAt(Vec3 eye, Vec3 center, Vec3 up) {
    Vec3 f = normalizeVec3(subVec3(center, eye)), s = normalizeVec3(crossVec3(f, up)), u = crossVec3(s, f);
    Mat4 r = mat4Identity();
    r.m[0] = s.x; r.m[4] = s.y; r.m[8] = s.z; r.m[1] = u.x; r.m[5] = u.y; r.m[9] = u.z;
    r.m[2] = -f.x; r.m[6] = -f.y; r.m[10] = -f.z;
    r.m[12] = -dotVec3(s, eye); r.m[13] = -dotVec3(u, eye); r.m[14] = dotVec3(f, eye);
    return r;
}

void initQuad() {
    if (quadVAO) return;
    glGenVertexArrays(1, &quadVAO);
    glGenBuffers(1, &quadVBO);
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(float) * 16, nullptr, GL_DYNAMIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), 0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 2, GL_FLOAT, GL_FALSE, 4 * sizeof(float), (void*)(2 * sizeof(float)));
    glEnableVertexAttribArray(1);
}

void loadTex(unsigned& tex, const char* path) {
    tex = loadImageToTexture(path);
    glBindTexture(GL_TEXTURE_2D, tex);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

void drawQ(unsigned s, unsigned tex, float x, float y, float w, float h, float r, float g, float b, float a) {
    initQuad();
    float v[] = { x - w,y + h,0,0, x - w,y - h,0,1, x + w,y - h,1,1, x + w,y + h,1,0 };
    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(v), v);
    glUseProgram(s);
    glUniform4f(glGetUniformLocation(s, "uColor"), r, g, b, a);

    unsigned t = tex;
    bool dummy = !tex;
    if (dummy) {
        unsigned char wCol[4] = { 255,255,255,255 };
        glGenTextures(1, &t);
        glBindTexture(GL_TEXTURE_2D, t);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, wCol);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, t);
    glUniform1i(glGetUniformLocation(s, "uTex"), 0);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);
    if (dummy) glDeleteTextures(1, &t);
}

void drawBar(unsigned s, float prog) {
    drawQ(s, 0, 0.0f, 0.85f, 0.5f, 0.06f, 0.3f, 0.3f, 0.3f, 1.0f);
    float w = 0.5f * prog;
    drawQ(s, 0, -0.5f + w, 0.85f, w, 0.055f, 0.0f, 0.8f, 0.0f, 1.0f);
}

const unsigned char* getGlyph(char c) {
    static const unsigned char space[7] = { 0,0,0,0,0,0,0 };
    static const unsigned char A[7] = { 0x0E,0x11,0x11,0x1F,0x11,0x11,0x11 };
    static const unsigned char C[7] = { 0x0E,0x11,0x10,0x10,0x10,0x11,0x0E };
    static const unsigned char E[7] = { 0x1F,0x10,0x10,0x1E,0x10,0x10,0x1F };
    static const unsigned char I[7] = { 0x1F,0x04,0x04,0x04,0x04,0x04,0x1F };
    static const unsigned char J[7] = { 0x07,0x02,0x02,0x02,0x02,0x12,0x0C };
    static const unsigned char L[7] = { 0x10,0x10,0x10,0x10,0x10,0x10,0x1F };
    static const unsigned char M[7] = { 0x11,0x1B,0x15,0x11,0x11,0x11,0x11 };
    static const unsigned char O[7] = { 0x0E,0x11,0x11,0x11,0x11,0x11,0x0E };
    static const unsigned char S[7] = { 0x0F,0x10,0x10,0x0E,0x01,0x01,0x1E };
    static const unsigned char V[7] = { 0x11,0x11,0x11,0x11,0x11,0x0A,0x04 };
    static const unsigned char zero[7] = { 0x0E,0x11,0x13,0x15,0x19,0x11,0x0E };
    static const unsigned char two[7] = { 0x0E,0x11,0x01,0x02,0x04,0x08,0x1F };
    static const unsigned char eight[7] = { 0x0E,0x11,0x11,0x0E,0x11,0x11,0x0E };
    static const unsigned char slash[7] = { 0x01,0x02,0x04,0x08,0x10,0x00,0x00 };
    c = toupper(c);
    if (c == 'A') return A; if (c == 'C') return C; if (c == 'E') return E; if (c == 'I') return I;
    if (c == 'J') return J; if (c == 'L') return L; if (c == 'M') return M; if (c == 'O') return O;
    if (c == 'S') return S; if (c == 'V') return V; if (c == '0') return zero; if (c == '2') return two;
    if (c == '8') return eight; if (c == '/') return slash; return space;
}

unsigned createTextTexture(const char* text, int scale, int padding, int& outW, int& outH) {
    int len = strlen(text);
    outW = len * (5 * scale + padding) - padding;
    outH = 7 * scale;
    std::vector<unsigned char> pixels(outW * outH * 4, 0);
    for (int i = 0; i < len; i++) {
        const unsigned char* glyph = getGlyph(text[i]);
        int baseX = i * (5 * scale + padding);
        for (int y = 0; y < 7; y++)
            for (int x = 0; x < 5; x++)
                if (glyph[y] & (1 << (4 - x)))
                    for (int sy = 0; sy < scale; sy++)
                        for (int sx = 0; sx < scale; sx++) {
                            int idx = ((y * scale + sy) * outW + baseX + x * scale + sx) * 4;
                            pixels[idx] = pixels[idx + 1] = pixels[idx + 2] = pixels[idx + 3] = 255;
                        }
    }
    unsigned tex;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, outW, outH, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    return tex;
}

Mesh createMesh(const std::vector<Vertex3D>& vertices) {
    Mesh mesh{};
    if (vertices.empty()) return mesh;
    glGenVertexArrays(1, &mesh.vao);
    glGenBuffers(1, &mesh.vbo);
    glBindVertexArray(mesh.vao);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.vbo);
    glBufferData(GL_ARRAY_BUFFER, vertices.size() * sizeof(Vertex3D), vertices.data(), GL_STATIC_DRAW);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), (void*)0);
    glEnableVertexAttribArray(0);
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), (void*)(3 * sizeof(float)));
    glEnableVertexAttribArray(1);
    glVertexAttribPointer(2, 2, GL_FLOAT, GL_FALSE, sizeof(Vertex3D), (void*)(6 * sizeof(float)));
    glEnableVertexAttribArray(2);
    mesh.vertexCount = (int)vertices.size();
    return mesh;
}

void destroyMesh(Mesh& mesh) {
    if (mesh.vbo) glDeleteBuffers(1, &mesh.vbo);
    if (mesh.vao) glDeleteVertexArrays(1, &mesh.vao);
    mesh.vbo = mesh.vao = mesh.vertexCount = 0;
}

Mesh createQuadMesh() {
    std::vector<Vertex3D> v(6);
    v[0] = { -0.5f,0.0f,-0.5f, 0,1,0, 0,0 }; v[1] = { 0.5f,0.0f,-0.5f, 0,1,0, 1,0 };
    v[2] = { 0.5f,0.0f,0.5f, 0,1,0, 1,1 }; v[3] = { -0.5f,0.0f,-0.5f, 0,1,0, 0,0 };
    v[4] = { 0.5f,0.0f,0.5f, 0,1,0, 1,1 }; v[5] = { -0.5f,0.0f,0.5f, 0,1,0, 0,1 };
    return createMesh(v);
}

Mesh createCubeMesh() {
    std::vector<Vertex3D> v; v.reserve(36);
    auto addFace = [&](Vec3 n, Vec3 a, Vec3 b, Vec3 c, Vec3 d) {
        v.push_back({ a.x,a.y,a.z, n.x,n.y,n.z, 0,0 }); v.push_back({ b.x,b.y,b.z, n.x,n.y,n.z, 1,0 });
        v.push_back({ c.x,c.y,c.z, n.x,n.y,n.z, 1,1 }); v.push_back({ a.x,a.y,a.z, n.x,n.y,n.z, 0,0 });
        v.push_back({ c.x,c.y,c.z, n.x,n.y,n.z, 1,1 }); v.push_back({ d.x,d.y,d.z, n.x,n.y,n.z, 0,1 });
        };
    float s = 0.5f;
    addFace({ 0,0,1 }, { -s,-s,s }, { s,-s,s }, { s,s,s }, { -s,s,s });
    addFace({ 0,0,-1 }, { s,-s,-s }, { -s,-s,-s }, { -s,s,-s }, { s,s,-s });
    addFace({ 1,0,0 }, { s,-s,s }, { s,-s,-s }, { s,s,-s }, { s,s,s });
    addFace({ -1,0,0 }, { -s,-s,-s }, { -s,-s,s }, { -s,s,s }, { -s,s,-s });
    addFace({ 0,1,0 }, { -s,s,s }, { s,s,s }, { s,s,-s }, { -s,s,-s });
    addFace({ 0,-1,0 }, { -s,-s,-s }, { s,-s,-s }, { s,-s,s }, { -s,-s,s });
    return createMesh(v);
}

Mesh createCylinderMesh(int segments, float radius, float height) {
    std::vector<Vertex3D> v;
    float half = height * 0.5f;
    for (int i = 0; i < segments; i++) {
        float a0 = (float)i / segments * 6.28318f, a1 = (float)(i + 1) / segments * 6.28318f;
        float x0 = cosf(a0) * radius, z0 = sinf(a0) * radius, x1 = cosf(a1) * radius, z1 = sinf(a1) * radius;
        Vec3 n0 = normalizeVec3({ x0,0,z0 }), n1 = normalizeVec3({ x1,0,z1 });
        v.push_back({ x0,-half,z0, n0.x,n0.y,n0.z, 0,0 }); v.push_back({ x1,-half,z1, n1.x,n1.y,n1.z, 1,0 });
        v.push_back({ x1,half,z1, n1.x,n1.y,n1.z, 1,1 }); v.push_back({ x0,-half,z0, n0.x,n0.y,n0.z, 0,0 });
        v.push_back({ x1,half,z1, n1.x,n1.y,n1.z, 1,1 }); v.push_back({ x0,half,z0, n0.x,n0.y,n0.z, 0,1 });
        v.push_back({ 0,half,0, 0,1,0, 0.5f,0.5f });
        v.push_back({ x1,half,z1, 0,1,0, (x1 / radius + 1.0f) * 0.5f,(z1 / radius + 1.0f) * 0.5f });
        v.push_back({ x0,half,z0, 0,1,0, (x0 / radius + 1.0f) * 0.5f,(z0 / radius + 1.0f) * 0.5f });
        v.push_back({ 0,-half,0, 0,-1,0, 0.5f,0.5f });
        v.push_back({ x0,-half,z0, 0,-1,0, (x0 / radius + 1.0f) * 0.5f,(z0 / radius + 1.0f) * 0.5f });
        v.push_back({ x1,-half,z1, 0,-1,0, (x1 / radius + 1.0f) * 0.5f,(z1 / radius + 1.0f) * 0.5f });
    }
    return createMesh(v);
}

Mesh createConeMesh(int segments, float radius, float height) {
    std::vector<Vertex3D> v;
    float half = height * 0.5f;
    Vec3 tip{ 0, half, 0 }, baseCenter{ 0, -half, 0 };
    for (int i = 0; i < segments; i++) {
        float a0 = (float)i / segments * 6.28318f, a1 = (float)(i + 1) / segments * 6.28318f;
        Vec3 p0{ cosf(a0) * radius, -half, sinf(a0) * radius }, p1{ cosf(a1) * radius, -half, sinf(a1) * radius };
        Vec3 n = normalizeVec3(crossVec3(subVec3(p1, tip), subVec3(p0, tip)));
        v.push_back({ tip.x, tip.y, tip.z, n.x,n.y,n.z, 0.5f,1.0f });
        v.push_back({ p1.x, p1.y, p1.z, n.x,n.y,n.z, 1.0f,0.0f });
        v.push_back({ p0.x, p0.y, p0.z, n.x,n.y,n.z, 0.0f,0.0f });
        v.push_back({ baseCenter.x, baseCenter.y, baseCenter.z, 0,-1,0, 0.5f,0.5f });
        v.push_back({ p0.x, p0.y, p0.z, 0,-1,0, (p0.x / radius + 1.0f) * 0.5f,(p0.z / radius + 1.0f) * 0.5f });
        v.push_back({ p1.x, p1.y, p1.z, 0,-1,0, (p1.x / radius + 1.0f) * 0.5f,(p1.z / radius + 1.0f) * 0.5f });
    }
    return createMesh(v);
}

void setMat4(unsigned shader, const char* name, const Mat4& mat) {
    glUniformMatrix4fv(glGetUniformLocation(shader, name), 1, GL_FALSE, mat.m);
}
void setVec3(unsigned shader, const char* name, Vec3 v) {
    glUniform3f(glGetUniformLocation(shader, name), v.x, v.y, v.z);
}

void drawMesh(unsigned shader, const Mesh& mesh, unsigned tex, bool useTex, const Mat4& model, Vec4 tint) {
    glUseProgram(shader);
    setMat4(shader, "uModel", model);
    glUniform4f(glGetUniformLocation(shader, "uTint"), tint.x, tint.y, tint.z, tint.w);
    glUniform1i(glGetUniformLocation(shader, "uUseTex"), useTex);
    if (useTex) {
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, tex);
        glUniform1i(glGetUniformLocation(shader, "uTex"), 0);
    }
    glBindVertexArray(mesh.vao);
    glDrawArrays(GL_TRIANGLES, 0, mesh.vertexCount);
}

void drawBottle(unsigned shader, const Mesh& cylinderMesh, const Mesh& coneMesh, Vec3 pos, Vec3 scale, Vec4 tint) {
    drawMesh(shader, cylinderMesh, 0, false, mat4Mul(mat4Translate(pos), mat4Scale(scale)), tint);
    Vec3 coneScale{ scale.x * 0.9f, scale.y * 0.35f, scale.z * 0.9f };
    Vec3 conePos = pos; conePos.y -= (scale.y * 0.5f + coneScale.y * 0.5f);
    drawMesh(shader, coneMesh, 0, false, mat4Mul(mat4Translate(conePos), mat4Mul(mat4RotateX(3.14159f), mat4Scale(coneScale))), tint);
}

void moveIngredient(GLFWwindow* win, Vec3& pos, float dt) {
    float speed = 1.1f * dt;
    if (glfwGetKey(win, GLFW_KEY_W)) pos.z -= speed;
    if (glfwGetKey(win, GLFW_KEY_S)) pos.z += speed;
    if (glfwGetKey(win, GLFW_KEY_A)) pos.x -= speed;
    if (glfwGetKey(win, GLFW_KEY_D)) pos.x += speed;
    if (glfwGetKey(win, GLFW_KEY_SPACE)) pos.y += speed;
    if (glfwGetKey(win, GLFW_KEY_Z)) pos.y -= speed;
    pos.x = clampf(pos.x, -1.2f, 1.2f);
    pos.z = clampf(pos.z, -1.2f, 1.2f);
    pos.y = clampf(pos.y, 0.1f, 1.5f);
}

void moveCamera(GLFWwindow* win, float dt) {
    float speed = 1.5f * dt, yawRad = camYaw * 0.0174533f;
    Vec3 forward = normalizeVec3({ cosf(yawRad), 0.0f, sinf(yawRad) });
    Vec3 right = normalizeVec3(crossVec3(forward, { 0,1,0 }));
    if (glfwGetKey(win, GLFW_KEY_UP)) camPos = addVec3(camPos, mulVec3(forward, speed));
    if (glfwGetKey(win, GLFW_KEY_DOWN)) camPos = subVec3(camPos, mulVec3(forward, speed));
    if (glfwGetKey(win, GLFW_KEY_LEFT)) camPos = subVec3(camPos, mulVec3(right, speed));
    if (glfwGetKey(win, GLFW_KEY_RIGHT)) camPos = addVec3(camPos, mulVec3(right, speed));
}

void mouseClick(GLFWwindow* win, int btn, int act, int mods) {
    if (btn == GLFW_MOUSE_BUTTON_LEFT && act == GLFW_PRESS && state == MENU) {
        double x, y; glfwGetCursorPos(win, &x, &y);
        float xN = (x / scrW) * 2 - 1, yN = -((y / scrH) * 2 - 1);
        if (xN >= -0.3f && xN <= 0.3f && yN >= -0.15f && yN <= 0.15f) state = COOKING;
    }
}

void mouseMove(GLFWwindow* win, double xpos, double ypos) {
    if (!cameraActive) return;
    if (firstMouse) { lastMouseX = xpos; lastMouseY = ypos; firstMouse = false; }
    float xoffset = (xpos - lastMouseX) * 0.1f, yoffset = (lastMouseY - ypos) * 0.1f;
    lastMouseX = xpos; lastMouseY = ypos;
    camYaw += xoffset; camPitch = clampf(camPitch + yoffset, -89.0f, 89.0f);
}

void keyPress(GLFWwindow* win, int key, int sc, int act, int mods) {
    if (key == GLFW_KEY_ESCAPE && act == GLFW_PRESS) glfwSetWindowShouldClose(win, GLFW_TRUE);
    if (key == GLFW_KEY_F1 && act == GLFW_PRESS) depthEnabled = !depthEnabled;
    if (key == GLFW_KEY_F2 && act == GLFW_PRESS) cullEnabled = !cullEnabled;
    if (key == GLFW_KEY_L && act == GLFW_PRESS) lightOn = !lightOn;
    if (key == GLFW_KEY_X && state == ASSEMBLING) {
        if (act == GLFW_PRESS && (curIng == KETCHUP || curIng == MUSTARD)) dripping = true;
        if (act == GLFW_RELEASE && dripping) {
            falling = true; fallPos = ingPos; fallPos.y -= 0.1f;
            fallType = (curIng == KETCHUP) ? 0 : 1; dripping = false;
        }
    }
}

bool loadModel(const char* objPath, const char* texPath, Model& model, const Mesh& fallback) {
    std::vector<Vertex3D> verts;
    if (loadObjMesh(objPath, verts)) {
        model.mesh = createMesh  (verts); model.ownsMesh = true;
        if (texPath) { model.texture = loadImageToTexture(texPath); model.useTexture = model.texture != 0; }
        return true;
    }
    model.mesh = fallback; model.ownsMesh = false; model.texture = 0; model.useTexture = false;
    return false;
}

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    GLFWmonitor* mon = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(mon);
    scrW = mode->width; scrH = mode->height;
    GLFWwindow* win = glfwCreateWindow(scrW, scrH, "Hamburger Simulator 3D", mon, NULL);
    if (!win) return endProgram("Prozor greska");
    glfwMakeContextCurrent(win);
    glfwSetMouseButtonCallback(win, mouseClick);
    glfwSetKeyCallback(win, keyPress);
    glfwSetCursorPosCallback(win, mouseMove);
    glfwSwapInterval(0);
    if (glewInit() != GLEW_OK) return endProgram("GLEW greska");
    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    GLFWcursor* cur = loadImageToCursor("res/spatula.png");
    if (cur) glfwSetCursor(win, cur);

    loadTex(tButton, "res/button.png"); loadTex(tDone, "res/done.png"); loadTex(tTable, "res/table.png");
    loadTex(tStove, "res/stove.png"); loadTex(tPlate, "res/plate.png");
    loadTex(tKetchup, "res/ketchup.png"); loadTex(tMustard, "res/mustard.png");

    shader2D = createShader("Shaders/basic.vert", "Shaders/basic.frag");
    shader3D = createShader("Shaders/basic3d.vert", "Shaders/basic3d.frag");

    for (int i = 0; i < 20; i++) puddles[i].active = false;
    glClearColor(0.85f, 0.9f, 0.95f, 1.0f);

    tStudentInfo = createTextTexture("Milos Milosavljevic SV80/2022", 3, 2, studentTexW, studentTexH);
    studentW = studentTexW / (float)scrW; studentH = studentTexH / (float)scrH;

    Mesh cubeMesh = createCubeMesh(), quadMesh = createQuadMesh();
    Mesh cylinderMesh = createCylinderMesh(24, 0.5f, 1.0f), coneMesh = createConeMesh(24, 0.45f, 0.7f);

    Model bunBot, bunTop, patty, ketchup, mustard, pickle, onion, lettuce, cheese, tomato;
    loadModel("res/models/bun_bottom.obj", "res/models/bun_bottom.png", bunBot, cylinderMesh);
    loadModel("res/models/bun_top.obj", "res/models/bun_top.jpg", bunTop, cylinderMesh);
    loadModel("res/models/patty.obj", "res/models/patty.jpg", patty, cylinderMesh);
    loadModel("res/models/ketchup.obj", "res/models/ketchup.png", ketchup, cylinderMesh);
    loadModel("res/models/mustard.obj", "res/models/mustard.png", mustard, cylinderMesh);
    loadModel("res/models/pickle.obj", "res/models/pickle.png", pickle, cylinderMesh);
    loadModel("res/models/onion.obj", "res/models/onion.png", onion, cylinderMesh);
    loadModel("res/models/lettuce.obj", "res/models/lettuce.png", lettuce, cylinderMesh);
    loadModel("res/models/cheese.obj", "res/models/cheese.png", cheese, cylinderMesh);
    loadModel("res/models/tomato.obj", "res/models/tomato.png", tomato, cylinderMesh);

    std::vector<Model*> models = { &bunBot, &patty, &ketchup, &mustard, &pickle, &onion, &lettuce, &cheese, &tomato, &bunTop };
    float bunScale = 0.0065f;
    std::vector<Vec3> scales = {
        {bunScale, 0.0026f, bunScale}, {bunScale * 50 * 0.85f, 0.01f, bunScale * 50 * 0.85f}, {0.2f, 0.35f, 0.2f}, {0.2f, 0.35f, 0.2f},
        {bunScale * 300 * 0.65f, 0.002f * 200, bunScale * 300 * 0.65f}, {bunScale * 50 * 0.7f, 0.0012f, bunScale * 50 * 0.7f},
        {bunScale * 300 * 0.92f, 0.0008f, bunScale * 300 * 0.92f}, {bunScale * 0.85f * 2, 0.006f * 2, bunScale * 0.85f * 2},
        {bunScale * 1000 * 0.75f, 0.0015f, bunScale * 1000 * 0.75f}, {bunScale, 0.0026f, bunScale}
    };
    std::vector<float> heights = { 0.009f, 0.07f, 0.02f, 0.02f, 0.003f, 0.004f, 0.003f, 0.002f, 0.005f, 0.009f };
    std::vector<float> stackOffsets(ING_COUNT, 0.0f);
    stackOffsets[BUN_BOT] = 0.1f; stackOffsets[BUN_TOP] = 0.18f;

    Vec3 basePattyScale = scales[PATTY];
    float basePattyHeight = heights[PATTY];
    Vec3 rawPattyScale{ basePattyScale.x * 1.2f, basePattyScale.y * 1.5f, basePattyScale.z * 1.2f };
    Vec3 cookedPattyScale = basePattyScale;
    scales[PATTY] = cookedPattyScale; heights[PATTY] = basePattyHeight;

    Vec3 tableTopPos{ 0,-0.05f,0 }, tableTopSize{ 2.8f, 0.1f, 1.6f }, legSize{ 0.15f, 0.7f, 0.15f };
    Vec3 stovePos{ 0, 0.15f, -0.9f }, stoveSize{ 1.3f, 0.3f, 0.8f };
    Vec3 platePos{ 0, 0.05f, 0 }, wastePos{ 0.9f, 0.05f, 0 }, wasteSize{ 0.35f, 0.1f, 0.35f };
    float plateRadius = 0.35f;

    auto lastTime = std::chrono::steady_clock::now();

    while (!glfwWindowShouldClose(win)) {
        auto frameStart = std::chrono::steady_clock::now();
        float deltaTime = std::chrono::duration<float>(frameStart - lastTime).count();
        lastTime = frameStart;

        bool allowCamera = (state == COOKING || state == ASSEMBLING);
        if (allowCamera != cameraActive) {
            cameraActive = allowCamera; firstMouse = true;
            glfwSetInputMode(win, GLFW_CURSOR, cameraActive ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
        }
        if (cameraActive) moveCamera(win, deltaTime);

        if (depthEnabled) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
        if (cullEnabled) { glEnable(GL_CULL_FACE); glCullFace(GL_BACK); }
        else glDisable(GL_CULL_FACE);

        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        float yawRad = camYaw * 0.0174533f, pitchRad = camPitch * 0.0174533f;
        Vec3 camFront = normalizeVec3({ cosf(pitchRad) * cosf(yawRad), sinf(pitchRad), cosf(pitchRad) * sinf(yawRad) });
        Mat4 view = mat4LookAt(camPos, addVec3(camPos, camFront), { 0,1,0 });
        Mat4 proj = mat4Perspective(0.785398f, (float)scrW / scrH, 0.1f, 100.0f);

        glUseProgram(shader3D);
        setMat4(shader3D, "uView", view); setMat4(shader3D, "uProj", proj);
        setVec3(shader3D, "uLightPos", lightPos); setVec3(shader3D, "uLightColor", lightColor);
        setVec3(shader3D, "uViewPos", camPos);
        glUniform1i(glGetUniformLocation(shader3D, "uLightOn"), lightOn);

        drawMesh(shader3D, cubeMesh, tTable, true, mat4Mul(mat4Translate(tableTopPos), mat4Scale(tableTopSize)), { 1,1,1,1 });

        float legX = tableTopSize.x * 0.5f - legSize.x * 0.5f, legZ = tableTopSize.z * 0.5f - legSize.z * 0.5f;
        float legY = tableTopPos.y - tableTopSize.y * 0.5f - legSize.y * 0.5f;
        Vec3 legPositions[] = { { legX, legY, legZ }, { -legX, legY, legZ }, { legX, legY, -legZ }, { -legX, legY, -legZ } };
        for (auto& lp : legPositions)
            drawMesh(shader3D, cubeMesh, 0, false, mat4Mul(mat4Translate(lp), mat4Scale(legSize)), { 0.35f,0.22f,0.12f,1 });

        drawMesh(shader3D, cubeMesh, tStove, true, mat4Mul(mat4Translate(stovePos), mat4Scale(stoveSize)), { 1,1,1,1 });

        bool restoreBlend = glIsEnabled(GL_BLEND);
        if (restoreBlend) glDisable(GL_BLEND);
        drawMesh(shader3D, cylinderMesh, tPlate, true, mat4Mul(mat4Translate(platePos), mat4Scale({ plateRadius * 2.0f, 0.08f, plateRadius * 2.0f })), { 1,1,1,1 });
        drawMesh(shader3D, cubeMesh, tPlate, true, mat4Mul(mat4Translate(wastePos), mat4Scale(wasteSize)), { 1,1,1,1 });
        if (restoreBlend) glEnable(GL_BLEND);

        for (int i = 0; i < puddleCount; i++) {
            if (!puddles[i].active || inBurger[BUN_TOP]) continue;
            bool restoreCull = cullEnabled;
            if (restoreCull) glDisable(GL_CULL_FACE);
            drawMesh(shader3D, quadMesh, puddles[i].isKetchup ? tKetchup : tMustard, true,
                mat4Mul(mat4Translate({ puddles[i].x, puddles[i].y, puddles[i].z }), mat4Scale({ 0.15f, 1.0f, 0.15f })), { 1,1,1,1 });
            if (restoreCull) glEnable(GL_CULL_FACE);
        }

        if (state == COOKING) {
            moveIngredient(win, pattyPos, deltaTime);
            float stoveTop = stovePos.y + stoveSize.y * 0.5f, pattyHalf = rawPattyScale.y * 0.5f;
            if (pattyPos.y < stoveTop + pattyHalf) pattyPos.y = stoveTop + pattyHalf;

            if (fabsf(pattyPos.x - stovePos.x) <= stoveSize.x * 0.5f && fabsf(pattyPos.z - stovePos.z) <= stoveSize.z * 0.5f &&
                pattyPos.y <= stoveTop + pattyHalf + 0.01f) {
                cookProg += 0.25f * deltaTime;
                if (cookProg > 1.0f) cookProg = 1.0f;
            }
            float r = 0.8f - cookProg * 0.3f, g = 0.5f - cookProg * 0.15f, b = 0.4f - cookProg * 0.25f;
            if (cookProg >= 1.0f) { pattyCol[0] = r; pattyCol[1] = g; pattyCol[2] = b; }

            Vec3 cookScale = (cookProg >= 1.0f) ? cookedPattyScale : rawPattyScale;
            bool restoreCull = cullEnabled;
            if (restoreCull) glDisable(GL_CULL_FACE);
            if (cookProg >= 1.0f && patty.ownsMesh)
                drawMesh(shader3D, patty.mesh, patty.texture, patty.useTexture, mat4Mul(mat4Translate(pattyPos), mat4Scale(cookScale)), { r,g,b,1 });
            else
                drawMesh(shader3D, cylinderMesh, 0, false, mat4Mul(mat4Translate(pattyPos), mat4Scale(cookScale)), { r,g,b,1 });
            if (restoreCull) glEnable(GL_CULL_FACE);

            if (cookProg >= 1.0f) { state = ASSEMBLING; pattyPos = ingPos = { 0, 0.7f, 0.2f }; }
        }
        else if (state == ASSEMBLING) {
            float yOff = 0, stackBaseY = platePos.y + 0.14f, stackHeightFactor = 0.85f;
            for (int i = 0; i < ING_COUNT; i++) {
                if (!inBurger[i] || i == KETCHUP || i == MUSTARD) continue;
                Vec3 pos = platePos; pos.y = stackBaseY + yOff + heights[i] * 0.5f * stackHeightFactor + stackOffsets[i];
                Vec4 tint{ 1,1,1,1 };
                if (i == PATTY) tint = { pattyCol[0], pattyCol[1], pattyCol[2], 1 };
                if (i == TOMATO && !models[i]->useTexture) tint = { 0.9f, 0.15f, 0.15f, 1 };
                Mat4 model = mat4Mul(mat4Translate(pos), mat4Scale(scales[i]));
                if (i == CHEESE) model = mat4Mul(mat4Translate(pos), mat4Mul(mat4RotateX(1.5708f), mat4Scale(scales[i])));
                bool restoreCull = (i == BUN_BOT || i == BUN_TOP || i == PATTY) && cullEnabled;
                if (restoreCull) glDisable(GL_CULL_FACE);
                if (i == PATTY && patty.ownsMesh)
                    drawMesh(shader3D, patty.mesh, patty.texture, patty.useTexture, model, tint);
                else
                    drawMesh(shader3D, models[i]->mesh, models[i]->texture, models[i]->useTexture, model, tint);
                if (restoreCull) glEnable(GL_CULL_FACE);
                yOff += heights[i] * stackHeightFactor;
            }

            if (processed < ING_COUNT && !falling) {
                moveIngredient(win, ingPos, deltaTime);
                Vec4 tint{ 1,1,1,1 };
                if (curIng == PATTY) tint = { pattyCol[0], pattyCol[1], pattyCol[2], 1 };
                if (curIng == KETCHUP) tint = { 0.9f, 0.1f, 0.1f, 1 };
                if (curIng == MUSTARD) tint = { 0.95f, 0.85f, 0.1f, 1 };

                if (curIng == PATTY && patty.ownsMesh) {
                    bool restoreCull = cullEnabled;
                    if (restoreCull) glDisable(GL_CULL_FACE);
                    drawMesh(shader3D, patty.mesh, patty.texture, patty.useTexture, mat4Mul(mat4Translate(ingPos), mat4Scale(scales[curIng])), tint);
                    if (restoreCull) glEnable(GL_CULL_FACE);
                }
                else if (curIng == KETCHUP || curIng == MUSTARD) {
                    drawBottle(shader3D, cylinderMesh, coneMesh, ingPos, scales[curIng], tint);
                }
                else {
                    Mat4 model = mat4Mul(mat4Translate(ingPos), mat4Scale(scales[curIng]));
                    if (curIng == CHEESE) model = mat4Mul(mat4Translate(ingPos), mat4Mul(mat4RotateX(1.5708f), mat4Scale(scales[curIng])));
                    bool restoreCull = (curIng == BUN_BOT || curIng == BUN_TOP) && cullEnabled;
                    if (restoreCull) glDisable(GL_CULL_FACE);
                    drawMesh(shader3D, models[curIng]->mesh, models[curIng]->texture, models[curIng]->useTexture, model, tint);
                    if (restoreCull) glEnable(GL_CULL_FACE);
                }

                bool overPlate = (std::sqrt((ingPos.x - platePos.x) * (ingPos.x - platePos.x) + (ingPos.z - platePos.z) * (ingPos.z - platePos.z)) < plateRadius);
                bool overWaste = (fabsf(ingPos.x - wastePos.x) < wasteSize.x * 0.5f && fabsf(ingPos.z - wastePos.z) < wasteSize.z * 0.5f);

                if (curIng != KETCHUP && curIng != MUSTARD) {
                    if ((overPlate || overWaste) && ingPos.y < 0.4f) {
                        if (overPlate) inBurger[curIng] = true;
                        processed++; curIng++; ingPos = { 0, 0.7f, 0.2f };
                    }
                }
            }

            if (falling) {
                fallPos.y -= 1.5f * deltaTime;
                drawMesh(shader3D, quadMesh, fallType == 0 ? tKetchup : tMustard, true,
                    mat4Mul(mat4Translate(fallPos), mat4Scale({ 0.08f, 1.0f, 0.08f })), { 1,1,1,1 });

                bool overPlate = (std::sqrt((fallPos.x - platePos.x) * (fallPos.x - platePos.x) + (fallPos.z - platePos.z) * (fallPos.z - platePos.z)) < plateRadius);
                bool overWaste = (fabsf(fallPos.x - wastePos.x) < wasteSize.x * 0.5f && fabsf(fallPos.z - wastePos.z) < wasteSize.z * 0.5f);
                if (fallPos.y <= 0.02f) {
                    float stackHeight = 0, stackHeightFactor = 0.85f;
                    for (int i = 0; i < ING_COUNT; i++)
                        if (inBurger[i] && i != KETCHUP && i != MUSTARD)
                            stackHeight += heights[i] * stackHeightFactor + stackOffsets[i];
                    float puddleY = platePos.y + 0.14f + stackHeight - 0.02f;
                    if (overPlate) {
                        inBurger[curIng] = true;
                        if (puddleCount < 20) {
                            puddles[puddleCount] = { fallPos.x, puddleY, fallPos.z, fallType == 0, true };
                            puddleCount++;
                        }
                        processed++; curIng++;
                    }
                    else if (overWaste) {
                        processed++; curIng++;
                    }
                    else if (puddleCount < 20) {
                        puddles[puddleCount] = { fallPos.x, platePos.y + 0.01f, fallPos.z, fallType == 0, true };
                        puddleCount++;
                    }
                    falling = false; ingPos = { 0, 0.7f, 0.2f };
                }
            }
            if (processed >= ING_COUNT) state = FINISHED;
        }
        else if (state == FINISHED) {
            float yOff = 0, stackBaseY = platePos.y + 0.14f, stackHeightFactor = 0.85f;
            for (int i = 0; i < ING_COUNT; i++) {
                if (!inBurger[i] || i == KETCHUP || i == MUSTARD) continue;
                Vec3 pos = platePos; pos.y = stackBaseY + yOff + heights[i] * 0.5f * stackHeightFactor + stackOffsets[i];
                Vec4 tint{ 1,1,1,1 };
                if (i == PATTY) tint = { pattyCol[0], pattyCol[1], pattyCol[2], 1 };
                if (i == TOMATO && !models[i]->useTexture) tint = { 0.9f, 0.15f, 0.15f, 1 };
                Mat4 model = mat4Mul(mat4Translate(pos), mat4Scale(scales[i]));
                if (i == CHEESE) model = mat4Mul(mat4Translate(pos), mat4Mul(mat4RotateX(1.5708f), mat4Scale(scales[i])));
                bool restoreCull = (i == BUN_BOT || i == BUN_TOP || i == PATTY) && cullEnabled;
                if (restoreCull) glDisable(GL_CULL_FACE);
                drawMesh(shader3D, models[i]->mesh, models[i]->texture, models[i]->useTexture, model, tint);
                if (restoreCull) glEnable(GL_CULL_FACE);
                yOff += heights[i] * stackHeightFactor;
            }
        }

        glDisable(GL_DEPTH_TEST);
        if (state == MENU) drawQ(shader2D, tButton, 0, 0, 0.3f, 0.15f, 1, 1, 1, 1);
        if (state == COOKING) drawBar(shader2D, cookProg);
        if (state == FINISHED) drawQ(shader2D, tDone, 0, 0.6f, 0.25f, 0.25f, 1, 1, 1, 1);
        drawQ(shader2D, tStudentInfo, 0.75f, 0.85f, studentW, studentH, 0.6f, 0.85f, 1.0f, 0.7f);

        glfwSwapBuffers(win);
        glfwPollEvents();

        float targetFrame = 1.0f / 75.0f;
        auto frameEnd = std::chrono::steady_clock::now();
        float frameTime = std::chrono::duration<float>(frameEnd - frameStart).count();
        if (frameTime < targetFrame)
            std::this_thread::sleep_for(std::chrono::duration<float>(targetFrame - frameTime));
    }

    if (quadVBO) glDeleteBuffers(1, &quadVBO);
    if (quadVAO) glDeleteVertexArrays(1, &quadVAO);
    glDeleteProgram(shader2D); glDeleteProgram(shader3D);

    if (bunBot.ownsMesh) destroyMesh(bunBot.mesh);
    if (bunTop.ownsMesh) destroyMesh(bunTop.mesh);
    if (patty.ownsMesh) destroyMesh(patty.mesh);
    if (ketchup.ownsMesh) destroyMesh(ketchup.mesh);
    if (mustard.ownsMesh) destroyMesh(mustard.mesh);
    if (pickle.ownsMesh) destroyMesh(pickle.mesh);
    if (onion.ownsMesh) destroyMesh(onion.mesh);
    if (lettuce.ownsMesh) destroyMesh(lettuce.mesh);
    if (cheese.ownsMesh) destroyMesh(cheese.mesh);
    if (tomato.ownsMesh) destroyMesh(tomato.mesh);

    destroyMesh(cubeMesh); destroyMesh(quadMesh); destroyMesh(cylinderMesh); destroyMesh(coneMesh);
    glfwDestroyWindow(win); glfwTerminate();
    return 0;
}