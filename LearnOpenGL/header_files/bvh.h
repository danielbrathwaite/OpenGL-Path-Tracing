
#ifndef BVH_H
#define BVH_H


#include "triangle.h"
#include <vector>
#include <queue>

using namespace std;

struct BVH{
    glm::vec4 minPoint;
    glm::vec4 maxPoint;
    glm::vec4 data; // {bvhChild1, bvhChild2, bvhParent, isRightChild}
    glm::vec4 triangle_data; //{triangleIndex1, triangleIndex2, unused, unused}
};

void bvh_bounding_points(BVH &b1, BVH &b2, glm::vec4 &pmin, glm::vec4 &pmax){
    pmin = b1.minPoint;
    pmax = b1.maxPoint;

    for(int a = 0; a < 3; a++){
        if(b2.minPoint[a] < pmin[a])
            pmin[a] = b2.minPoint[a];
        
        if(b2.maxPoint[a] > pmax[a])
            pmax[a] = b2.maxPoint[a];
    }

}

double bvh_intersection_surface(BVH &b1, BVH &b2){
    glm::vec4 pmin;
    glm::vec4 pmax;

    bvh_bounding_points(b1, b2, pmin, pmax);

    glm::vec4 dims = pmax - pmin;

    return (dims[0] * dims[1] + dims[0] * dims[2] + dims[1] * dims[2]) * 2.0;
}


bool verify_tree(vector<BVH> &tree, vector<Triangle> &triangles){
    
    string triError = "Triangle intersection invalid.";
    string bvhError = "BVH heirarchy invalid.";
    cout << "Verifying tree heirarchy..." << endl;

    for(int i = tree.size() - 1; i >= 0; i--){
        BVH cur = tree[i];

        if(cur.triangle_data[0] > -1) {
            glm::vec4 pmin, pmax;
            tri_bounding_points(triangles[cur.triangle_data[0]],
                                triangles[cur.triangle_data[1]], 
                                pmin, 
                                pmax);

            for(int b = 0; b < 3; b++){
                if(cur.minPoint[b] > pmin[b]){
                    cout << triError << endl;
                    return false;
                }

                if(cur.maxPoint[b] < pmax[b]){
                    cout << triError << endl;
                    return false;
                }
            }
            continue;
        }

        BVH c1 = tree[cur.data[0]];
        BVH c2 = tree[cur.data[1]];

        for(int a = 0; a < 3; a++){
            if(cur.maxPoint[a] < c1.maxPoint[a]){
                cout << bvhError << endl;
                return false;
            }

            if(cur.maxPoint[a] < c2.maxPoint[a]){
                cout << bvhError << endl;
                return false;
            }

            if(cur.minPoint[a] > c1.minPoint[a]){
                cout << bvhError << endl;
                return false;
            }

            if(cur.minPoint[a] > c2.minPoint[a]){
                cout << bvhError << endl;
                return false;
            }
        }
    }
    cout << "Tree heirarchy validated." << endl;

    return true;
}


vector<BVH> buildTree(vector<Triangle> &triangles){
    vector<BVH> tree;
    vector<int> tris_remaining;
    for(int j = 0; j < triangles.size(); j++){
        tris_remaining.push_back(j);
    }

    cout << "Finding geometry bounding boxes..." << endl;

    while(tris_remaining.size()){
        Triangle t = triangles[tris_remaining[0]];

        double min_surface = INFINITY;
        int best_join = tris_remaining[0];
        int to_remove = 0;
        for(int i = 1; i < tris_remaining.size(); i++){
            double join_surface = tri_intersection_surface(t, triangles[tris_remaining[i]]);
            if(join_surface < min_surface){
                min_surface = join_surface;
                best_join = tris_remaining[i];
                to_remove = i;
            }
        }

        BVH add;
        add.data[0] = -1;
        add.data[1] = -1;
        tri_bounding_points(triangles[tris_remaining[0]], triangles[best_join], add.minPoint, add.maxPoint);
        add.triangle_data[0] = tris_remaining[0];
        add.triangle_data[1] = best_join;
        tree.push_back(add);

        if(to_remove){
            tris_remaining.erase(tris_remaining.begin() + to_remove);
        }
        tris_remaining.erase(tris_remaining.begin());
    }

    cout << "Constructing BVH tree..." << endl;

    vector<int> pending_add;
    vector<int> just_added;
    for(int i = 0; i < tree.size(); i++){just_added.push_back(i);}

    while (just_added.size() > 1) {
        while (just_added.size()) {
            int joining = just_added[0];
            double min_surface = INFINITY;
            int best_join = just_added[0];
            int to_remove = 0;

            for (int i = 1; i < just_added.size(); i++) {
                double join_surface = bvh_intersection_surface(tree[joining], tree[just_added[i]]);
                if (join_surface < min_surface) {
                    min_surface = join_surface;
                    best_join = just_added[i];
                    to_remove = i;
                }
            }

            BVH add;
            add.data[0] = joining;
            add.data[1] = best_join;
            add.data[2] = -1;

            tree[joining].data[2] = tree.size();
            tree[joining].data[3] = 0;
            tree[best_join].data[2] = tree.size();
            tree[best_join].data[3] = 1;

            bvh_bounding_points(tree[joining], tree[best_join], add.minPoint, add.maxPoint);
            add.triangle_data[0] = -1;
            add.triangle_data[1] = -1;

            pending_add.push_back(tree.size());
            tree.push_back(add);

            if (to_remove)
                just_added.erase(just_added.begin() + to_remove);
            just_added.erase(just_added.begin());
        }
        just_added = pending_add;
        pending_add.clear();
    }

    cout << "BVH tree construction complete. # of nodes : " << tree.size() << endl;

    verify_tree(tree, triangles);

    return tree; 
}


