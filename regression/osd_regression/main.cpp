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

#if defined(__APPLE__)
    #include <GLUT/glut.h>
#else
    #include <stdlib.h>
    #include <GL/glew.h>
    #include <GL/glut.h>
#endif

#include <stdio.h>
#include <cassert>

#include "../common/mutex.h"

#include <far/meshFactory.h>

#include <osd/vertex.h>
#include <osd/cpuVertexBuffer.h>
#include <osd/cpuDispatcher.h>
#include <osd/cpuComputeController.h>
#include <osd/cpuComputeContext.h>

#ifdef OPENSUBDIV_HAS_CUDA
    #include <osd/cudaDispatcher.h>
#endif

#ifdef OPENSUBDIV_HAS_OPENCL
    #include <osd/clDispatcher.h>
#endif

#include "../common/shape_utils.h"

//
// Regression testing matching Osd to Hbr
//
// Notes:
// - precision is currently held at 1e-6
//
// - results cannot be bitwise identical as some vertex interpolations
//   are not happening in the same order.
//
// - only vertex interpolation is being tested at the moment.
//
#define PRECISION 1e-6

//------------------------------------------------------------------------------
// Vertex class implementation
struct xyzVV {

    xyzVV() { }

    xyzVV( int /*i*/ ) { }

    xyzVV( float x, float y, float z ) { _pos[0]=x; _pos[1]=y; _pos[2]=z; }

    xyzVV( const xyzVV & src ) { _pos[0]=src._pos[0]; _pos[1]=src._pos[1]; _pos[2]=src._pos[2]; }

   ~xyzVV( ) { }

    void     AddWithWeight(const xyzVV& src, float weight, void * =0 ) { 
        _pos[0]+=weight*src._pos[0]; 
        _pos[1]+=weight*src._pos[1]; 
        _pos[2]+=weight*src._pos[2]; 
    }

    void     AddVaryingWithWeight(const xyzVV& , float, void * =0 ) { }

    void     Clear( void * =0 ) { _pos[0]=_pos[1]=_pos[2]=0.0f; }

    void     SetPosition(float x, float y, float z) { _pos[0]=x; _pos[1]=y; _pos[2]=z; }

    void     ApplyVertexEdit(const OpenSubdiv::HbrVertexEdit<xyzVV> & edit) {
                 const float *src = edit.GetEdit();
                 switch(edit.GetOperation()) {
                   case OpenSubdiv::HbrHierarchicalEdit<xyzVV>::Set:
                     _pos[0] = src[0];
                     _pos[1] = src[1];
                     _pos[2] = src[2];
                     break;
                   case OpenSubdiv::HbrHierarchicalEdit<xyzVV>::Add:
                     _pos[0] += src[0];
                     _pos[1] += src[1];
                     _pos[2] += src[2];
                     break;
                   case OpenSubdiv::HbrHierarchicalEdit<xyzVV>::Subtract:
                     _pos[0] -= src[0];
                     _pos[1] -= src[1];
                     _pos[2] -= src[2];
                     break;
                 }
             }

    void ApplyVertexEdit(OpenSubdiv::FarVertexEdit const & edit) {
        const float *src = edit.GetEdit();
        switch(edit.GetOperation()) {
          case OpenSubdiv::FarVertexEdit::Set:
            _pos[0] = src[0];
            _pos[1] = src[1];
            _pos[2] = src[2];
            break;
          case OpenSubdiv::FarVertexEdit::Add:
            _pos[0] += src[0];
            _pos[1] += src[1];
            _pos[2] += src[2];
            break;
        }
    }
    
    void     ApplyMovingVertexEdit(const OpenSubdiv::HbrMovingVertexEdit<xyzVV> &) { }

    const float * GetPos() const { return _pos; }

private:
    float _pos[3];
};

//------------------------------------------------------------------------------
class xyzFV;
typedef OpenSubdiv::HbrMesh<xyzVV>           xyzmesh;
typedef OpenSubdiv::HbrFace<xyzVV>           xyzface;
typedef OpenSubdiv::HbrVertex<xyzVV>         xyzvertex;
typedef OpenSubdiv::HbrHalfedge<xyzVV>       xyzhalfedge;
typedef OpenSubdiv::HbrFaceOperator<xyzVV>   xyzFaceOperator;
typedef OpenSubdiv::HbrVertexOperator<xyzVV> xyzVertexOperator;

