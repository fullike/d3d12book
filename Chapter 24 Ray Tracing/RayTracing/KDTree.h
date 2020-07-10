#pragma once
#include <vector>
#include <memory>
struct BBox
{
	float min[3];
	float max[3];
};

struct KDNode
{
	std::unique_ptr<KDNode> left;
	std::unique_ptr<KDNode> right;
	BBox bbox;
	std::vector<unsigned int> triangles;
};

struct KDTree
{
	std::vector<BBox> triangleBoxes;
	std::unique_ptr<KDNode> root;
};