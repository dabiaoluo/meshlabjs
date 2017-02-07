#ifndef COARSEISOTROPICREMESHING_H
#define COARSEISOTROPICREMESHING_H
#endif // COARSEISOTROPICREMESHING_H
#include<vcg/complex/algorithms/update/quality.h>
#include<vcg/complex/algorithms/update/curvature.h>
#include<vcg/complex/algorithms/update/normal.h>
#include<vcg/complex/algorithms/refine.h>
#include<vcg/complex/algorithms/stat.h>
#include<vcg/complex/algorithms/smooth.h>
#include<vcg/complex/algorithms/local_optimization/tri_edge_collapse.h>
#include<vcg/space/index/spatial_hashing.h>

using namespace vcg;
using namespace std;

typedef  face::Pos<MyFace> MyPos;
typedef  BasicVertexPair<MyVertex> MyPair;
typedef  EdgeCollapser<MyMesh, BasicVertexPair<MyVertex>> MyCollapser;
typedef  GridStaticPtr<MyFace, float> MyGrid;
typedef  MyMesh::PerVertexAttributeHandle<int> CornerMap;

struct Params {
    float maxLength;
    float minLength;
    float lengthThr;
    float creaseThr;
    bool adapt;

    Params(bool adapt, float length, float lengthThr, float creaseThr) :
        adapt(adapt), maxLength((4.f/3.f)*length), minLength((4.f/5.f)*length),
        lengthThr(lengthThr), creaseThr(math::Cos(math::ToRad(creaseThr)))
    {}
};


// My lerp (vcg does not implement lerp on scalars as of 30-01-2017)
inline float lerp (float a, float b, float lambda)
{
    math::Clamp(lambda, 0.f, 1.f);
    return a * lambda + (1-lambda) * b;
}
// this returns the value of cos(a) where a is the angle between n0 and n1. (scalar prod is cos(a))
inline float fastAngle(Point3f n0, Point3f n1)
{
    return math::Clamp(n0*n1, -1.f, 1.f);
}
// compare the value of the scalar prod with the cos of the crease threshold
inline bool testCreaseEdge(MyPos &p, float creaseThr)
{
    float angle = fastAngle(NormalizedTriangleNormal(*(p.F())), NormalizedTriangleNormal(*(p.FFlip())));
    return (angle <= creaseThr && angle >= -creaseThr);
}

//riscrivi con vfiterator e iterando solo sui vertici
void computeQuality(MyMesh &m)
{
    tri::UpdateFlags<MyMesh>::VertexClearV(m);
    for(auto fi=m.face.begin(); fi!=m.face.end(); ++fi)
        if(!(*fi).IsD())
        {
            Point3f fNormal = NormalizedTriangleNormal(*fi);
            for(auto i=0; i<3; ++i)
                if(!(*fi).V(i)->IsV())
                {
                    MyVertex *v = (*fi).V(i);
                    vector<MyFace*> ff;
                    vector<int> vi;

                    face::VFStarVF<MyFace>(v, ff, vi);

                    float tot = 0;
                    for(MyFace *f: ff)
                        if(f != &*fi)
                        {
                            tot+= 1-math::Abs(fastAngle(fNormal, NormalizedTriangleNormal(*f)));
                        }
                    v->Q() = tot / (float)(std::max(1, ((int)ff.size()-1)));
                    v->SetV();
                }
        }
}
//here i could also compute corners by using a temp vertexhandle to store the num of incident edges on a vert
//and a bitflag to store the corner bit on vertices
void computeVertexCrease(MyMesh &m, float creaseThr, int creaseBitFlag)
{
    tri::UpdateFlags<MyMesh>::VertexClear(m, creaseBitFlag);
    tri::UpdateFlags<MyMesh>::FaceClearV(m);

    for(auto fi=m.face.begin(); fi!=m.face.end(); ++fi)
        if(!(*fi).IsD())
        {
            for(auto i=0; i<3; ++i)
            {
                MyPos pi(&*fi, i);
                if(!pi.FFlip()->IsV()) // edge not visited already
                {
                    if(testCreaseEdge(pi, creaseThr))
                    {
                        pi.V()->SetUserBit(creaseBitFlag);
                        pi.VFlip()->SetUserBit(creaseBitFlag);
                    }
                }
            }
            (*fi).SetV();
        }
}