typedef OpenSubdiv::HbrMesh<OpenSubdiv::OsdVertex>     OsdHbrMesh;
typedef OpenSubdiv::HbrVertex<OpenSubdiv::OsdVertex>   OsdHbrVertex;
typedef OpenSubdiv::HbrFace<OpenSubdiv::OsdVertex>     OsdHbrFace;
typedef OpenSubdiv::HbrHalfedge<OpenSubdiv::OsdVertex> OsdHbrHalfedge;
//------------------------------------------------------------------------------
// Returns true if a vertex or any of its parents is on a boundary
bool VertexOnBoundary( xyzvertex const * v ) {

    if (not v)
        return false;

    if (v->OnBoundary())
        return true;

    xyzvertex const * pv = v->GetParentVertex();
    if (pv)
        return VertexOnBoundary(pv);
    else {
        xyzhalfedge const * pe = v->GetParentEdge();
        if (pe) {
              return VertexOnBoundary(pe->GetOrgVertex()) or
                     VertexOnBoundary(pe->GetDestVertex());
        } else {
            xyzface const * pf = v->GetParentFace(), * rootf = pf;
            while (pf) {
                pf = pf->GetParent();
                if (pf)
                    rootf=pf;
            }
            if (rootf)
                for (int i=0; i<rootf->GetNumVertices(); ++i)
                    if (rootf->GetVertex(i)->OnBoundary())
                        return true;
        }
    }
    return false;
}

//------------------------------------------------------------------------------

int checkVertexBuffer( xyzmesh * hmesh,
                       OpenSubdiv::OsdCpuVertexBuffer * vb,
                       std::vector<int> const & remap) {
    int count=0;
    float deltaAvg[3] = {0.0f, 0.0f, 0.0f},
          deltaCnt[3] = {0.0f, 0.0f, 0.0f};

    int nverts = hmesh->GetNumVertices();
    for (int i=0; i<nverts; ++i) {

        xyzvertex * hv = hmesh->GetVertex(i);

        float * ov = & vb->BindCpuBuffer()[ remap[ hv->GetID() ] * vb->GetNumElements() ];

        // boundary interpolation rules set to "none" produce "undefined" vertices on
        // boundary vertices : far does not match hbr for those, so skip comparison.
        if ( hmesh->GetInterpolateBoundaryMethod()==xyzmesh::k_InterpolateBoundaryNone and
             VertexOnBoundary(hv) )
             continue;


        if ( hv->GetData().GetPos()[0] != ov[0] )
            deltaCnt[0]++;
        if ( hv->GetData().GetPos()[1] != ov[1] )
            deltaCnt[1]++;
        if ( hv->GetData().GetPos()[2] != ov[2] )
            deltaCnt[2]++;

        float delta[3] = { hv->GetData().GetPos()[0] - ov[0],
                           hv->GetData().GetPos()[1] - ov[1],
                           hv->GetData().GetPos()[2] - ov[2] };

        deltaAvg[0]+=delta[0];
        deltaAvg[1]+=delta[1];
        deltaAvg[2]+=delta[2];

        float dist = sqrtf( delta[0]*delta[0]+delta[1]*delta[1]+delta[2]*delta[2]);
        if ( dist > PRECISION ) {
            printf("// HbrVertex<T> %d fails : dist=%.10f (%.10f %.10f %.10f)"
                   " (%.10f %.10f %.10f)\n", i, dist, hv->GetData().GetPos()[0],
                                                      hv->GetData().GetPos()[1],
                                                      hv->GetData().GetPos()[2],
                                                      ov[0],
                                                      ov[1],
                                                      ov[2] );
           count++;
        }
    }

    if (deltaCnt[0])
        deltaAvg[0]/=deltaCnt[0];
    if (deltaCnt[1])
        deltaAvg[1]/=deltaCnt[1];
    if (deltaCnt[2])
        deltaAvg[2]/=deltaCnt[2];

    printf("    delta ratio : (%d/%d %d/%d %d/%d)\n", (int)deltaCnt[0], nverts,
                                                      (int)deltaCnt[1], nverts,
                                                      (int)deltaCnt[2], nverts );
    printf("    average delta : (%.10f %.10f %.10f)\n", deltaAvg[0],
                                                        deltaAvg[1],
                                                        deltaAvg[2] );
    if (count==0)
        printf("  success !\n");

    return count;
}

//------------------------------------------------------------------------------
static void refine( xyzmesh * mesh, int maxlevel ) {

    for (int l=0; l<maxlevel; ++l ) {
        int nfaces = mesh->GetNumFaces();
        for (int i=0; i<nfaces; ++i) {
            xyzface * f = mesh->GetFace(i);
            if (f->GetDepth()==l)
                f->Refine();
        }
    }

}

