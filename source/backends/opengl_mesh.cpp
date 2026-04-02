#include "opengl_mesh.h"
#include <glad/glad.h>

namespace Nothofagus
{

void OpenGLMesh::initBuffers()
{
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);
}

void OpenGLMesh::fillBuffers(const Mesh& mesh, unsigned int usage)
{
    size = mesh.indices.size();

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, mesh.vertices.size() * SIZE_IN_BYTES, mesh.vertices.data(), usage);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh.indices.size() * SIZE_IN_BYTES, mesh.indices.data(), usage);
}

void OpenGLMesh::drawCall() const
{
    glBindVertexArray(vao);
    glDrawElements(GL_TRIANGLES, size, GL_UNSIGNED_INT, nullptr);
    glBindVertexArray(0);
}

void OpenGLMesh::clear() const
{
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &ebo);
}

}
