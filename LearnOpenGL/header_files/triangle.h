
#ifndef TRI_H
#define TRI_H


#include "material.h"

struct Triangle{
    glm::vec4 v0;
    glm::vec4 v1;
    glm::vec4 v2;
    glm::vec4 materialData;
};


#endif
