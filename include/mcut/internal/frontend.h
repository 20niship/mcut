/**
 * Copyright (c) 2021-2022 Floyd M. Chitalu.
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

/**
 * @file mcut.h
 * @author Floyd M. Chitalu
 * @date 11 July 2022
 *
 * @brief API-function implementations.
 *
 * NOTE: This header file defines the pre- and post-cutting processing of mesh
 * data, which includes any intermediate correctons/modifications to the user's
 * input meshes like 'polygon partitioning'.
 *
 */

#ifndef _FRONTEND_H_
#define _FRONTEND_H_

#include "mcut/mcut.h"

#include <map>
#include <memory>

namespace frontend {

// internal frontend data structure which we use to store connected component
// data. Information requested by a client/user via "mcGetConnectedComponentData"
// is read from this data structure (halfedge meshes are used by the backend
// kernel)
struct IndexArrayMesh {
    IndexArrayMesh() { }
    ~IndexArrayMesh()
    {
    }

    std::unique_ptr<double[]> pVertices;
    std::unique_ptr<uint32_t[]> pSeamVertexIndices;
    std::unique_ptr<uint32_t[]> pVertexMapIndices; // descriptor/index in original mesh (source/cut-mesh), each vertex has an entry
    std::unique_ptr<uint32_t[]> pFaceIndices;
    std::unique_ptr<uint32_t[]> pFaceMapIndices; // descriptor/index in original mesh (source/cut-mesh), each face has an entry
    std::unique_ptr<uint32_t[]> pFaceSizes;
    std::unique_ptr<uint32_t[]> pEdges;
    std::unique_ptr<uint32_t[]> pFaceAdjFaces;
    std::unique_ptr<uint32_t[]> pFaceAdjFacesSizes;
    std::unique_ptr<uint32_t[]> pTriangleIndices; // same as "pFaceIndices" but guaranteed to be only triangles

    uint32_t numVertices = 0;
    uint32_t numSeamVertexIndices = 0;
    uint32_t numFaces = 0;
    uint32_t numFaceIndices = 0;
    uint32_t numEdgeIndices = 0;
    uint32_t numFaceAdjFaceIndices = 0;
    uint32_t numTriangleIndices = 0;
};

// base struct from which other structs represent connected components inherit
struct McConnCompBase {
    virtual ~McConnCompBase() {};
    McConnectedComponentType type = (McConnectedComponentType)0;
    IndexArrayMesh indexArrayMesh;
};

// struct representing a fragment
struct McFragmentConnComp : public McConnCompBase {
    McFragmentLocation fragmentLocation = (McFragmentLocation)0;
    McFragmentSealType srcMeshSealType = (McFragmentSealType)0;
    McPatchLocation patchLocation = (McPatchLocation)0;
};

// struct representing a patch
struct McPatchConnComp : public McConnCompBase {
    McPatchLocation patchLocation = (McPatchLocation)0;
};

// struct representing a seam
struct McSeamConnComp : public McConnCompBase {
    McSeamOrigin origin = (McSeamOrigin)0;
};

// struct representing an input (user provided mesh)
struct McInputConnComp : public McConnCompBase {
    McInputOrigin origin = (McInputOrigin)0;
};

// our custome deleter function for std::unique_ptr variable of an array type
template <typename Derived>
void ccDeletorFunc(McConnCompBase* p)
{
    delete static_cast<Derived*>(p);
}

// struct defining the state of a context object
struct McDispatchContextInternal {
#if defined(MCUT_MULTI_THREADED)
    // work scheduling state
    mcut::thread_pool scheduler;
#endif

    // the current set of connected components associated with context
    std::map<McConnectedComponent, std::unique_ptr<McConnCompBase, void (*)(McConnCompBase*)>> connComps = {};

    // The state and flag variable current used to configure the next dispatch call
    McFlags flags = (McFlags)0;
    McFlags dispatchFlags = (McFlags)0;

    // client/user debugging variable
    // ------------------------------

    // function pointer to user-define callback function for status/erro reporting
    pfn_mcDebugOutput_CALLBACK debugCallback = nullptr;
    // user provided data for callback
    const void* debugCallbackUserParam = nullptr;

    // TODO: make use of the following three filter inside the log function

    // controller for permmited messages based on the source of message
    McFlags debugSource = 0;
    // controller for permmited messages based on the type of message
    McFlags debugType = 0;
    // controller for permmited messages based on the severity of message
    McFlags debugSeverity = 0;

    void log(McDebugSource source,
        McDebugType type,
        unsigned int id,
        McDebugSeverity severity,
        const std::string& message)
    {
        if (debugCallback != nullptr) {
            (*debugCallback)(source, type, id, severity, message.length(), message.c_str(), debugCallbackUserParam);
        }
    }
};

// list of contexts created by client/user
extern "C" std::map<McContext, std::unique_ptr<McDispatchContextInternal>> gDispatchContexts;

void create_context_impl(
    McContext* pContext, McFlags flags);

void debug_message_callback_impl(
    McContext context,
    pfn_mcDebugOutput_CALLBACK cb,
    const void* userParam);

void debug_message_control_impl(
    McContext context,
    McDebugSource source,
    McDebugType type,
    McDebugSeverity severity,
    bool enabled);

void get_info_impl(
    const McContext context,
    McFlags info,
    uint64_t bytes,
    void* pMem,
    uint64_t* pNumBytes);

void dispatch_impl(
    McContext context,
    McFlags flags,
    const void* pSrcMeshVertices,
    const uint32_t* pSrcMeshFaceIndices,
    const uint32_t* pSrcMeshFaceSizes,
    uint32_t numSrcMeshVertices,
    uint32_t numSrcMeshFaces,
    const void* pCutMeshVertices,
    const uint32_t* pCutMeshFaceIndices,
    const uint32_t* pCutMeshFaceSizes,
    uint32_t numCutMeshVertices,
    uint32_t numCutMeshFaces);

void get_connected_components_impl(
    const McContext context,
    const McConnectedComponentType connectedComponentType,
    const uint32_t numEntries,
    McConnectedComponent* pConnComps,
    uint32_t* numConnComps);

void get_connected_component_data_impl(
    const McContext context,
    const McConnectedComponent connCompId,
    McFlags flags,
    uint64_t bytes,
    void* pMem,
    uint64_t* pNumBytes);

void release_connected_components_impl(
    const McContext context,
    uint32_t numConnComps,
    const McConnectedComponent* pConnComps);

void release_context_impl(
    McContext context);

} // namespace frontend{

#endif // #ifndef _FRONTEND_H_