//------------------------------------------------------------------------------
int checkMesh( char const * msg, char const * shape, int levels, Scheme scheme=kCatmark ) {

    int result =0;

    printf("- %s (scheme=%d)\n", msg, scheme);

    xyzmesh * refmesh = simpleHbr<xyzVV>(shape, scheme, 0);

    refine( refmesh, levels );


    std::vector<float> coarseverts;

    OsdHbrMesh * hmesh = simpleHbr<OpenSubdiv::OsdVertex>(shape, scheme, coarseverts);

    OpenSubdiv::FarMesh<OpenSubdiv::OsdVertex> *farmesh;
    OpenSubdiv::FarMeshFactory<OpenSubdiv::OsdVertex> meshFactory(hmesh, levels);

    farmesh = meshFactory.Create();

    static OpenSubdiv::OsdCpuComputeController *controller =
        new OpenSubdiv::OsdCpuComputeController();

    OpenSubdiv::OsdCpuComputeContext *context = OpenSubdiv::OsdCpuComputeContext::Create(farmesh);
    
    std::vector<int> remap = meshFactory.GetRemappingTable();
    {
        OpenSubdiv::OsdCpuVertexBuffer * vb = OpenSubdiv::OsdCpuVertexBuffer::Create(3, farmesh->GetNumVertices());

        vb->UpdateData( & coarseverts[0], (int)coarseverts.size()/3 );

        controller->Refine( context, vb );

        checkVertexBuffer(refmesh, vb, remap);
    }

    delete hmesh;

    return result;
}

