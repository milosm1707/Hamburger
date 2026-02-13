#include "Util.h"
#include <iostream>
#include <fstream>
#include <sstream>
#include <vector>
#include <cmath>
#include <GL/glew.h>
#include <GLFW/glfw3.h>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

int endProgram(std::string message) {
    std::cout << message << std::endl;
    glfwTerminate();
    return -1;
}

unsigned int loadImageToTexture(const char* filepath) {
    int width, height, channels;

    // KLJU?NO: 4 zna?i "forsiraj RGBA" - omogu?ava transparentnost!
    unsigned char* data = stbi_load(filepath, &width, &height, &channels, 4);

    if (!data) {
        std::cout << "Textura nije ucitana! Putanja: " << filepath << std::endl;
        return 0;
    }

    std::cout << "? Ucitana: " << filepath << " (" << width << "x" << height
        << ", " << channels << " kanala)" << std::endl;

    unsigned int texture;
    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);

    // MORA biti GL_RGBA za transparentnost!
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, width, height, 0,
        GL_RGBA, GL_UNSIGNED_BYTE, data);

    glGenerateMipmap(GL_TEXTURE_2D);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    stbi_image_free(data);
    return texture;
}

unsigned int createShader(const char* vsPath, const char* fsPath) {
    std::ifstream vsFile(vsPath);
    std::stringstream vsStream;
    vsStream << vsFile.rdbuf();
    std::string vsString = vsStream.str();
    const char* vsSource = vsString.c_str();

    std::ifstream fsFile(fsPath);
    std::stringstream fsStream;
    fsStream << fsFile.rdbuf();
    std::string fsString = fsStream.str();
    const char* fsSource = fsString.c_str();

    if (!vsFile.is_open()) {
        std::cout << "Greska pri citanju: " << vsPath << std::endl;
        return 0;
    }
    if (!fsFile.is_open()) {
        std::cout << "Greska pri citanju: " << fsPath << std::endl;
        return 0;
    }

    unsigned int vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vsSource, NULL);
    glCompileShader(vertexShader);

    int success;
    char infoLog[512];
    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        std::cout << "VERTEX shader greska: " << infoLog << std::endl;
    }

    unsigned int fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fsSource, NULL);
    glCompileShader(fragmentShader);

    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);
    if (!success) {
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        std::cout << "FRAGMENT shader greska: " << infoLog << std::endl;
    }

    unsigned int shaderProgram = glCreateProgram();
    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);
    if (!success) {
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        std::cout << "LINKING greska: " << infoLog << std::endl;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return shaderProgram;
}

GLFWcursor* loadImageToCursor(const char* filepath) {
    int width, height, channels;

    // Kursor tako?e treba RGBA
    unsigned char* data = stbi_load(filepath, &width, &height, &channels, 4);

    if (!data) {
        std::cout << "Kursor nije ucitan! Putanja: " << filepath << std::endl;
        return nullptr;
    }

    GLFWimage image;
    image.width = width;
    image.height = height;
    image.pixels = data;

    // Hotspot u gornjem levom uglu
    GLFWcursor* cursor = glfwCreateCursor(&image, 0, 0);

    stbi_image_free(data);
    return cursor;
}