//deprecated, not sure why this does not work
/*bool hit_bvh(BVH &b, Ray &r){
    for (int a = 0; a < 3; a++) {
        double invD = 1.0f / r.dir[a];
        double t0 = (b.minPoint[a] - r.orig[a]) * invD;
        double t1 = (b.maxPoint[a] - r.orig[a]) * invD;
        if (invD < 0.0){
            double temp = t0;
            t0 = t1;
            t1 = temp;
        }
        double t_min = t0;
        double t_max = t1;
        if (t_max <= t_min)
            return false;
    }
    return true;
}*/

//This goes on the GPU
/*bool bvh_intersect(BVH& b, Ray& r)
{ 
    float tmin = (b.minPoint[0] - r.orig[0]) / r.dir[0]; 
    float tmax = (b.maxPoint[0] - r.orig[0]) / r.dir[0]; 
 
    if (tmin > tmax) swap(tmin, tmax); 
 
    float tymin = (b.minPoint[1] - r.orig[1]) / r.dir[1]; 
    float tymax = (b.maxPoint[1] - r.orig[1]) / r.dir[1]; 
 
    if (tymin > tymax) swap(tymin, tymax); 
 
    if ((tmin > tymax) || (tymin > tmax)) 
        return false; 
 
    if (tymin > tmin) 
        tmin = tymin; 
 
    if (tymax < tmax) 
        tmax = tymax; 
 
    float tzmin = (b.minPoint[2] - r.orig[2]) / r.dir[2]; 
    float tzmax = (b.maxPoint[2] - r.orig[2]) / r.dir[2]; 
 
    if (tzmin > tzmax) swap(tzmin, tzmax); 
 
    if ((tmin > tzmax) || (tzmin > tmax)) 
        return false; 
 
    if (tzmin > tmin) 
        tmin = tzmin; 
 
    if (tzmax < tmax) 
        tmax = tzmax; 
 
    return true; 
}


vector<Tri> visualize_bvh(vector<BVH> heirarchy, int depth) {
    
    queue<int> bvh_queue;
    int current_depth = 1;
    int num_in_current_depth = 1;
    int num_in_next_depth = 0;
    bvh_queue.push(heirarchy.size()-1);

    while(bvh_queue.size() && current_depth < depth){
        BVH cur = heirarchy[bvh_queue.front()];

        num_in_current_depth--;
        bvh_queue.pop();

        if(cur.bvhChild1 > -1){
            bvh_queue.push(cur.bvhChild1);
            bvh_queue.push(cur.bvhChild2);
            num_in_next_depth += 2;
        }

        if(num_in_current_depth == 0){
            num_in_current_depth = num_in_next_depth;
            num_in_next_depth = 0;
            current_depth++;
        }
    }
    RaytracingMaterial vis;
    vis.color = color(1.0, 0.0, 0.0);
    vis.emissionColor = color(0.0);
    vis.smoothness = 0.0;
    vis.specularColor = (1.0, 1.0, 1.0);
    vis.specularProbability = 0.0;

    vector<Tri> visualize;
    while(bvh_queue.size()){
        BVH cur = heirarchy[bvh_queue.front()];
        bvh_queue.pop();

        point3 pmin = cur.minPoint;
        point3 pmax = cur.maxPoint;

        point3 fbl = vec3(pmin[0], pmin[1], pmin[2]);
        point3 fbr = vec3(pmax[0], pmin[1], pmin[2]);
        point3 ftl = vec3(pmin[0], pmin[1], pmax[2]);
        point3 ftr = vec3(pmax[0], pmin[1], pmax[2]);
        point3 bbl = vec3(pmin[0], pmax[1], pmin[2]);
        point3 bbr = vec3(pmax[0], pmax[1], pmin[2]);
        point3 btl = vec3(pmin[0], pmax[1], pmax[2]);
        point3 btr = vec3(pmax[0], pmax[1], pmax[2]);

        Tri t;
        t.material = vis;

        t.v0 = fbl;
        t.v1 = fbr;
        t.v2 = ftl;
        visualize.push_back(t);

        t.v0 = ftr;
        t.v1 = ftl;
        t.v2 = fbr;
        visualize.push_back(t);

    }
    return visualize;
}*/


#endif