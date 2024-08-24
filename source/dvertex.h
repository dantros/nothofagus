#pragma once

namespace Nothofagus
{

struct DVertex
{
    float x, y, tx, ty;

    /* Number of floats to represent this vertex data attribute */
    unsigned int arity() const { return 4; }
};

}