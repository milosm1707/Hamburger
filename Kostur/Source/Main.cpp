#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <cmath>
#include "../Header/Util.h"
#include <vector>
#include <cctype>

enum GameState { MENU, COOKING, ASSEMBLING, FINISHED };
enum Ingredient { BUN_BOT, PATTY, KETCHUP, MUSTARD, PICKLE, ONION, LETTUCE, CHEESE, TOMATO, BUN_TOP, ING_COUNT };

GameState state = MENU;
int scrW = 1920, scrH = 1080;

float pattyX = 0.0f, pattyY = 0.6f, cookProg = 0.0f, pattyCol[3];
int curIng = BUN_BOT;
float ingX = 0.0f, ingY = 0.6f;
bool inBurger[ING_COUNT] = { false };
int processed = 0;

bool falling = false, dripping = false;
float fallY = 0.0f, fallX = 0.0f;
int fallType = -1;

struct Puddle { float x, y; bool isKetchup, active; };
Puddle puddles[20];
int puddleCount = 0;

unsigned tButton, tStove, tTable, tPlate, tPatty;
unsigned tBunBot, tBunTop, tKetchup, tMustard, tPickle, tOnion, tLettuce, tCheese, tTomato;
unsigned tKetchupBot, tMustardBot, tWaste, tDone, shader, tStudentInfo;
unsigned quadVAO = 0, quadVBO = 0;

void initQuad() {
    if (quadVAO != 0) return;

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

void shutdownQuad() {
    if (quadVBO != 0) glDeleteBuffers(1, &quadVBO);
    if (quadVAO != 0) glDeleteVertexArrays(1, &quadVAO);
    quadVBO = 0;
    quadVAO = 0;
}

void loadTex(unsigned& tex, const char* path) {
    tex = loadImageToTexture(path);
    glBindTexture(GL_TEXTURE_2D, tex);
    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
}

void mouseClick(GLFWwindow* win, int btn, int act, int mods) {
    if (btn == GLFW_MOUSE_BUTTON_LEFT && act == GLFW_PRESS && state == MENU) {
        double x, y;
        glfwGetCursorPos(win, &x, &y);
        float xN = (x / scrW) * 2 - 1, yN = -((y / scrH) * 2 - 1);
        if (xN >= -0.3f && xN <= 0.3f && yN >= -0.15f && yN <= 0.15f) state = COOKING;
    }
}

void keyPress(GLFWwindow* win, int key, int sc, int act, int mods) {
    if (key == GLFW_KEY_ESCAPE && act == GLFW_PRESS) glfwSetWindowShouldClose(win, GLFW_TRUE);
    if (key == GLFW_KEY_SPACE && state == ASSEMBLING) {
        if (act == GLFW_PRESS && (curIng == KETCHUP || curIng == MUSTARD)) dripping = true;
        if (act == GLFW_RELEASE && dripping) {
            falling = true;
            fallX = ingX;
            fallY = ingY - 0.1f;
            fallType = (curIng == KETCHUP) ? 0 : 1;
            dripping = false;
        }
    }
}

void move(GLFWwindow* win, float& x, float& y) {
    if (glfwGetKey(win, GLFW_KEY_W) == GLFW_PRESS) y += 0.005f;
    if (glfwGetKey(win, GLFW_KEY_S) == GLFW_PRESS) y -= 0.005f;
    if (glfwGetKey(win, GLFW_KEY_A) == GLFW_PRESS) x -= 0.005f;
    if (glfwGetKey(win, GLFW_KEY_D) == GLFW_PRESS) x += 0.005f;
    x = fmax(-0.9f, fmin(0.9f, x));
    y = fmax(-0.9f, fmin(0.9f, y));
}

void drawQ(unsigned s, unsigned tex, float x, float y, float w, float h, float r, float g, float b, float a, bool flip = false) {
    initQuad();

    float t1 = flip ? 1.0f : 0.0f, t2 = flip ? 0.0f : 1.0f;
    float v[] = { x - w,y + h,0,t1, x - w,y - h,0,t2, x + w,y - h,1,t2, x + w,y + h,1,t1 };

    glBindVertexArray(quadVAO);
    glBindBuffer(GL_ARRAY_BUFFER, quadVBO);
    glBufferSubData(GL_ARRAY_BUFFER, 0, sizeof(v), v);

    glUseProgram(s);
    glUniform4f(glGetUniformLocation(s, "uColor"), r, g, b, a);

    unsigned t = tex;
    bool dummy = false;
    if (tex == 0) {
        unsigned char w[4] = { 255,255,255,255 };
        glGenTextures(1, &t);
        glBindTexture(GL_TEXTURE_2D, t);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, 1, 1, 0, GL_RGBA, GL_UNSIGNED_BYTE, w);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        dummy = true;
    }

    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, t);
    glUniform1i(glGetUniformLocation(s, "uTex"), 0);
    glBindVertexArray(quadVAO);
    glDrawArrays(GL_TRIANGLE_FAN, 0, 4);

    if (dummy) glDeleteTextures(1, &t);
}