//------------------------------------------------------------------------------
int main(int argc, char ** argv) {

    // Make sure we have an OpenGL context.
    glutInit(&argc, argv);
    glutCreateWindow("osd_regression");
    glewInit();

    int levels=5, total=0;

#define test_catmark_edgeonly
#define test_catmark_edgecorner
#define test_catmark_pyramid
#define test_catmark_pyramid_creases0
#define test_catmark_pyramid_creases1
#define test_catmark_cube
#define test_catmark_cube_creases0
#define test_catmark_cube_creases1
#define test_catmark_cube_corner0
#define test_catmark_cube_corner1
#define test_catmark_cube_corner2
#define test_catmark_cube_corner3
#define test_catmark_cube_corner4
#define test_catmark_dart_edgeonly
#define test_catmark_dart_edgecorner
#define test_catmark_tent
#define test_catmark_tent_creases0
#define test_catmark_tent_creases1
#define test_catmark_square_hedit0
#define test_catmark_square_hedit1
#define test_catmark_square_hedit2
#define test_catmark_square_hedit3

#define test_loop_triangle_edgeonly
#define test_loop_triangle_edgecorner
#define test_loop_icosahedron
#define test_loop_cube
#define test_loop_cube_creases0
#define test_loop_cube_creases1

#define test_bilinear_cube

    printf("precision : %f\n",PRECISION);

#ifdef test_catmark_edgeonly
#include "../shapes/catmark_edgeonly.h"
    total += checkMesh( "test_catmark_edgeonly", catmark_edgeonly, levels, kCatmark );
#endif

#ifdef test_catmark_edgecorner
#include "../shapes/catmark_edgecorner.h"
    total += checkMesh( "test_catmark_edgeonly", catmark_edgecorner, levels, kCatmark );
#endif

#ifdef test_catmark_pyramid
#include "../shapes/catmark_pyramid.h"
    total += checkMesh( "test_catmark_pyramid", catmark_pyramid, levels, kCatmark );
#endif

#ifdef test_catmark_pyramid_creases0
#include "../shapes/catmark_pyramid_creases0.h"
    total += checkMesh( "test_catmark_pyramid_creases0", catmark_pyramid_creases0, levels, kCatmark );
#endif

#ifdef test_catmark_pyramid_creases1
#include "../shapes/catmark_pyramid_creases1.h"
    total += checkMesh( "test_catmark_pyramid_creases1", catmark_pyramid_creases1, levels, kCatmark );
#endif

#ifdef test_catmark_cube
#include "../shapes/catmark_cube.h"
    total += checkMesh( "test_catmark_cube", catmark_cube, levels, kCatmark );
#endif

#ifdef test_catmark_cube_creases0
#include "../shapes/catmark_cube_creases0.h"
    total += checkMesh( "test_catmark_cube_creases0", catmark_cube_creases0, levels, kCatmark );
#endif

#ifdef test_catmark_cube_creases1
#include "../shapes/catmark_cube_creases1.h"
    total += checkMesh( "test_catmark_cube_creases1", catmark_cube_creases1, levels, kCatmark );
#endif

#ifdef test_catmark_cube_corner0
#include "../shapes/catmark_cube_corner0.h"
    total += checkMesh( "test_catmark_cube_corner0", catmark_cube_corner0, levels, kCatmark );
#endif

#ifdef test_catmark_cube_corner1
#include "../shapes/catmark_cube_corner1.h"
    total += checkMesh( "test_catmark_cube_corner1", catmark_cube_corner1, levels, kCatmark );
#endif

#ifdef test_catmark_cube_corner2
#include "../shapes/catmark_cube_corner2.h"
    total += checkMesh( "test_catmark_cube_corner2", catmark_cube_corner2, levels, kCatmark );
#endif

#ifdef test_catmark_cube_corner3
#include "../shapes/catmark_cube_corner3.h"
    total += checkMesh( "test_catmark_cube_corner3", catmark_cube_corner3, levels, kCatmark );
#endif

#ifdef test_catmark_cube_corner4
#include "../shapes/catmark_cube_corner4.h"
    total += checkMesh( "test_catmark_cube_corner4", catmark_cube_corner4, levels, kCatmark );
#endif

#ifdef test_catmark_dart_edgecorner
#include "../shapes/catmark_dart_edgecorner.h"
    total += checkMesh( "test_catmark_dart_edgecorner", catmark_dart_edgecorner, levels, kCatmark );
#endif

#ifdef test_catmark_dart_edgeonly
#include "../shapes/catmark_dart_edgeonly.h"
    total += checkMesh( "test_catmark_dart_edgeonly", catmark_dart_edgeonly, levels, kCatmark );
#endif

#ifdef test_catmark_tent
#include "../shapes/catmark_tent.h"
    total += checkMesh( "test_catmark_tent", catmark_tent, levels, kCatmark );
#endif

#ifdef test_catmark_tent_creases0
#include "../shapes/catmark_tent_creases0.h"
    total += checkMesh( "test_catmark_tent_creases0", catmark_tent_creases0, levels );
#endif

#ifdef test_catmark_tent_creases1
#include "../shapes/catmark_tent_creases1.h"
    total += checkMesh( "test_catmark_tent_creases1", catmark_tent_creases1, levels );
#endif

#ifdef test_catmark_square_hedit0
#include "../shapes/catmark_square_hedit0.h"
    total += checkMesh( "test_catmark_square_hedit0", catmark_square_hedit0, levels );
#endif

#ifdef test_catmark_square_hedit1
#include "../shapes/catmark_square_hedit1.h"
    total += checkMesh( "test_catmark_square_hedit1", catmark_square_hedit1, levels );
#endif

#ifdef test_catmark_square_hedit2
#include "../shapes/catmark_square_hedit2.h"
    total += checkMesh( "test_catmark_square_hedit2", catmark_square_hedit2, levels );
#endif

#ifdef test_catmark_square_hedit3
#include "../shapes/catmark_square_hedit3.h"
    total += checkMesh( "test_catmark_square_hedit3", catmark_square_hedit3, levels );
#endif


#ifdef test_loop_triangle_edgeonly
#include "../shapes/loop_triangle_edgeonly.h"
    total += checkMesh( "test_loop_triangle_edgeonly", loop_triangle_edgeonly, levels, kLoop );
#endif

#ifdef test_loop_triangle_edgecorner
#include "../shapes/loop_triangle_edgecorner.h"
    total += checkMesh( "test_loop_triangle_edgecorner", loop_triangle_edgecorner, levels, kLoop );
#endif

#ifdef test_loop_saddle_edgeonly
#include "../shapes/loop_saddle_edgeonly.h"
    total += checkMesh( "test_loop_saddle_edgeonly", loop_saddle_edgeonly, levels, kLoop );
#endif

#ifdef test_loop_saddle_edgecorner
#include "../shapes/loop_saddle_edgecorner.h"
    total += checkMesh( "test_loop_saddle_edgecorner", loop_saddle_edgecorner, levels, kLoop );
#endif

#ifdef test_loop_icosahedron
#include "../shapes/loop_icosahedron.h"
    total += checkMesh( "test_loop_icosahedron", loop_icosahedron, levels, kLoop );
#endif

#ifdef test_loop_cube
#include "../shapes/loop_cube.h"
    total += checkMesh( "test_loop_cube", loop_cube, levels, kLoop );
#endif

#ifdef test_loop_cube_creases0
#include "../shapes/loop_cube_creases0.h"
    total += checkMesh( "test_loop_cube_creases0", loop_cube_creases0,levels, kLoop );
#endif

#ifdef test_loop_cube_creases1
#include "../shapes/loop_cube_creases1.h"
    total += checkMesh( "test_loop_cube_creases1", loop_cube_creases1, levels, kLoop );
#endif



#ifdef test_bilinear_cube
#include "../shapes/bilinear_cube.h"
    total += checkMesh( "test_bilinear_cube", bilinear_cube, levels, kBilinear );
#endif

    if (total==0)
      printf("All tests passed.\n");
    else
      printf("Total failures : %d\n", total);
}