bool loadObjMesh(const char* filePath, std::vector<Vertex3D>& outVertices) {
    std::ifstream file(filePath);
    if (!file.is_open()) {
        std::cout << "Ne mogu da otvorim OBJ: " << filePath << std::endl;
        return false;
    }

    struct Vec3 { float x, y, z; };
    struct Vec2 { float x, y; };

    std::vector<Vec3> positions;
    std::vector<Vec3> normals;
    std::vector<Vec2> texcoords;

    std::string line;
    while (std::getline(file, line)) {
        if (line.empty() || line[0] == '#') continue;
        std::stringstream ss(line);
        std::string prefix;
        ss >> prefix;
        if (prefix == "v") {
            Vec3 v{};
            ss >> v.x >> v.y >> v.z;
            positions.push_back(v);
        }
        else if (prefix == "vt") {
            Vec2 vt{};
            ss >> vt.x >> vt.y;
            vt.y = 1.0f - vt.y;
            texcoords.push_back(vt);
        }
        else if (prefix == "vn") {
            Vec3 vn{};
            ss >> vn.x >> vn.y >> vn.z;
            normals.push_back(vn);
        }
        else if (prefix == "f") {
            std::vector<std::string> tokens;
            std::string token;
            while (ss >> token) tokens.push_back(token);
            if (tokens.size() < 3) continue;

            auto parseIndex = [](const std::string& value, int count) {
                if (value.empty()) return 0;
                int idx = std::stoi(value);
                if (idx < 0) idx = count + idx + 1;
                return idx;
            };

            auto getVertex = [&](const std::string& vert, Vec3& pos, Vec3& norm, Vec2& tex, bool& hasNorm, bool& hasTex) {
                std::stringstream vs(vert);
                std::string vStr, tStr, nStr;
                std::getline(vs, vStr, '/');
                std::getline(vs, tStr, '/');
                std::getline(vs, nStr, '/');

                int vIdx = parseIndex(vStr, static_cast<int>(positions.size())) - 1;
                int tIdx = parseIndex(tStr, static_cast<int>(texcoords.size())) - 1;
                int nIdx = parseIndex(nStr, static_cast<int>(normals.size())) - 1;

                pos = (vIdx >= 0 && vIdx < static_cast<int>(positions.size())) ? positions[vIdx] : Vec3{};
                if (tIdx >= 0 && tIdx < static_cast<int>(texcoords.size())) {
                    tex = texcoords[tIdx];
                    hasTex = true;
                }
                if (nIdx >= 0 && nIdx < static_cast<int>(normals.size())) {
                    norm = normals[nIdx];
                    hasNorm = true;
                }
            };

            auto makeVertex = [&](const Vec3& pos, const Vec3& norm, const Vec2& tex) {
                Vertex3D v{};
                v.px = pos.x; v.py = pos.y; v.pz = pos.z;
                v.nx = norm.x; v.ny = norm.y; v.nz = norm.z;
                v.u = tex.x; v.v = tex.y;
                outVertices.push_back(v);
            };

            std::vector<Vec3> facePos;
            std::vector<Vec2> faceTex;
            std::vector<Vec3> faceNorm;
            bool hasNorm = false;
            bool hasTex = false;

            for (const auto& faceToken : tokens) {
                Vec3 pos{}, norm{};
                Vec2 tex{};
                getVertex(faceToken, pos, norm, tex, hasNorm, hasTex);
                facePos.push_back(pos);
                faceTex.push_back(tex);
                faceNorm.push_back(norm);
            }

            Vec3 computedNorm{};
            if (!hasNorm && facePos.size() >= 3) {
                Vec3 a = facePos[1];
                Vec3 b = facePos[0];
                Vec3 c = facePos[2];
                Vec3 u{ a.x - b.x, a.y - b.y, a.z - b.z };
                Vec3 v{ c.x - b.x, c.y - b.y, c.z - b.z };
                computedNorm = Vec3{
                    u.y * v.z - u.z * v.y,
                    u.z * v.x - u.x * v.z,
                    u.x * v.y - u.y * v.x
                };
                float len = std::sqrt(computedNorm.x * computedNorm.x + computedNorm.y * computedNorm.y + computedNorm.z * computedNorm.z);
                if (len > 0.0f) {
                    computedNorm.x /= len;
                    computedNorm.y /= len;
                    computedNorm.z /= len;
                }
            }

            for (size_t i = 1; i + 1 < facePos.size(); ++i) {
                Vec3 n0 = hasNorm ? faceNorm[0] : computedNorm;
                Vec3 n1 = hasNorm ? faceNorm[i] : computedNorm;
                Vec3 n2 = hasNorm ? faceNorm[i + 1] : computedNorm;
                Vec2 t0 = hasTex ? faceTex[0] : Vec2{};
                Vec2 t1 = hasTex ? faceTex[i] : Vec2{};
                Vec2 t2 = hasTex ? faceTex[i + 1] : Vec2{};

                makeVertex(facePos[0], n0, t0);
                makeVertex(facePos[i], n1, t1);
                makeVertex(facePos[i + 1], n2, t2);
            }
        }
    }

    return !outVertices.empty();
}