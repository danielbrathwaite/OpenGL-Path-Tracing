
#ifndef TRI_H
#define TRI_H


#include "material.h"

struct Triangle{
    glm::vec4 v0;
    glm::vec4 v1;
    glm::vec4 v2;
    glm::vec4 materialData;
};

void tri_bounding_points(Triangle &t1, Triangle &t2, glm::vec4 &pmin, glm::vec4 &pmax) {
    pmin = t1.v0;
    pmax = t1.v0;

    for (int a = 0; a < 3; a++) {
        if (t1.v1[a] < pmin[a])
            pmin[a] = t1.v1[a];
        if (t1.v1[a] > pmax[a])
            pmax[a] = t1.v1[a];

        if (t1.v2[a] < pmin[a])
            pmin[a] = t1.v2[a];
        if (t1.v2[a] > pmax[a])
            pmax[a] = t1.v2[a];
    }

    for (int a = 0; a < 3; a++) {
        if (t2.v0[a] < pmin[a])
            pmin[a] = t2.v0[a];
        if (t2.v0[a] > pmax[a])
            pmax[a] = t2.v0[a];

        if (t2.v1[a] < pmin[a])
            pmin[a] = t2.v1[a];
        if (t2.v1[a] > pmax[a])
            pmax[a] = t2.v1[a];

        if (t2.v2[a] < pmin[a])
            pmin[a] = t2.v2[a];
        if (t2.v2[a] > pmax[a])
            pmax[a] = t2.v2[a];
    }
}

double tri_intersection_surface(Triangle &t1, Triangle &t2) {
    glm::vec4 pmin, pmax;

    tri_bounding_points(t1, t2, pmin, pmax);

    glm::vec4 dims = pmax - pmin;

    return (dims[0] * dims[1] + dims[0] * dims[2] + dims[1] * dims[2]) * 2.0;
}


#endif
