#include "KDTree.h"

KDTree::KDTree(size_t numTriangles)
{
	triangleBoxes.resize(numTriangles);
}

void KDTree::AddTriangle(int index, float* v0, float* v1, float* v2)
{
	triangleBoxes[index] = BBox(v0, v1, v2);
}

void KDTree::Build()
{
	root = std::make_unique<KDNode>();
}