void computeVertexCreaseAndCorner(MyMesh &m, float creaseThr, int creaseBitFlag)
{
    tri::UpdateSelection<MyMesh>::VertexClear(m);
    tri::UpdateFlags<MyMesh>::VertexClear(m, creaseBitFlag);
    CornerMap h = tri::Allocator<MyMesh>::GetPerVertexAttribute<int>(m);

    for(auto vi=m.vert.begin(); vi!=m.vert.end(); ++vi)
        h[vi] = 0;

    for(auto fi=m.face.begin(); fi!=m.face.end(); ++fi)
        if(!(*fi).IsD())
        {
            for(auto i=0; i<3; ++i)
            {
                MyPos pi(&*fi, i);

                if(testCreaseEdge(pi, creaseThr))
                {
                    pi.V()->SetUserBit(creaseBitFlag);
                    pi.VFlip()->SetUserBit(creaseBitFlag);
                    ++h[tri::Index(m, pi.V())];
                }
                pi.FlipE();            // need to flip edge to visit all the incident edges for each vert
                if(testCreaseEdge(pi, creaseThr))
                {
                    pi.V()->SetUserBit(creaseBitFlag);
                    pi.VFlip()->SetUserBit(creaseBitFlag);
                    ++h[tri::Index(m, pi.V())];
                }
            }
        }
    //in a manifold mesh every edge is share by 2 faces => every crease adds 2 to its vertices handles
    //if a vertex has more than 2 incident crease edges is a corner => if h[vi] > 4 it is corner
    for(auto vi=m.vert.begin(); vi!=m.vert.end(); ++vi)
        if(h[vi] > 4)
            (*vi).SetS();

}
//same as before... won't probably use this version..
void selectCreaseCorners(MyMesh &m, float creaseThr)
{
    tri::UpdateSelection<MyMesh>::VertexClear(m);
    CornerMap h = tri::Allocator<MyMesh>::GetPerVertexAttribute<int>(m);

    for(auto vi=m.vert.begin(); vi!=m.vert.end(); ++vi)
        h[vi] = 0;

    for(auto fi=m.face.begin(); fi!=m.face.end(); ++fi)
        if(!(*fi).IsD())
        {
            for(auto i=0; i<3; ++i)
            {
                MyPos pi(&*fi, i);

                if(testCreaseEdge(pi, creaseThr))
                    ++h[tri::Index(m, pi.V())];

                pi.FlipE();

                if(testCreaseEdge(pi, creaseThr))
                    ++h[tri::Index(m, pi.V())];
            }
        }
    //now every crease added 2 to the vertex handle => if h[i] > 4 then corner
    for(auto vi=m.vert.begin(); vi!=m.vert.end(); ++vi)
        if(h[vi] > 4)
            (*vi).SetS();
}

/*
 Computes the ideal valence for the vertex in pos p:
 4 for border vertices
 6 for internal vertices
*/
inline int idealValence(MyPos &p)
{
    if(p.IsBorder()) return 4;
    return 6;
}
inline int idealValence(MyVertex &v)
{
    if(v.IsB()) return 4;
    return 6;
}

