#ifndef RAYTRACING_RESOURCES_CPP
#define RAYTRACING_RESOURCES_CPP
#include <glm/glm.hpp>
#include <iostream>
#include <vector>
#include <string>
#include <fstream>
#include <sstream>
#include <filesystem>
#include <algorithm>
#include <iomanip>
#include <cmath>

using namespace glm;

const float INV_3 = 1.0f / 3.0f;

class Triangle {
public:
    vec3 v0;
    vec3 v1;
    vec3 v2;
    
    Triangle() {
        v0 = vec3(0.0f);
        v1 = vec3(0.0f);
        v2 = vec3(0.0f);
    }

    Triangle(const vec3& iv0, const vec3& iv1, const vec3& iv2) {
        v0 = iv0;
        v1 = iv1;
        v2 = iv2;
    }

    std::vector<float> center(){
        return {
            (v0.x + v1.x + v2.x) * INV_3,
            (v0.y + v1.y + v2.y) * INV_3,
            (v0.z + v1.z + v2.z) * INV_3
        };
    };

    vec3 minCoord() const {
        return min(min(v0, v1), v2);
    }

    vec3 maxCoord() const {
        return max(max(v0, v1), v2);
    }

    friend std::ostream& operator<<(std::ostream& os, const Triangle& tri) {
        os << "Triangle("
           << "v0: (" << tri.v0.x << ", " << tri.v0.y << ", " << tri.v0.z << "), "
           << "v1: (" << tri.v1.x << ", " << tri.v1.y << ", " << tri.v1.z << "), "
           << "v2: (" << tri.v2.x << ", " << tri.v2.y << ", " << tri.v2.z << ")"
           << ")";
        return os;
    }
};

class BoundingBox {
public:
    int index;
    int count;
    vec4 minCorner;
    vec4 maxCorner;
    int childInd;
    int padding;
    BoundingBox() {
        index = 0;
        count = 0;
        minCorner = vec4(vec3(100000.0f), 0.0f);
        maxCorner = vec4(vec3(-100000.0f), 0.0f);
        childInd = 0;
        padding = 0;
    }

    BoundingBox(const int& ind) {
        index = ind;
        count = 0;
        minCorner = vec4(vec3(100000.0f), 0.0f);
        maxCorner = vec4(vec3(-100000.0f), 0.0f);
        childInd = 0;
        padding = 0;
    }

    BoundingBox(const int& ind, const int& cnt) {
        index = ind;
        count = cnt;
        minCorner = vec4(vec3(100000.0f), 0.0f);
        maxCorner = vec4(vec3(-100000.0f), 0.0f);
        childInd = 0;
        padding = 0;
    }

    BoundingBox(const vec3& minC, const vec3& maxC) {
        index = 0;
        count = 0;
        minCorner = vec4(minC, 0.0f);
        maxCorner = vec4(maxC, 0.0f);
        childInd = 0;
        padding = 0;
    }

    BoundingBox(const int& ind, const int& cnt, const vec3& minC, const vec3& maxC) {
        index = ind;
        count = cnt;
        minCorner = vec4(minC, 0.0f);
        maxCorner = vec4(maxC, 0.0f);
        childInd = 0;
        padding = 0;
    }

    BoundingBox(const int& ind, const int& cnt, const vec3& minC, const vec3& maxC, const int& childI) {
        index = ind;
        count = cnt;
        minCorner = vec4(minC, 0.0f);
        maxCorner = vec4(maxC, 0.0f);
        childInd = childI;
        padding = 0;
    }

    BoundingBox setChildInd(const int& ind) {
        return BoundingBox(index, count, minCorner, maxCorner, ind);
    }

    void grow(const Triangle& tri) {
        minCorner = vec4(min(vec3(minCorner.x, minCorner.y, minCorner.z), tri.minCoord()), 0.0f);
        maxCorner = vec4(max(vec3(maxCorner.x, maxCorner.y, maxCorner.z), tri.maxCoord()), 0.0f);
    }

