/**
 * Copyright (c) 2021-2023 Floyd M. Chitalu.
 * All rights reserved.
 *
 * NOTE: This file is licensed under GPL-3.0-or-later (default).
 * A commercial license can be purchased from Floyd M. Chitalu.
 *
 * License details:
 *
 * (A)  GNU General Public License ("GPL"); a copy of which you should have
 *      recieved with this file.
 * 	    - see also: <http://www.gnu.org/licenses/>
 * (B)  Commercial license.
 *      - email: floyd.m.chitalu@gmail.com
 *
 * The commercial license options is for users that wish to use MCUT in
 * their products for comercial purposes but do not wish to release their
 * software products under the GPL license.
 *
 * Author(s)     : Floyd M. Chitalu
 */

#include "mcut/mcut.h"

#include <stdio.h>
#include <stdlib.h>

#include <vector>

void writeOFF(
    const char* fpath,
    float* pVertices,
    uint32_t* pFaceIndices,
    uint32_t* pFaceSizes,
    uint32_t numVertices,
    uint32_t numFaces);

int main()
{
    // 1. Create meshes.
    // -----------------

    // Shape to Cut:
    double cubeVertices[] = {
        -1, -1, 1, // 0
        1, -1, 1, // 1
        -1, 1, 1, // 2
        1, 1, 1, // 3
        -1, -1, -1, // 4
        1, -1, -1, // 5
        -1, 1, -1, // 6
        1, 1, -1 // 7
    };
    uint32_t cubeFaces[] = {
        0, 3, 2, // 0
        0, 1, 3, // 1
        1, 7, 3, // 2
        1, 5, 7, // 3
        5, 6, 7, // 4
        5, 4, 6, // 5
        4, 2, 6, // 6
        4, 0, 2, // 7
        2, 7, 6, // 8
        2, 3, 7, // 9
        4, 1, 0, // 10
        4, 5, 1, // 11

    };
    int numCubeVertices = 8;
    int numCubeFaces = 12;

    uint32_t cubeFaceSizes[] = {
        3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3, 3
    };

    // Cutting Shape:

    double cutMeshVertices[] = {
        -1.2, 1.6, 0.994070,
        1.4, -1.3, 0.994070,
        -1.2, 1.6, -1.005929,
        1.4, -1.3, -1.005929
    };

    uint32_t cutMeshFaces[] = {
        1, 2, 0,
        1, 3, 2
    };

    uint32_t numCutMeshVertices = 4;
    uint32_t numCutMeshFaces = 2;

    // 2. create a context
    // -------------------
    McContext context = MC_NULL_HANDLE;
    McResult err = mcCreateContext(&context, MC_NULL_HANDLE);

    if (err != MC_NO_ERROR) {
        fprintf(stderr, "could not create context (err=%d)\n", (int)err);
        exit(1);
    }

    // 3. do the magic!
    // ----------------
    err = mcDispatch(
        context,
        MC_DISPATCH_VERTEX_ARRAY_DOUBLE,
        cubeVertices,
        cubeFaces,
        cubeFaceSizes,
        numCubeVertices,
        numCubeFaces,
        cutMeshVertices,
        cutMeshFaces,
        nullptr, // cutMeshFaceSizes, // no need to give 'faceSizes' parameter since cut-mesh is a triangle mesh
        numCutMeshVertices,
        numCutMeshFaces);

    if (err != MC_NO_ERROR) {
        fprintf(stderr, "dispatch call failed (err=%d)\n", (int)err);
        exit(1);
    }

    // 4. query the number of available connected component (all types)
    // -------------------------------------------------------------
    uint32_t numConnComps;
    std::vector<McConnectedComponent> connComps;

    err = mcGetConnectedComponents(context, MC_CONNECTED_COMPONENT_TYPE_ALL, 0, NULL, &numConnComps);

    if (err != MC_NO_ERROR) {
        fprintf(stderr, "1:mcGetConnectedComponents(MC_CONNECTED_COMPONENT_TYPE_ALL) failed (err=%d)\n", (int)err);
        exit(1);
    }

    if (numConnComps == 0) {
        fprintf(stdout, "no connected components found\n");
        exit(0);
    }

    connComps.resize(numConnComps);

    err = mcGetConnectedComponents(context, MC_CONNECTED_COMPONENT_TYPE_ALL, (uint32_t)connComps.size(), connComps.data(), NULL);

    if (err != MC_NO_ERROR) {
        fprintf(stderr, "2:mcGetConnectedComponents(MC_CONNECTED_COMPONENT_TYPE_ALL) failed (err=%d)\n", (int)err);
        exit(1);
    }

    // 5. query the data of each connected component from MCUT
    // -------------------------------------------------------

    for (int i = 0; i < (int)connComps.size(); ++i) {
        McConnectedComponent connComp = connComps[i]; // connected compoenent id

        // query the vertices
        // ----------------------

        McSize numBytes = 0;

        err = mcGetConnectedComponentData(context, connComp, MC_CONNECTED_COMPONENT_DATA_VERTEX_FLOAT, 0, NULL, &numBytes);

        if (err != MC_NO_ERROR) {
            fprintf(stderr, "1:mcGetConnectedComponentData(MC_CONNECTED_COMPONENT_DATA_VERTEX_FLOAT) failed (err=%d)\n", (int)err);
            exit(1);
        }

        uint32_t numberOfVertices = (uint32_t)(numBytes / (sizeof(float) * 3));

        std::vector<float> vertices(numberOfVertices * 3u);

        err = mcGetConnectedComponentData(context, connComp, MC_CONNECTED_COMPONENT_DATA_VERTEX_FLOAT, numBytes, (void*)vertices.data(), NULL);

        if (err != MC_NO_ERROR) {
            fprintf(stderr, "2:mcGetConnectedComponentData(MC_CONNECTED_COMPONENT_DATA_VERTEX_FLOAT) failed (err=%d)\n", (int)err);
            exit(1);
        }

        // get connected component type to determine whether we should flip faces
        // ----------------------------------------------------------------------
        bool flip_faces = true;
        
        {
            McConnectedComponentType type;
            err = mcGetConnectedComponentData(context, connComp, MC_CONNECTED_COMPONENT_DATA_TYPE, sizeof(McConnectedComponentType), (void*)&type, NULL);

            if (err != MC_NO_ERROR) {
                fprintf(stderr, "2:mcGetConnectedComponentData(MC_CONNECTED_COMPONENT_DATA_TYPE) failed (err=%d)\n", (int)err);
                exit(1);
            }

            // in this example we flip the faces of all fragments

            flip_faces = (type == McConnectedComponentType::MC_CONNECTED_COMPONENT_TYPE_FRAGMENT);

            McConnectedComponentFaceWindingOrder windingOrder = McConnectedComponentFaceWindingOrder::MC_CONNECTED_COMPONENT_FACE_WINDING_ORDER_AS_GIVEN;

            if (flip_faces) {
                printf("** Will flip/reverse face indices!!\n");
                windingOrder = McConnectedComponentFaceWindingOrder::MC_CONNECTED_COMPONENT_FACE_WINDING_ORDER_REVERSED;
            }

            err = mcBindState(context, MC_CONTEXT_CONNECTED_COMPONENT_FACE_WINDING_ORDER, sizeof(McConnectedComponentFaceWindingOrder), &windingOrder);

            if (err != MC_NO_ERROR) {
                fprintf(stderr, "mcBindState(MC_CONTEXT_CONNECTED_COMPONENT_FACE_WINDING_ORDER) failed (err=%d)\n", (int)err);
                exit(1);
            }
        }
        // query the faces
        // -------------------

        numBytes = 0;
        err = mcGetConnectedComponentData(context, connComp, MC_CONNECTED_COMPONENT_DATA_FACE, 0, NULL, &numBytes);

        if (err != MC_NO_ERROR) {
            fprintf(stderr, "1:mcGetConnectedComponentData(MC_CONNECTED_COMPONENT_DATA_FACE) failed (err=%d)\n", (int)err);
            exit(1);
        }

        std::vector<uint32_t> faceIndices;
        faceIndices.resize(numBytes / sizeof(uint32_t));

        err = mcGetConnectedComponentData(context, connComp, MC_CONNECTED_COMPONENT_DATA_FACE, numBytes, faceIndices.data(), NULL);

        if (err != MC_NO_ERROR) {
            fprintf(stderr, "2:mcGetConnectedComponentData(MC_CONNECTED_COMPONENT_DATA_FACE) failed (err=%d)\n", (int)err);
            exit(1);
        }

        // query the face sizes
        // ------------------------
        numBytes = 0;
        err = mcGetConnectedComponentData(context, connComp, MC_CONNECTED_COMPONENT_DATA_FACE_SIZE, 0, NULL, &numBytes);
        if (err != MC_NO_ERROR) {
            fprintf(stderr, "1:mcGetConnectedComponentData(MC_CONNECTED_COMPONENT_DATA_FACE_SIZE) failed (err=%d)\n", (int)err);
            exit(1);
        }

        std::vector<uint32_t> faceSizes;
        faceSizes.resize(numBytes / sizeof(uint32_t));

        err = mcGetConnectedComponentData(context, connComp, MC_CONNECTED_COMPONENT_DATA_FACE_SIZE, numBytes, faceSizes.data(), NULL);

        if (err != MC_NO_ERROR) {
            fprintf(stderr, "2:mcGetConnectedComponentData(MC_CONNECTED_COMPONENT_DATA_FACE_SIZE) failed (err=%d)\n", (int)err);
            exit(1);
        }

        char fnameBuf[128];
        sprintf(fnameBuf, "cc%d%s.off", i, (flip_faces ? "-reversed" : "-normal"));

        // save to mesh file (.off)
        // ------------------------
        writeOFF(fnameBuf,
            (float*)vertices.data(),
            (uint32_t*)faceIndices.data(),
            (uint32_t*)faceSizes.data(),
            (uint32_t)vertices.size() / 3,
            (uint32_t)faceSizes.size());
    }

    // 6. free connected component data
    // --------------------------------
    err = mcReleaseConnectedComponents(context, 0, NULL);

    if (err != MC_NO_ERROR) {
        fprintf(stderr, "mcReleaseConnectedComponents failed (err=%d)\n", (int)err);
        exit(1);
    }

    // 7. destroy context
    // ------------------
    err = mcReleaseContext(context);

    if (err != MC_NO_ERROR) {
        fprintf(stderr, "mcReleaseContext failed (err=%d)\n", (int)err);
        exit(1);
    }

    return 0;
}

