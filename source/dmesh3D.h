#pragma once

// #include "mesh.h"
// #include <iostream>
// //#include "shaders_core.h"
// #include <glm/glm.hpp>

// namespace Nothofagus
// {

// /* We will use 32 bits data, so floats and integers have 4 bytes.
//  * 1 byte = 8 bits
//  */
// constexpr unsigned int SIZE_IN_BYTES = 4;

// struct DMesh3D
// {
//     //GPUID 
//     unsigned int vao, vbo, ebo, textureArray;
//     std::size_t size;

//     // uniform
//     glm::mat3 transform;

//     void initBuffers();

//     //void fillBuffers(const Mesh& mesh, GPUID usage);
//     void fillBuffers(const Mesh& mesh, unsigned int usage);

//     void drawCall() const;

//     /* Freeing GPU memory */
//     void clear() const;
// };

// std::ostream& operator<<(std::ostream& os, const DMesh3D& dMesh);

// }