    std::vector<float> halfPoint() const {
        return {
            (minCorner.x + maxCorner.x) * 0.5f,
            (minCorner.y + maxCorner.y) * 0.5f,
            (minCorner.z + maxCorner.z) * 0.5f
        };
    }

    int axisIndex() const {
        vec3 distance = maxCorner - minCorner;
        return distance.x > max(distance.y, distance.z) ? 0 : (distance.y > distance.z ? 1 : 2);
    }
};

float lerp(const float& a, const float& b, const float& t) {
    return a + t * (b - a);
}

std::vector<Triangle> ParseObj(std::string name, vec3 scale, vec3 translate) {
    std::vector<Triangle> mesh;

    if (!std::filesystem::exists(name)) {
        std::cerr << "ParseObj: file does not exist: " << name << std::endl;
        std::cerr << "Working directory: " << std::filesystem::current_path() << std::endl;
        return {};
    }

    std::ifstream file(name);
    if (!file.is_open()) {
        std::cerr << "ParseObj: failed to open: " << name << std::endl;
        std::cerr << "Working directory: " << std::filesystem::current_path() << std::endl;
        return {};
    }

    std::vector<vec3> vertices;
    std::string line;
    while (std::getline(file, line)) {
        if (line.empty()) continue;

        std::stringstream s(line);
        std::string token;
        s >> token;

        // Explicitly ignore vertex normals and other non-vertex/face records.
        // Common tokens to ignore: "vn" (vertex normal), "vt" (texcoord), "vp" (parameter space),
        // comments '#' and material/group/object directives.
        if (token == "#" ||
            token == "vn" ||
            token == "vt" ||
            token == "vp" ||
            token == "o" ||
            token == "g" ||
            token == "s" ||
            token == "usemtl" ||
            token == "mtllib") {
            continue;
        }
        else if (token == "v") {
            float x, y, z;
            if (!(s >> x >> y >> z)) {
                // malformed vertex line
                continue;
            }
            vertices.emplace_back(x, y, z);
        }
        else if (token == "f") {
            std::string a, b, c;
            if (!(s >> a >> b >> c)) {
                // not a triangle face or malformed
                continue;
            }
            auto parseIndex = [](const std::string& vtoken) -> int {
                // split on '/'
                size_t slash = vtoken.find('/');
                std::string idx = (slash == std::string::npos) ? vtoken : vtoken.substr(0, slash);
                try {
                    return std::stoi(idx);
                }
                catch (...) {
                    return 0;
                }
                };
            int ia = parseIndex(a);
            int ib = parseIndex(b);
            int ic = parseIndex(c);
            if (ia <= 0 || ib <= 0 || ic <= 0) continue; // invalid indices

            // obj indices are 1-based
            if ((size_t)ia > vertices.size() || (size_t)ib > vertices.size() || (size_t)ic > vertices.size()) {
                // malformed index
                continue;
            }

            mesh.emplace_back(
                vertices[ia - 1] * scale + translate,
                vertices[ib - 1] * scale + translate,
                vertices[ic - 1] * scale + translate
            );
        }
    }

    file.close();

    return mesh;
}

float BBCost(const BoundingBox& BB, const int numTris) {
    vec3 size = vec3(BB.maxCorner - BB.minCorner);
    float halfSurfaceArea = size.x * (size.y + size.z) + size.y * size.z;
    return halfSurfaceArea * static_cast<float>(numTris);
}

float axisCost(const BoundingBox& BB, const int& currAxis, const float& pos, std::vector<Triangle> mesh) {
    BoundingBox BBA = BoundingBox();
    BoundingBox BBB = BoundingBox();

    int trisA = 0, trisB = 0;

    for (int i = BB.index; i < BB.index + BB.count; i++) {
        std::vector<float> centerCoord = mesh[i].center();

        if (centerCoord[currAxis] < pos) {
            BBA.grow(mesh[i]);
            trisA++;
        } else {
            BBB.grow(mesh[i]);
            trisB++;
        }
    }

    return BBCost(BBA, trisA) + BBCost(BBB, trisB);
}

