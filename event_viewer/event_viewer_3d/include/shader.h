#pragma once
#include <string>
#include <GL/glew.h>
#include <glm/glm.hpp>

class Shader {
public:
    Shader(const std::string& vertexPath, const std::string& fragmentPath);
    ~Shader();

    void use() const;
    void setMat4(const std::string& name, const glm::mat4& mat) const;
    void setFloat(const std::string& name, float value) const;

private:
    GLuint m_id;
    GLuint loadAndCompile(const std::string& path, GLenum shaderType);
};