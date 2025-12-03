The code in RaytracingResources.h is referenced in main to populate a global bvh std::vector. It's pretty simply implemented so I didn't want to include the entire main file since it has a lot of Vulkan boilerplate as well. I just copy/pasted the BVH related stuff here:

```
std::vector<BoundingBox> BVH = { BoundingBox(0, mesh.size()) };

int main() {
    // Add the whole mesh to the first bounding box
    for (int i = BVH[0].index; i < BVH[0].index + BVH[0].count; i++) {
        BVH[0].grow(mesh[i]);
    }

    // Run BuildBVH
    BuildBVH(0, BVH, mesh);

    // Vulkan calls...

    return 0;
}
```