int chooseSplitAxis(const BoundingBox& BB, const std::vector<Triangle>& mesh, float& pos, float& cost) {
    int tests = 5;
    float bestCost = INFINITY;
    float bestPos = 0.0f;
    int bestAxis = 0;

    for (int axis = 0; axis < 3; axis++) {
        float minPos = 0.0f;
        if (axis == 0) minPos = BB.minCorner.x;
        if (axis == 1) minPos = BB.minCorner.y;
        if (axis == 2) minPos = BB.minCorner.z;

        float maxPos = 0.0f;
        if (axis == 0) maxPos = BB.maxCorner.x;
        if (axis == 1) maxPos = BB.maxCorner.y;
        if (axis == 2) maxPos = BB.maxCorner.z;

        for (int i = 0; i < tests; i++) {
            float splitPos = (static_cast<float>(i)) / (static_cast<float>(tests));

            float sampleSplitPos = minPos + splitPos * (maxPos - minPos);
            
            float cost = axisCost(BB, axis, sampleSplitPos, mesh);
            if (cost < bestCost) {
                bestCost = cost;
                bestPos = sampleSplitPos;
                bestAxis = axis;
            }
        }
    }

    pos = bestPos;
    cost = bestCost;
    return bestAxis;
}

void BuildBVH(int boxIndex, std::vector<BoundingBox>& boxes, std::vector<Triangle>& mesh, int depth = 0) {
    BoundingBox* currBB = &boxes[boxIndex];
    if (depth > 32) {
        return;
    }

    float splitCost;
    float splitPos;
    int splitAxis = chooseSplitAxis(*currBB, mesh, splitPos, splitCost);
    
    if (splitCost >= BBCost(*currBB, currBB->count)) {
        return;
    }

    BoundingBox* BBA = new BoundingBox(currBB->index);
    BoundingBox* BBB = new BoundingBox(currBB->index);

    for (int i = currBB->index; i < currBB->index + currBB->count; i++) {
        Triangle tri = mesh[i];
        
        std::vector<float> centerCoord = tri.center();

        bool isSideA = centerCoord[splitAxis] < splitPos;
        BoundingBox* nextBB = isSideA ? BBA : BBB;
        nextBB->grow(tri);
        nextBB->count++;

        if (isSideA) {
            int swapInd = nextBB->index + nextBB->count - 1;
            std::swap(mesh[i], mesh[swapInd]);
            BBB->index++;
        }
    }

    if (BBA->count > 0 && BBB->count > 0) {
        int childIndex = boxes.size();
        boxes[boxIndex].childInd = childIndex;
        
        boxes.push_back(*BBA);
        boxes.push_back(*BBB);

        BuildBVH(childIndex, boxes, mesh, depth + 1);
        BuildBVH(childIndex + 1, boxes, mesh, depth + 1);
    }
}

