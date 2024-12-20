

#include "dmesh.h"
#include <glad/glad.h>

namespace Nothofagus{


void DMesh::initBuffers()
{
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);
}

//void DMesh::fillBuffers(const Mesh& mesh, GPUID usage)
void DMesh::fillBuffers(const Mesh& mesh, unsigned int usage)
{
    size = mesh.indices.size();

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, mesh.vertices.size() * SIZE_IN_BYTES, mesh.vertices.data(), usage);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh.indices.size() * SIZE_IN_BYTES, mesh.indices.data(), usage);
}

void DMesh::drawCall() const
{
    // Binding the VAO and executing the draw call
    glBindVertexArray(vao);

    glBindTexture(GL_TEXTURE_2D, texture);
    glDrawElements(GL_TRIANGLES, size, GL_UNSIGNED_INT, nullptr);

    // Unbind the current VAO
    glBindVertexArray(0);
}

void DMesh::clear() const
{
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &ebo);
    /* TODO: clear texture memory */
}

std::ostream& operator<<(std::ostream& os, const DMesh& dMesh)
{
    os << "vao=" << dMesh.vao
        << " vbo=" << dMesh.vbo
        << " ebo=" << dMesh.ebo
        << " tex=" << dMesh.texture;

    return os;
}

void DMesh3D::initBuffers()
{
    glGenVertexArrays(1, &vao);
    glGenBuffers(1, &vbo);
    glGenBuffers(1, &ebo);
}

//void DMesh3D::fillBuffers(const Mesh& mesh, GPUID usage)
void DMesh3D::fillBuffers(const Mesh& mesh, unsigned int usage)
{
    size = mesh.indices.size();

    glBindBuffer(GL_ARRAY_BUFFER, vbo);
    glBufferData(GL_ARRAY_BUFFER, mesh.vertices.size() * SIZE_IN_BYTES, mesh.vertices.data(), usage);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, mesh.indices.size() * SIZE_IN_BYTES, mesh.indices.data(), usage);
}

void DMesh3D::drawCall() const
{
    // Binding the VAO and executing the draw call
    glBindVertexArray(vao);

    glBindTexture(GL_TEXTURE_2D_ARRAY, textureArray);
    glDrawElements(GL_TRIANGLES, size, GL_UNSIGNED_INT, nullptr);

    // Unbind the current VAO
    glBindVertexArray(0);
}

void DMesh3D::clear() const
{
    glDeleteVertexArrays(1, &vao);
    glDeleteBuffers(1, &vbo);
    glDeleteBuffers(1, &ebo);
    /* TODO: clear texture memory */
}

std::ostream& operator<<(std::ostream& os, const DMesh3D& dMesh)
{
    os << "vao=" << dMesh.vao
        << " vbo=" << dMesh.vbo
        << " ebo=" << dMesh.ebo
        << " tex=" << dMesh.textureArray;

    return os;
}

} // namespace BoxRenderer