void drawBar(unsigned s, float prog) {
    drawQ(s, 0, 0.0f, 0.85f, 0.5f, 0.06f, 0.3f, 0.3f, 0.3f, 1.0f);
    float w = 0.5f * prog;
    drawQ(s, 0, -0.5f + w, 0.85f, w, 0.055f, 0.0f, 0.8f, 0.0f, 1.0f);
}

int studentTexW = 0, studentTexH = 0;
float studentW = 0.0f, studentH = 0.0f;

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

    c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
    switch (c) {
    case 'A': return A;
    case 'C': return C;
    case 'E': return E;
    case 'I': return I;
    case 'J': return J;
    case 'L': return L;
    case 'M': return M;
    case 'O': return O;
    case 'S': return S;
    case 'V': return V;
    case '0': return zero;
    case '2': return two;
    case '8': return eight;
    case '/': return slash;
    case ' ': return space;
    default: return space;
    }
}

unsigned int createTextTexture(const char* text, int scale, int padding, int& outW, int& outH) {
    const int glyphW = 5;
    const int glyphH = 7;
    int len = static_cast<int>(std::strlen(text));

    outW = len * (glyphW * scale + padding) - padding;
    outH = glyphH * scale;

    std::vector<unsigned char> pixels(outW * outH * 4, 0);

    for (int i = 0; i < len; i++) {
        const unsigned char* glyph = getGlyph(text[i]);
        int baseX = i * (glyphW * scale + padding);

        for (int y = 0; y < glyphH; y++) {
            for (int x = 0; x < glyphW; x++) {
                bool on = (glyph[y] & (1 << (glyphW - 1 - x))) != 0;
                if (!on) continue;

                for (int sy = 0; sy < scale; sy++) {
                    for (int sx = 0; sx < scale; sx++) {
                        int px = baseX + x * scale + sx;
                        int py = y * scale + sy;
                        int idx = (py * outW + px) * 4;

                        pixels[idx + 0] = 255;
                        pixels[idx + 1] = 255;
                        pixels[idx + 2] = 255;
                        pixels[idx + 3] = 255;
                    }
                }
            }
        }
    }

    unsigned int tex = 0;
    glGenTextures(1, &tex);
    glBindTexture(GL_TEXTURE_2D, tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, outW, outH, 0, GL_RGBA, GL_UNSIGNED_BYTE, pixels.data());
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    return tex;
}

