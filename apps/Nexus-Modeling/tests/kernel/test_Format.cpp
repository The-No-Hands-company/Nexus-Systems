#include <gtest/gtest.h>
#include "nexus/format/FormatIO.h"
using namespace nexus::format;
TEST(FormatIO,ObjImport){std::string obj="v 0 0 0\nv 1 0 0\nv 0 1 0\nf 1 2 3\n";auto data=ObjIO::importFromText(obj);EXPECT_EQ(data.positions.size(),3u);EXPECT_EQ(data.faces.size(),1u);}
TEST(FormatIO,ObjExport){ObjData d;d.positions.push_back({0,0,0});d.positions.push_back({1,0,0});d.positions.push_back({0,1,0});ObjFace f;f.v={0,1,2};d.faces.push_back(f);auto text=ObjIO::exportToText(d);EXPECT_TRUE(text.find("v 0")!=std::string::npos);}
TEST(FormatIO,ObjToMesh){ObjData d;d.positions.push_back({0,0,0});d.positions.push_back({1,0,0});d.positions.push_back({0,1,0});ObjFace f;f.v={0,1,2};d.faces.push_back(f);auto mesh=ObjIO::toMesh(d);EXPECT_GT(mesh.topology().faceCount(),0u);}
TEST(FormatIO,GltfExport){GltfScene s;GltfNode n;n.name="Test";s.nodes.push_back(n);GltfMesh m;m.name="M1";s.meshes.push_back(m);auto json=GltfIO::exportToJson(s);EXPECT_TRUE(json.find("\"Test\"")!=std::string::npos);}
TEST(FormatIO,GltfImport){auto s=GltfIO::importFromJson("{}");EXPECT_GE(s.nodes.size(),1u);}
