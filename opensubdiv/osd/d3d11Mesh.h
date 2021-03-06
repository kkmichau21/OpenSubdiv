//
//     Copyright (C) Pixar. All rights reserved.
//
//     This license governs use of the accompanying software. If you
//     use the software, you accept this license. If you do not accept
//     the license, do not use the software.
//
//     1. Definitions
//     The terms "reproduce," "reproduction," "derivative works," and
//     "distribution" have the same meaning here as under U.S.
//     copyright law.  A "contribution" is the original software, or
//     any additions or changes to the software.
//     A "contributor" is any person or entity that distributes its
//     contribution under this license.
//     "Licensed patents" are a contributor's patent claims that read
//     directly on its contribution.
//
//     2. Grant of Rights
//     (A) Copyright Grant- Subject to the terms of this license,
//     including the license conditions and limitations in section 3,
//     each contributor grants you a non-exclusive, worldwide,
//     royalty-free copyright license to reproduce its contribution,
//     prepare derivative works of its contribution, and distribute
//     its contribution or any derivative works that you create.
//     (B) Patent Grant- Subject to the terms of this license,
//     including the license conditions and limitations in section 3,
//     each contributor grants you a non-exclusive, worldwide,
//     royalty-free license under its licensed patents to make, have
//     made, use, sell, offer for sale, import, and/or otherwise
//     dispose of its contribution in the software or derivative works
//     of the contribution in the software.
//
//     3. Conditions and Limitations
//     (A) No Trademark License- This license does not grant you
//     rights to use any contributor's name, logo, or trademarks.
//     (B) If you bring a patent claim against any contributor over
//     patents that you claim are infringed by the software, your
//     patent license from such contributor to the software ends
//     automatically.
//     (C) If you distribute any portion of the software, you must
//     retain all copyright, patent, trademark, and attribution
//     notices that are present in the software.
//     (D) If you distribute any portion of the software in source
//     code form, you may do so only under this license by including a
//     complete copy of this license with your distribution. If you
//     distribute any portion of the software in compiled or object
//     code form, you may only do so under a license that complies
//     with this license.
//     (E) The software is licensed "as-is." You bear the risk of
//     using it. The contributors give no express warranties,
//     guarantees or conditions. You may have additional consumer
//     rights under your local laws which this license cannot change.
//     To the extent permitted under your local laws, the contributors
//     exclude the implied warranties of merchantability, fitness for
//     a particular purpose and non-infringement.
//

#ifndef OSD_D3D11MESH_H
#define OSD_D3D11MESH_H

#include "../version.h"

#include "../osd/mesh.h"
#include "../osd/d3d11DrawContext.h"

namespace OpenSubdiv {
namespace OPENSUBDIV_VERSION {

typedef OsdMeshInterface<OsdD3D11DrawContext> OsdD3D11MeshInterface;

template <class VERTEX_BUFFER, class COMPUTE_CONTROLLER>
class OsdMesh<VERTEX_BUFFER, COMPUTE_CONTROLLER, OsdD3D11DrawContext> : public OsdD3D11MeshInterface {
public:
    typedef VERTEX_BUFFER VertexBuffer;
    typedef COMPUTE_CONTROLLER ComputeController;
    typedef typename ComputeController::ComputeContext ComputeContext; 
    typedef OsdD3D11DrawContext DrawContext; 
    typedef typename DrawContext::VertexBufferBinding VertexBufferBinding;

    OsdMesh(HbrMesh<OsdVertex> * hmesh,
            int numElements,
            int level,
            OsdMeshBitset bits,
            ID3D11DeviceContext *d3d11DeviceContext) :

            _farMesh(0),
            _vertexBuffer(0),
            _computeContext(0),
            _computeController(0),
            _drawContext(0),
            _pd3d11DeviceContext(d3d11DeviceContext)
    {
        FarMeshFactory<OsdVertex> meshFactory(hmesh, level, bits.test(MeshAdaptive));
        _farMesh = meshFactory.Create(bits.test(MeshPtexData),
                                      bits.test(MeshFVarData));

        ID3D11Device * pd3d11Device;
        _pd3d11DeviceContext->GetDevice(&pd3d11Device);

        int numVertices = _farMesh->GetNumVertices();
        _vertexBuffer = typename VertexBuffer::Create(numElements, numVertices, pd3d11Device);
        _computeContext = typename ComputeContext::Create(_farMesh);
        _computeController = new ComputeController();
        _drawContext = DrawContext::Create(_farMesh, _vertexBuffer,
                                           _pd3d11DeviceContext,
                                           bits.test(MeshPtexData),
                                           bits.test(MeshFVarData));
    }

    virtual ~OsdMesh() {
        delete _farMesh;
        delete _vertexBuffer;
        delete _computeContext;
        delete _computeController;
        delete _drawContext;
    }

