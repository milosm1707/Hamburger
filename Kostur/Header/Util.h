#pragma once
#include <GL/glew.h>
#include <GLFW/glfw3.h>
#include <string>
#include <vector>

int endProgram(std::string message);
unsigned int createShader(const char* vsSource, const char* fsSource);
unsigned loadImageToTexture(const char* filePath);
GLFWcursor* loadImageToCursor(const char* filePath);

struct Vertex3D {
    float px, py, pz;
    float nx, ny, nz;
    float u, v;
};

bool loadObjMesh(const char* filePath, std::vector<Vertex3D>& outVertices);