float computeMeanValence(MyMesh &m)
{
    tri::UpdateTopology<MyMesh>::FaceFace(m);
    tri::UpdateFlags<MyMesh>::VertexClearV(m);
    int total = 0, count = 0, totalOff = 0;

    for(auto fi=m.face.begin(); fi!=m.face.end(); ++fi)
        if(!(*fi).IsD())
        {
            for(int i=0; i<3; ++i)
            {
                MyPos p(&*fi, i);
                if(!p.V()->IsV())
                {
                    total += p.NumberOfIncidentVertices();
                    totalOff += abs(idealValence(p)-p.NumberOfIncidentVertices());
                    p.V()->SetV();
                    ++count;
                }
            }
        }
    printf("MEAN IDEAL VALENCE OFFSET: %.15f\n", ((float)totalOff/(float)count));
    return (float) total / (float) count;
}
/*
    Edge Swap Step:
    This method optimizes the valence of each vertex.
    oldDist is the sum of the absolute distance of each vertex from its ideal valence
    newDist is the sum of the absolute distance of each vertex from its ideal valence after
    the edge swap.
    If the swap decreases the total absolute distance, then it's applied, preserving the triangle
    quality.                        +1
            v1                    v1
           / \                   /|\
          /   \                 / | \
         /     \               /  |  \
        /    _*p\           -1/   |   \ -1
      v2--------v0 ========> v2   |   v0
       \        /             \   |   /
        \      /               \  |  /
         \    /                 \ | /
          \  /                   \|/ +1
           v3                     v3
        Before Swap             After Swap
*/

bool testSwap(MyPos p, float creaseThr)
{
    //if border or feature, do not swap
    if(p.IsBorder() || testCreaseEdge(p, creaseThr)) return false;

    int oldDist = 0, newDist = 0, idealV, actualV;

    MyPos tp=p;

    MyVertex *v0=tp.V();
    idealV  = idealValence(tp); actualV = tp.NumberOfIncidentVertices();
    oldDist += abs(idealV - actualV); newDist += abs(idealV - (actualV - 1));

    tp.FlipF();tp.FlipE();tp.FlipV();
    MyVertex *v1=tp.V();
    idealV  = idealValence(tp); actualV = tp.NumberOfIncidentVertices();
    oldDist += abs(idealV - actualV); newDist += abs(idealV - (actualV + 1));

    tp.FlipE();tp.FlipV();tp.FlipE();
    MyVertex *v2=tp.V();
    idealV  = idealValence(tp); actualV = tp.NumberOfIncidentVertices();
    oldDist += abs(idealV - actualV); newDist += abs(idealV - (actualV - 1));

    tp.FlipF();tp.FlipE();tp.FlipV();
    MyVertex *v3=tp.V();
    idealV  = idealValence(tp); actualV = tp.NumberOfIncidentVertices();
    oldDist += abs(idealV - actualV); newDist += abs(idealV - (actualV + 1));

    float qOld = std::min(Quality(v0->P(),v2->P(),v3->P()),Quality(v0->P(),v1->P(),v2->P()));
    float qNew = std::min(Quality(v0->P(),v1->P(),v3->P()),Quality(v2->P(),v3->P(),v1->P()));

    return (newDist < oldDist && qNew >= qOld * 0.50f) ||
            (newDist == oldDist && qNew > qOld * 1.f) || qNew > 1.5f * qOld;
}

// Edge swap step: edges are flipped in order to optimize valence and triangle quality across the mesh
void ImproveValence(MyMesh &m, Params params)
{
    tri::UpdateTopology<MyMesh>::FaceFace(m); //collapser does not update FF

    tri::UpdateFlags<MyMesh>::FaceClearV(m);
    int swapCnt=0;
    for(auto fi=m.face.begin();fi!=m.face.end();++fi)
        if(!(*fi).IsD())
        {
            for(int i=0;i<3;++i)
            {
                MyPos pi(&*fi,i);
                if(!pi.FFlip()->IsV())
                    if(testSwap(pi, params.creaseThr) &&
                            face::CheckFlipEdgeNormal(*fi, i, math::ToRad(10.f)) &&
                            face::CheckFlipEdge(*fi,i) )
                    {
                        face::FlipEdge(*fi,i);
                        swapCnt++;
                    }
            }
            fi->SetV();
        }
    printf("Performed %i swaps\n",swapCnt);
}