void writeBVHToFile(const std::string& nameBVH, const std::string& nameMesh, std::vector<Triangle>& mesh) {
    if (!std::filesystem::exists(nameBVH)) {
        std::cerr << "writeBVHToFile: BVH file does not exist: " << nameBVH << std::endl;
        std::cerr << "Working directory: " << std::filesystem::current_path() << std::endl;
        return;
    }

    std::ifstream BVHReadFile(nameBVH);
    if (!BVHReadFile.is_open()) {
        std::cerr << "writeBVHToFile: failed to open: " << nameBVH << std::endl;
        std::cerr << "Working directory: " << std::filesystem::current_path() << std::endl;
        return;
    }

    std::vector<BoundingBox> boxes;
    boxes.push_back(BoundingBox(0, static_cast<int>(mesh.size())));
    for (const Triangle& tri : mesh) {
        boxes[0].grow(tri);
    }

    BuildBVH(0, boxes, mesh);

    std::ofstream BVHFile(nameBVH);
    for (BoundingBox& box : boxes) {
        BVHFile << box.index << ", " << box.count << ", " << box.minCorner.x << ", " << box.minCorner.y << ", " << box.minCorner.z << ", ";
        BVHFile << box.maxCorner.x << ", " << box.maxCorner.y << ", " << box.maxCorner.z << ", ";
        BVHFile << box.childInd << ", " << box.padding << std::endl;
    }
    BVHFile.close();

    if (!std::filesystem::exists(nameMesh)) {
        std::cerr << "writeBVHToFile: BVH file does not exist: " << nameMesh << std::endl;
        std::cerr << "Working directory: " << std::filesystem::current_path() << std::endl;
        return;
    }

    std::ifstream meshReadFile(nameMesh);
    if (!meshReadFile.is_open()) {
        std::cerr << "writeBVHToFile: failed to open: " << nameMesh << std::endl;
        std::cerr << "Working directory: " << std::filesystem::current_path() << std::endl;
        return;
    }

    std::ofstream meshFile(nameMesh);
    for (Triangle& tri : mesh) {
        meshFile << tri.v0.x << ", " << tri.v0.y << ", " << tri.v0.z << ", ";
        meshFile << tri.v1.x << ", " << tri.v1.y << ", " << tri.v1.z << ", ";
        meshFile << tri.v2.x << ", " << tri.v2.y << ", " << tri.v2.z << std::endl;
    }
    meshFile.close();
}

void readBVHFromFile(const std::string& nameBVH, const std::string& nameMesh, std::vector<BoundingBox>& boxes, std::vector<Triangle>& mesh) {
    boxes.clear();
    if (!std::filesystem::exists(nameBVH)) {
        std::cerr << "readBVHFromFile: BVH file does not exist: " << nameBVH << std::endl;
        std::cerr << "Working directory: " << std::filesystem::current_path() << std::endl;
        return;
    }
    std::ifstream BVHFile(nameBVH);
    if (!BVHFile.is_open()) {
        std::cerr << "readBVHFromFile: BVH failed to open: " << nameBVH << std::endl;
        std::cerr << "Working directory: " << std::filesystem::current_path() << std::endl;
        return;
    }

    std::string lineBVH;
    while (std::getline(BVHFile, lineBVH)) {
        std::stringstream sBVH(lineBVH);
        int index, count, childInd, padding;
        char discard;
        float minX, minY, minZ, maxX, maxY, maxZ;
        sBVH >> index >> discard >> count >> discard;
        sBVH >> minX >> discard >> minY >> discard >> minZ >> discard;
        sBVH >> maxX >> discard >> maxY >> discard >> maxZ >> discard;
        sBVH >> childInd >> discard >> padding;
        BoundingBox box(index, count, vec3(minX, minY, minZ), vec3(maxX, maxY, maxZ), childInd);
        box.padding = padding;
        boxes.emplace_back(box);
    }
    BVHFile.close();

    mesh.clear();
    if (!std::filesystem::exists(nameMesh)) {
        std::cerr << "readBVHFromFile: mesh file does not exist: " << nameMesh << std::endl;
        std::cerr << "Working directory: " << std::filesystem::current_path() << std::endl;
        return;
    }
    std::ifstream meshFile(nameMesh);
    if (!meshFile.is_open()) {
        std::cerr << "readBVHFromFile: mesh failed to open: " << nameMesh << std::endl;
        std::cerr << "Working directory: " << std::filesystem::current_path() << std::endl;
        return;
    }

    std::string lineMesh;
    while (std::getline(meshFile, lineMesh)) {
        std::stringstream sMesh(lineMesh);
        vec3 v0, v1, v2;
        char discard;
        sMesh >> v0.x >> discard >> v0.y >> discard >> v0.z;
        sMesh >> v1.x >> discard >> v1.y >> discard >> v1.z;
        sMesh >> v2.x >> discard >> v2.y >> discard >> v2.z;
        Triangle tri(v0, v1, v2);
        mesh.emplace_back(tri);
    }
    meshFile.close();
}

#endif