int main() {
    glfwInit();
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWmonitor* mon = glfwGetPrimaryMonitor();
    const GLFWvidmode* mode = glfwGetVideoMode(mon);
    scrW = mode->width;
    scrH = mode->height;

    GLFWwindow* win = glfwCreateWindow(scrW, scrH, "Hamburger Simulator", mon, NULL);
    if (!win) return endProgram("Prozor greska");
    glfwMakeContextCurrent(win);
    glfwSetMouseButtonCallback(win, mouseClick);
    glfwSetKeyCallback(win, keyPress);

    if (glewInit() != GLEW_OK) return endProgram("GLEW greska");

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    GLFWcursor* cur = loadImageToCursor("res/spatula.png");
    if (cur) glfwSetCursor(win, cur);

    loadTex(tButton, "res/button.png");
    loadTex(tStove, "res/stove3.png");
    loadTex(tTable, "res/table.png");
    loadTex(tPlate, "res/plate3.png");
    loadTex(tPatty, "res/patty4.png");
    loadTex(tBunBot, "res/bun_bottom3.png");
    loadTex(tBunTop, "res/bun_top3.png");
    loadTex(tKetchup, "res/ketchup2.png");
    loadTex(tMustard, "res/mustard.png");
    loadTex(tPickle, "res/pickle3.png");
    loadTex(tOnion, "res/onion3.png");
    loadTex(tLettuce, "res/lettuce3.png");
    loadTex(tCheese, "res/cheese3.png");
    loadTex(tTomato, "res/tomato2.png");
    loadTex(tKetchupBot, "res/ketchup3.png");
    loadTex(tMustardBot, "res/mustard3.png");
    loadTex(tWaste, "res/plate3.png");
    loadTex(tDone, "res/done.png");

    shader = createShader("Shaders/basic.vert", "Shaders/basic.frag");
    for (int i = 0; i < 20; i++) puddles[i].active = false;
    glClearColor(0.85f, 0.9f, 0.95f, 1.0f);

    tStudentInfo = createTextTexture("Milos Milosavljevic SV80/2022", 3, 2, studentTexW, studentTexH);
    studentW = studentTexW / static_cast<float>(scrW);
    studentH = studentTexH / static_cast<float>(scrH);

    while (!glfwWindowShouldClose(win)) {
        glClear(GL_COLOR_BUFFER_BIT);

        if (state == MENU) {
            drawQ(shader, tButton, 0.0f, 0.0f, 0.3f, 0.15f, 1, 1, 1, 1);
        }
        else if (state == COOKING) {
            drawQ(shader, tStove, 0, -0.65f, 0.6f, 0.3f, 1, 1, 1, 1);
            move(win, pattyX, pattyY);

            bool onStove = (pattyY < -0.25f && pattyY > -0.95f && pattyX > -0.6f && pattyX < 0.6f);
            if (onStove) {
                cookProg += 0.0002f;
                if (cookProg > 1.0f) cookProg = 1.0f;
            }

            float r = 0.8f - cookProg * 0.3f;
            float g = 0.5f - cookProg * 0.15f;
            float b = 0.4f - cookProg * 0.25f;

            if (cookProg >= 1.0f) {
                pattyCol[0] = r; pattyCol[1] = g; pattyCol[2] = b;
            }

            drawQ(shader, tPatty, pattyX, pattyY, 0.15f, 0.15f, r, g, b, 1);
            drawBar(shader, cookProg);

            if (cookProg >= 1.0f) {
                state = ASSEMBLING;
                pattyX = pattyY = 0.6f;
                ingX = 0.0f; ingY = 0.6f;
            }
        }
        else if (state == ASSEMBLING) {
            drawQ(shader, tTable, 0, -0.5f, 1.0f, 0.4f, 1, 1, 1, 1);
            drawQ(shader, tPlate, 0, -0.5f, 0.25f, 0.25f, 1, 1, 1, 1);
            drawQ(shader, tWaste, 0.6f, -0.5f, 0.2f, 0.2f, 1, 1, 1, 1);

            for (int i = 0; i < puddleCount; i++) {
                if (puddles[i].active) {
                    unsigned pTex = puddles[i].isKetchup ? tKetchup : tMustard;
                    drawQ(shader, pTex, puddles[i].x, puddles[i].y, 0.08f, 0.04f, 1, 1, 1, 1);
                }
            }

            float plateY = -0.5f, yOff = 0.0f;
            unsigned allTex[] = { tBunBot, tPatty, tKetchup, tMustard, tPickle, tOnion, tLettuce, tCheese, tTomato, tBunTop };
            float allH[] = { 0.06f, 0.08f, 0.03f, 0.03f, 0.06f, 0.07f, 0.06f, 0.06f, 0.07f, 0.08f };

            for (int i = 0; i < ING_COUNT; i++) {
                if (!inBurger[i]) continue;
                unsigned tex = allTex[i];
                float h = allH[i];
                float r = 1, g = 1, b = 1;
                if (i == PATTY) { r = pattyCol[0]; g = pattyCol[1]; b = pattyCol[2]; }
                drawQ(shader, tex, 0, plateY + yOff, 0.18f, h, r, g, b, 1);
                yOff += h + 0.01f;
            }

            if (processed < ING_COUNT && !falling) {
                move(win, ingX, ingY);

                unsigned cTex;
                float w = 0.18f, h = 0.06f;
                bool isBot = false, flip = false;

                switch (curIng) {
                case BUN_BOT: cTex = tBunBot; break;
                case PATTY: cTex = tPatty; h = 0.08f; break;
                case KETCHUP: cTex = tKetchupBot; isBot = flip = true; w = 0.08f; h = 0.15f; break;
                case MUSTARD: cTex = tMustardBot; isBot = flip = true; w = 0.08f; h = 0.15f; break;
                case PICKLE: cTex = tPickle; break;
                case ONION: cTex = tOnion; h = 0.07f; break;
                case LETTUCE: cTex = tLettuce; break;
                case CHEESE: cTex = tCheese; break;
                case TOMATO: cTex = tTomato; h = 0.07f; break;
                case BUN_TOP: cTex = tBunTop; h = 0.08f; break;
                default: cTex = 0;
                }

                float r = 1, g = 1, b = 1;

                if (curIng == PATTY) { r = pattyCol[0]; g = pattyCol[1]; b = pattyCol[2]; }
                drawQ(shader, cTex, ingX, ingY, w, h, r, g, b, 1, flip);

                if (!isBot) {
                    bool overPlate = (ingX > -0.25f && ingX < 0.25f && ingY < -0.15f && ingY > -0.8f);
                    bool overWaste = (ingX > 0.45f && ingX < 0.75f && ingY < -0.15f && ingY > -0.85f);

                    if (overPlate) {
                        inBurger[curIng] = true;
                        processed++;
                        curIng++;
                        ingX = 0.0f; ingY = 0.6f;
                    }
                    else if (overWaste) {
                        processed++;
                        curIng++;
                        ingX = 0.0f; ingY = 0.6f;
                    }
                }
            }

            if (falling) {
                fallY -= 0.01f;
                unsigned sTex = (fallType == 0) ? tKetchup : tMustard;
                drawQ(shader, sTex, fallX, fallY, 0.04f, 0.02f, 1, 1, 1, 1);

                bool overPlateX = (fallX > -0.25f && fallX < 0.25f);
                bool overWasteX = (fallX > 0.45f && fallX < 0.75f);
                if (fallY <= -0.3f) {
                    if (overPlateX) {
                        inBurger[curIng] = true;
                        processed++;
                        curIng++;
                    }
                    else if (overWasteX) {
                        processed++;
                        curIng++;
                    }
                    else {
                        if (puddleCount < 20) {
                            puddles[puddleCount].x = fallX;
                            puddles[puddleCount].y = -0.7f;
                            puddles[puddleCount].isKetchup = (fallType == 0);
                            puddles[puddleCount].active = true;
                            puddleCount++;
                        }
                    }
                    falling = false;
                    ingX = 0.0f; ingY = 0.6f;
                }
            }

            if (processed >= ING_COUNT) state = FINISHED;
        }
        else if (state == FINISHED) {
            drawQ(shader, tTable, 0, -0.5f, 1.0f, 0.4f, 1, 1, 1, 1);
            drawQ(shader, tPlate, 0, -0.5f, 0.25f, 0.25f, 1, 1, 1, 1);

            float plateY = -0.5f, yOff = 0.0f;
            unsigned allTex[] = { tBunBot, tPatty, tKetchup, tMustard, tPickle, tOnion, tLettuce, tCheese, tTomato, tBunTop };
            float allH[] = { 0.06f, 0.08f, 0.03f, 0.03f, 0.06f, 0.07f, 0.06f, 0.06f, 0.07f, 0.08f };

            for (int i = 0; i < ING_COUNT; i++) {
                if (!inBurger[i]) continue;
                unsigned tex = allTex[i];
                float h = allH[i];
                float r = 1, g = 1, b = 1;
                if (i == PATTY) { r = pattyCol[0]; g = pattyCol[1]; b = pattyCol[2]; }
                drawQ(shader, tex, 0, plateY + yOff, 0.18f, h, r, g, b, 1);
                yOff += h + 0.01f;
            }

            drawQ(shader, tDone, 0, 0.6f, 0.25f, 0.25f, 1, 1, 1, 1);
        }

        drawQ(shader, tStudentInfo, 0.75f, 0.85f, studentW, studentH, 0.6f, 0.85f, 1.0f, 0.7f);
        glfwSwapBuffers(win);
        glfwPollEvents();
    }

    shutdownQuad();
    glDeleteProgram(shader);
    glfwDestroyWindow(win);
    glfwTerminate();
    return 0;
}/*#########################################################################*/