// The predicate that defines which edges should be split
class EdgeSplitPred
{
public:
    int count = 0;
    float length, lengthThr, minQ, maxQ;
    bool adapt;
    bool operator()(MyPos &ep)
    { //could adjust mult on corners..accumulation of vertices is cause of nnonmanifoldnes
        float mult = (adapt)? lerp(0.5f, 1.5f, (((math::Abs(ep.V()->Q())+math::Abs(ep.VFlip()->Q()))/2.f)/(maxQ-minQ))) : 1.f;
        float dist = Distance(ep.V()->P(), ep.VFlip()->P());
        //if the edge length is less the lengthThr don't split anymore
        if(dist > lengthThr && dist > mult*length)
        {
            ++count;
            return true;
        }
        else
            return false;
    }
};

void SplitLongEdges(MyMesh &m, Params &params)
{
    tri::UpdateTopology<MyMesh>::FaceFace(m);

    float minQ,maxQ;

    if(params.adapt) //if not adaptive, do not compute quality
    {
        Distribution<float> distr;
        tri::Stat<MyMesh>::ComputePerVertexQualityDistribution(m,distr);

        maxQ = distr.Percentile(0.9f);
        minQ = distr.Percentile(0.1f);
    }

    tri::MidPoint<MyMesh> midFunctor(&m);
    EdgeSplitPred ep;
    ep.minQ      = minQ;
    ep.maxQ      = maxQ;
    ep.adapt     = params.adapt;
    ep.length    = params.maxLength;
    ep.lengthThr = params.lengthThr;
    //RefineE updates FF topology after doing the refine (not needed in collapse then)
    tri::RefineE(m,midFunctor,ep);
    printf("Split done: splitted %d edges\n",ep.count);
}

//Geometric check on feasibility of the collapse.
//The check fails if:
//  -new face has too bad quality.
//  -new face normal diverges too much after collapse.
//  -new face has too long edges.
// TRY: if the vertex has valence 4 (cross vertex) we relax the check on length
bool checkFacesAroundVert(MyPos &p, Point3f &mp, float length=0, bool relaxed=false)
{
    vector<MyFace*> ff;
    vector<int> vi;

    face::VFStarVF<MyFace>(p.V(), ff, vi);

    for(MyFace *f: ff)
        if(!(*f).IsD() && f != p.F()) //i'm not a deleted face
        {
            MyPos pi(f, p.V()); //same vertex

            MyVertex *v0 = pi.V();
            MyVertex *v1 = pi.F()->V1(pi.VInd());
            MyVertex *v2 = pi.F()->V2(pi.VInd());

            if( v1 == p.VFlip() || v2 == p.VFlip()) //i'm the other deleted face
                continue;

            //quality and normal divergence checks
            float newQ = Quality(mp, v1->P(), v2->P());
            float oldQ = Quality(v0->P(), v1->P(), v2->P());

            if(newQ <= 0.5*oldQ)
                return false;

            Point3f oldN = NormalizedTriangleNormal(*(pi.F()));
            Point3f newN = Normal(mp, v1->P(), v2->P()).Normalize();
            float div = fastAngle(oldN, newN);
            float thr = math::Cos(math::ToRad(2.5f));

            if(div <= thr && div >= -thr)
                return false;

            //here if the vertex is a cross vert we skip the check on length, to ease the collapsing of crosses
            if(!relaxed)
                if(Distance(mp, v1->P()) > length || Distance(mp, v2->P()) > length)
                    return false;
        }
    return true;
}

