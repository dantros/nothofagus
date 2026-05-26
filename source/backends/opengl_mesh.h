#pragma once

#include "mesh.h"
#include <glm/glm.hpp>

namespace Nothofagus
{

struct OpenGLMesh
{
    unsigned int vao, vbo, ebo;
    std::size_t size;
    glm::mat3 transform;

    void initBuffers();
    void fillBuffers(const Mesh& mesh, unsigned int usage);
    void drawCall() const;
    void clear() const;
};

}
