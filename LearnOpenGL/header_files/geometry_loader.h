#ifndef GEOMETRY_LOADER_H
#define GEOMETRY_LOADER_H

#include "triangle.h"
#include "material.h"

#include <iostream>
#include <fstream>
#include <vector>
#include <strstream>
#include <unordered_map>

using namespace std;

void load_vertex_data(string geometry_data, string material_data, vector<Triangle>& trivect, vector<Material>& matvect) {

    vector<glm::vec4> verts;

    unordered_map<string, int> mmap;

    fstream m(material_data);

    if (!m.is_open()) {
        cout << "Failed to open material file: " << material_data << endl;
        return;
    }

    cout << "Loading materials (" << material_data << ")" << endl;

    while (!m.eof()) {
        char line[128];
        m.getline(line, 128);

        strstream s;
        s << line;

        string identifier;
        string materialname;

        s >> identifier >> materialname;

        if (identifier == "newmtl") {

            glm::vec4 color;
            glm::vec4 emissionColor;
            glm::vec4 specularColor;

            glm::vec4 data; //{emissionStrength, smoothness, specularProbability, unused}

            for (int mind = 0; mind < 8; mind++) {
                char linedata[128];
                m.getline(linedata, 128);

                strstream sdata;
                sdata << linedata;

                string throwaway;

                if (linedata[0] == 'N') {
                    if (linedata[1] == 's') {
                        sdata >> throwaway >> data.z;
                        data.z /= 1000.0; //scale to between [0, 1]
                    }
                }
                else if (linedata[0] == 'K') {

                    if (linedata[1] == 'a') {
                        // do not know what ambient color is
                    }
                    if (linedata[1] == 'e') {
                        sdata >> throwaway >> emissionColor.r >> emissionColor.g >> emissionColor.b;
                    }
                    if (linedata[1] == 'd') {
                        sdata >> throwaway >> color.r >> color.g >> color.b;
                    }
                    if (linedata[1] == 's') {
                        sdata >> throwaway >> specularColor.r >> specularColor.g >> specularColor.b;
                    }
                }
            }
            if (data.z > 0) {
                data.y = 1.0;
            }
            data.x = 7.5; // TODO: update 
            Material add = {
                color,
                emissionColor,
                specularColor,
                data
            };
            matvect.push_back(add);
            mmap[materialname] = matvect.size() - 1;
        }

    }

    cout << "Material loading finished.\n" << endl;


    fstream f(geometry_data);

    if (!f.is_open()) {
        cout << "Failed to open vertex file: " << geometry_data << endl;
        return;
    }

    cout << "Loading geometry (" << geometry_data << ")" << endl;

    string currentMaterial;
    string preface;

    while (!f.eof()) {
        char line[128];
        f.getline(line, 128);

        strstream s;
        s << line;

        char identifier;

        if (line[0] == 'u') {
            s >> preface >> currentMaterial;
        }
        if (line[0] == 'v') {
            glm::vec4 v;
            s >> identifier >> v.x >> v.y >> v.z;
            verts.push_back(v);
        }

        if (line[0] == 'f') {
            int f[3];
            s >> identifier >> f[0] >> f[1] >> f[2];
            if (!mmap.count(currentMaterial)) {
                cout << "Could not locate material : " << currentMaterial << endl;
            }
            trivect.push_back({ verts[f[0] - 1], verts[f[1] - 1], verts[f[2] - 1], glm::vec4(mmap[currentMaterial], 0.0, 0.0, 0.0) });
        }

    }

    cout << "Geometry loading finished.\n" << endl;
}

#endif