// Collapse test: Usual collapse test plus borders and adaptivity management
bool testCollapse(MyPos &p, Point3f &mp, float minQ, float maxQ, Params &params)
{
    float mult = (params.adapt) ? lerp(0.5f, 1.5f, (((math::Abs(p.V()->Q())+math::Abs(p.VFlip()->Q()))/2.f)/(maxQ-minQ))) : 1.f;
    float dist = Distance(p.V()->P(), p.VFlip()->P());
    float thr  = std::max(mult*params.minLength, params.lengthThr);

    if(dist < thr)//if to collapse
    {
        if(!checkFacesAroundVert(p, mp, mult*params.maxLength))
            return false;

        p.FlipV();

        if(!checkFacesAroundVert(p, mp, mult*params.maxLength))
            return false;

        return true;
    }
    return false;
}
//Feature preserving collapse step
void CollapseShortEdges(MyMesh &m, Params &params)
{
    float minQ, maxQ;
    int count = 0, candidates = 0;

    if(params.adapt)
    {
        Distribution<float> distr;
        tri::Stat<MyMesh>::ComputePerVertexQualityDistribution(m,distr);


        maxQ = distr.Percentile(0.9f);
        minQ = distr.Percentile(0.1f);
    }

    tri::UpdateTopology<MyMesh>::VertexFace(m);
    tri::UpdateFlags<MyMesh>::VertexBorderFromNone(m);
    for(auto fi=m.face.begin(); fi!=m.face.end(); ++fi)
        if(!(*fi).IsD())
        {
            for(auto i=0; i<3; ++i)
            {
                MyPos pi(&*fi, i);
                if(!pi.V()->IsB() && !pi.VFlip()->IsB())
                {
                    ++candidates;
                    MyPair bp(pi.V(), pi.VFlip());
                    Point3f mp = (bp.V(1)->P()+bp.V(0)->P())/2.f;

                    if(testCollapse(pi, mp, minQ, maxQ, params) && MyCollapser::LinkConditions(bp))
                    {
                        MyCollapser::Do(m, bp, mp);
                        ++count;
                        break;
                    }
                }
            }
        }
    printf("Collapse candidate edges: %d\n", candidates);
    printf("Collapsed edges: %d\n", count);
    //compact vectors, since we killed vertices
    Allocator<MyMesh>::CompactEveryVector(m);
}

//Here I just need to check the faces of the cross, since the other faces are not
//affected by the collapse of the internal faces of the cross.
bool testCrossCollapse(MyPos &p, Point3f &mp)
{
    if(!checkFacesAroundVert(p, mp, 0, true))
        return false;
    return true;
}

//Choose the best way to collapse a cross based on the (external) cross vertices valence
//and resulting face quality
//                                      +0                   -1
//             v1                    v1                    v1
//            /| \                   /|\                  / \
//           / |  \                 / | \                /   \
//          /  |   \               /  |  \              /     \
//         / *p|    \           -1/   |   \ -1       +0/       \+0
//       v0-------- v2 ========> v0   |   v2    OR    v0-------v2
//        \    |    /             \   |   /            \       /
//         \   |   /               \  |  /              \     /
//          \  |  /                 \ | /                \   /
//           \ | /                   \|/ +0               \ / -1
//             v3                     v3                   v3

MyPair chooseBestCrossCollapse(MyPos &p, vector<MyFace*> &ff)
{
    vector<MyVertex*> vv0, vv1, vv2, vv3;
    MyVertex *v0, *v1, *v2, *v3;

    v0 = p.F()->V1(p.VInd());
    v1 = p.F()->V2(p.VInd());

    for(MyFace *f: ff)
        if(!(*f).IsD() && f != p.F())
        {
            MyPos pi(f, p.V());
            MyVertex *fv1 = pi.F()->V1(pi.VInd());
            MyVertex *fv2 = pi.F()->V2(pi.VInd());

            if(fv1 == v0 || fv2 == v0)
                v3 = (fv1 == v0) ? fv2 : fv1;
            if(fv1 == v1 || fv2 == v1)
                v2 = (fv1 == v1) ? fv2 : fv1;
        }

    face::VVStarVF<MyFace>(v0, vv0);
    face::VVStarVF<MyFace>(v1, vv1);
    face::VVStarVF<MyFace>(v2, vv2);
    face::VVStarVF<MyFace>(v3, vv3);


    int nv0 = vv0.size(), nv1 = vv1.size();
    int nv2 = vv2.size(), nv3 = vv3.size();

    int delta1 = (idealValence(*v0) - nv0) + (idealValence(*v2) - nv2);
    int delta2 = (idealValence(*v1) - nv1) + (idealValence(*v3) - nv3);

    float Q1 = std::min(Quality(v0->P(), v1->P(), v3->P()), Quality(v1->P(), v2->P(), v3->P()));
    float Q2 = std::min(Quality(v0->P(), v1->P(), v2->P()), Quality(v2->P(), v3->P(), v0->P()));

    if(delta1 < delta2 && Q1 >= 0.6f*Q2)
        return MyPair(p.V(), v1);
    else
        return MyPair(p.V(), v0);
}

