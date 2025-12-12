#include "Util.h"
#include <iostream>
#include <fstream>
#include <sstream>
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