    virtual int GetNumVertices() const { return _farMesh->GetNumVertices(); }

    virtual void UpdateVertexBuffer(float const *vertexData, int numVerts) {
        ID3D11Device * pd3d11Device;
        _pd3d11DeviceContext->GetDevice(&pd3d11Device);
        _vertexBuffer->UpdateData(vertexData, numVerts, pd3d11Device);
    }
    virtual void Refine() {
        _computeController->Refine(_computeContext, _vertexBuffer);
    }
    virtual void Synchronize() {
        _computeController->Synchronize();
    }
    virtual VertexBufferBinding BindVertexBuffer() {
        return _vertexBuffer->BindD3D11Buffer(_pd3d11DeviceContext);
    }
    virtual DrawContext * GetDrawContext() {
        return _drawContext;
    }

private:
    FarMesh<OsdVertex> *_farMesh;
    VertexBuffer *_vertexBuffer;
    ComputeContext *_computeContext;
    ComputeController *_computeController;
    DrawContext *_drawContext;

    ID3D11DeviceContext *_pd3d11DeviceContext;
};

#ifdef OPENSUBDIV_HAS_OPENCL

#if defined(__APPLE__)
    #include <OpenCL/opencl.h>
#else
    #include <CL/opencl.h>
#endif

class OsdCLComputeController;

template <class VERTEX_BUFFER>
class OsdMesh<VERTEX_BUFFER, OsdCLComputeController, OsdD3D11DrawContext> : public OsdD3D11MeshInterface {
public:
    typedef VERTEX_BUFFER VertexBuffer;
    typedef OsdCLComputeController ComputeController; 
    typedef typename ComputeController::ComputeContext ComputeContext; 
    typedef OsdD3D11DrawContext DrawContext; 
    typedef typename DrawContext::VertexBufferBinding VertexBufferBinding; 

    OsdMesh(HbrMesh<OsdVertex> * hmesh,
            int numElements,
            int level,
            OsdMeshBitset bits,
            cl_context clContext,
            cl_command_queue clQueue,
            ID3D11DeviceContext *d3d11DeviceContext) :

            _farMesh(0),
            _vertexBuffer(0),
            _computeContext(0),
            _computeController(0),
            _drawContext(0),
            _clContext(clContext),
            _clQueue(clQueue),
            _pd3d11DeviceContext(d3d11DeviceContext)
    {
        FarMeshFactory<OsdVertex> meshFactory(hmesh, level, bits.test(MeshAdaptive));
        _farMesh = meshFactory.Create(bits.test(MeshPtexData),
                                      bits.test(MeshFVarData));

        ID3D11Device * pd3d11Device;
        _pd3d11DeviceContext->GetDevice(&pd3d11Device);

        int numVertices = _farMesh->GetNumVertices();
        _vertexBuffer = typename VertexBuffer::Create(numElements, numVertices, _clContext, pd3d11Device);
        _computeContext = ComputeContext::Create(_farMesh, _clContext);
        _computeController = new ComputeController(_clContext, _clQueue);
        _drawContext = DrawContext::Create(_farMesh, _vertexBuffer,
                                           _pd3d11DeviceContext,
                                           bits.test(MeshPtexData),
                                           bits.test(MeshFVarData));
    }

    virtual ~OsdMesh() {
        delete _farMesh;
        delete _vertexBuffer;
        delete _computeContext;
        delete _computeController;
        delete _drawContext;
    }

    virtual int GetNumVertices() const { return _farMesh->GetNumVertices(); }

    virtual void UpdateVertexBuffer(float const *vertexData, int numVerts) {
        ID3D11Device * pd3d11Device;
        _pd3d11DeviceContext->GetDevice(&pd3d11Device);
        _vertexBuffer->UpdateData(vertexData, numVerts, _clQueue, pd3d11Device);
    }
    virtual void Refine() {
        _computeController->Refine(_computeContext, _vertexBuffer);
    }
    virtual void Synchronize() {
        _computeController->Synchronize();
    }
    virtual VertexBufferBinding BindVertexBuffer() {
        return _vertexBuffer->BindD3D11Buffer(_pd3d11DeviceContext);
    }
    virtual DrawContext * GetDrawContext() {
        return _drawContext;
    }

private:
    FarMesh<OsdVertex> *_farMesh;
    VertexBuffer *_vertexBuffer;
    ComputeContext *_computeContext;
    ComputeController *_computeController;
    DrawContext *_drawContext;

    cl_context _clContext;
    cl_command_queue _clQueue;

    ID3D11DeviceContext *_pd3d11DeviceContext;
};
#endif

}  // end namespace OPENSUBDIV_VERSION
using namespace OPENSUBDIV_VERSION;

}  // end namespace OpenSubdiv

#endif  // OSD_D3D11MESH_H