void CollapseCrosses(MyMesh &m , Params &params)
{
    tri::UpdateTopology<MyMesh>::VertexFace(m);
    int count = 0;
    for(auto fi=m.face.begin(); fi!=m.face.end(); ++fi)
        if(!(*fi).IsD())
        {
            for(auto i=0; i<3; ++i)
            {
                MyPos pi(&*fi, i);
                if(!pi.V()->IsB() && !pi.F()->V1(pi.VInd())->IsB() && !pi.F()->V2(pi.VInd())->IsB())
                {
                    vector<MyFace*> ff;
                    vector<int> vi;
                    face::VFStarVF<MyFace>(pi.V(), ff, vi);

                    //removing crosses only
                    if(ff.size() == 4)
                    {
                        MyPair bp  = chooseBestCrossCollapse(pi, ff);
                        Point3f mp = bp.V(1)->P();
                        //todo: think about if you should try doing the other collapse if test or link fails for this one
                        if(testCrossCollapse(pi, mp) && MyCollapser::LinkConditions(bp))
                        {
                            MyCollapser::Do(m, bp, mp);
                            ++count;
                            break;
                        }
                    }
                }
            }
        }
    printf("Collapsed crosses: %d\n", count);
    //compact vectors, since we killed vertices
    Allocator<MyMesh>::CompactEveryVector(m);
}

// This function sets the selection bit on vertices that lie on creases
int selectVertexFromCrease(MyMesh &m, float creaseThr)
{
    tri::UpdateFlags<MyMesh>::FaceClearV(m);
    int count = 0;
    for(auto fi=m.face.begin(); fi!=m.face.end(); ++fi)
        if(!(*fi).IsD())
        {
            for(int i=0; i<3; ++i)
            {
                MyPos p(&*fi, i);
                //if edge is not already visited and is of crease
                if(!(p.FFlip()->IsV()) && testCreaseEdge(p, creaseThr))
                {
                    p.V()->SetS();
                    p.VFlip()->SetS();
                    ++count;
                }
            }
            (*fi).SetV();
        }
    return count;
}

/*
    Simple Laplacian Smoothing step - Border and crease vertices are ignored.
*/
void ImproveByLaplacian(MyMesh &m, Params params, bool DEBUGCREASE)
{
    tri::UpdateTopology<MyMesh>::FaceFace(m);
    tri::UpdateFlags<MyMesh>::VertexBorderFromFaceAdj(m);
    tri::UpdateSelection<MyMesh>::VertexFromBorderFlag(m);
    selectVertexFromCrease(m, params.creaseThr);
    int i = tri::UpdateSelection<MyMesh>::VertexInvert(m);
    tri::Smooth<MyMesh>::VertexCoordPlanarLaplacian(m,1,math::ToRad(10.f),true);
    printf("Laplacian done (selected vertices: %d)\n", i);
    if(!DEBUGCREASE)
        tri::UpdateSelection<MyMesh>::Clear(m);
}
/*
    Reprojection step, this method reprojects each vertex on the original surface
    sampling the nearest point onto it using a uniform grid MyGrid t
*/
void ProjectToSurface(MyMesh &m, MyGrid t, FaceTmark<MyMesh> mark)
{
    face::PointDistanceBaseFunctor<float> distFunct;
    float maxDist = 100.f, minDist = 0.f;
    int cnt = 0;
    for(auto vi=m.vert.begin();vi!=m.vert.end();++vi)
        if(!(*vi).IsD())
        {
            Point3f newP;
            t.GetClosest(distFunct, mark, vi->P(), maxDist, minDist, newP);
            vi->P() = newP;
            ++cnt;
        }
    printf("projected %d\n", cnt);
}
/*
 * TODO: Think about using tri::Clean<MyMesh>::ComputeValence to compute the valence in the flip stage
 */