// write mesh to .off file
void writeOFF(
    const char* fpath,
    float* pVertices,
    uint32_t* pFaceIndices,
    uint32_t* pFaceSizes,
    uint32_t numVertices,
    uint32_t numFaces)
{
    fprintf(stdout, "write: %s\n", fpath);

    FILE* file = fopen(fpath, "w");

    if (file == NULL) {
        fprintf(stderr, "error: failed to open `%s`", fpath);
        exit(1);
    }

    fprintf(file, "OFF\n");
    fprintf(file, "%d %d %d\n", numVertices, numFaces, 0 /*numEdges*/);
    int i;
    for (i = 0; i < (int)numVertices; ++i) {
        float* vptr = pVertices + (i * 3);
        fprintf(file, "%f %f %f\n", vptr[0], vptr[1], vptr[2]);
    }

    int faceBaseOffset = 0;
    for (i = 0; i < (int)numFaces; ++i) {
        uint32_t faceVertexCount = pFaceSizes[i];
        fprintf(file, "%d", (int)faceVertexCount);
        int j;
        for (j = 0; j < (int)faceVertexCount; ++j) {
            uint32_t* fptr = pFaceIndices + faceBaseOffset + j;
            fprintf(file, " %d", *fptr);
        }
        fprintf(file, "\n");
        faceBaseOffset += faceVertexCount;
    }

    fclose(file);
}
