#pragma once

#include "VertexSimple.h"

FVertexSimple GTriangleVertices[] =
{
    {  0.0f,  1.0f, 0.0f,  1.0f, 0.0f, 0.0f, 1.0f }, // Top (Red)
    {  1.0f, -1.0f, 0.0f,  0.0f, 1.0f, 0.0f, 1.0f }, // Bottom-Right (Green)
    { -1.0f, -1.0f, 0.0f,  0.0f, 0.0f, 1.0f, 1.0f }  // Bottom-Left (Blue)
};