void CoarseIsotropicRemeshing(uintptr_t _baseM, int iter, bool adapt, bool refine, bool swap, float crease, float lengthThr,
                              bool DEBUGLAPLA, bool DEBUGPROJ, bool DEBUGCREASE, bool DEBUGCOLLAPSE, bool DEBUGSPLIT)
{
    MyMesh &original = *((MyMesh*) _baseM), m;

    // Mesh cleaning
    int dup      = tri::Clean<MyMesh>::RemoveDuplicateVertex(original);
    int unref    = tri::Clean<MyMesh>::RemoveUnreferencedVertex(original);
    //int zeroArea = tri::Clean<MyMesh>::RemoveZeroAreaFace(original);
    Allocator<MyMesh>::CompactEveryVector(original);

    //Updating box before constructing the grid, otherwise we get weird results
    original.UpdateBoxAndNormals(); //per face normalized and per vertex

    //Build a uniform grid with the orignal mesh. Needed to apply the reprojection step.
    vcg::tri::Append<MyMesh,MyMesh>::MeshCopy(m,original);

    MyGrid t;
    t.Set(original.face.begin(), original.face.end());
    tri::FaceTmark<MyMesh> mark;
    mark.SetMesh(&original);

    tri::UpdateTopology<MyMesh>::FaceFace(m);
    tri::UpdateTopology<MyMesh>::VertexFace(m);
    tri::UpdateFlags<MyMesh>::VertexBorderFromFaceAdj(m);

    /* Manifold(ness) check*/
    if(tri::Clean<MyMesh>::CountNonManifoldEdgeFF(m) != 0 ||
            tri::Clean<MyMesh>::CountNonManifoldVertexFF(m) != 0)
    {
        printf("Input mesh is non-manifold, manifoldness is required!\nInterrupting filter");
        return;
    }

//    tri::UpdateCurvature<MyMesh>::MeanAndGaussian(m);
//    tri::UpdateQuality<MyMesh>::VertexFromAbsoluteCurvature(m);

        computeQuality(m);
        tri::UpdateQuality<MyMesh>::VertexSaturate(m);

    tri::UpdateTopology<MyMesh>::AllocateEdge(m);
    float length = tri::Stat<MyMesh>::ComputeEdgeLengthAverage(m);
    printf("Initial Mean edge size: %.6f\n", length);
    printf("Initial Mean Valence:   %.15f\n", computeMeanValence(m));

    Params params(adapt, length, lengthThr, crease);

    for(int i=0; i < iter; ++i)
    {
        printf("iter %d \n", i+1);
        if(refine)
        {
            if(DEBUGSPLIT)
                SplitLongEdges(m, params);
            if(DEBUGCOLLAPSE){
                CollapseShortEdges(m, params);
            }
        }
        if(swap)
            ImproveValence(m, params);
        CollapseCrosses(m, params);
        if(DEBUGLAPLA)
            ImproveByLaplacian(m, params, DEBUGCREASE);
        if(DEBUGPROJ)
            ProjectToSurface(m, t, mark);
    }

    tri::UpdateTopology<MyMesh>::AllocateEdge(m);
    printf("Final Mean edge size: %.6f\n", tri::Stat<MyMesh>::ComputeEdgeLengthAverage(m));
    printf("Final Mean Valence: %.15f \n", computeMeanValence(m));

    m.UpdateBoxAndNormals();
    vcg::tri::Append<MyMesh,MyMesh>::MeshCopy(original,m);
}
