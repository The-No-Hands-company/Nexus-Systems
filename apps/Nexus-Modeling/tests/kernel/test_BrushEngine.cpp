#include <gtest/gtest.h>
#include "nexus/brush/BrushEngine.h"
#include "nexus/geometry/Mesh.h"
using namespace nexus::brush;
using namespace nexus;
TEST(BrushEngine,ApplyStroke){BrushEngine be;auto m=geometry::primitives::makePlane(10,10,4,4);StrokePoint sp;sp.position={0,0,0};sp.normal={0,1,0};auto r=be.applyStroke(m,{sp});EXPECT_GT(r.topology().faceCount(),0u);}
TEST(BrushEngine,DeformPoints){BrushEngine be;std::vector<Vec3> pts={{0,0,0},{1,0,0}};std::vector<Vec3> nrm={{0,1,0},{0,1,0}};StrokePoint sp;sp.position={0,0,0};sp.normal={0,1,0};auto r=be.deformPoints(pts,nrm,sp);EXPECT_NE(r[0].y,pts[0].y);}
