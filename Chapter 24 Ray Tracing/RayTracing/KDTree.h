#pragma once
#include <vector>
#include <memory>

typedef unsigned int uint;
struct BBox
{
	BBox() {}
	BBox(float* v0, float* v1, float* v2)
	{
		min[0] = v0[0] < v1[0] ? (v0[0] < v2[0] ? v0[0] : v2[0]) : (v1[0] < v2[0] ? v1[0] : v2[0]);
		min[1] = v0[1] < v1[1] ? (v0[1] < v2[1] ? v0[1] : v2[1]) : (v1[1] < v2[1] ? v1[1] : v2[1]);
		min[2] = v0[2] < v1[2] ? (v0[2] < v2[2] ? v0[2] : v2[2]) : (v1[2] < v2[2] ? v1[2] : v2[2]);
		max[0] = v0[0] > v1[0] ? (v0[0] > v2[0] ? v0[0] : v2[0]) : (v1[0] > v2[0] ? v1[0] : v2[0]);
		max[1] = v0[1] > v1[1] ? (v0[1] > v2[1] ? v0[1] : v2[1]) : (v1[1] > v2[1] ? v1[1] : v2[1]);
		max[2] = v0[2] > v1[2] ? (v0[2] > v2[2] ? v0[2] : v2[2]) : (v1[2] > v2[2] ? v1[2] : v2[2]);
	}
	void Join(const BBox& box)
	{
		for (int i = 0; i < 3; i++)
		{
			min[i] = std::fmin(min[i], box.min[i]);
			max[i] = std::fmax(max[i], box.max[i]);
		}
	}
	void GetCenter(float* center)
	{
		center[0] = (min[0] + max[0]) / 2;
		center[1] = (min[1] + max[1]) / 2;
		center[2] = (min[2] + max[2]) / 2;
	}
	bool Intersect(const BBox& box)
	{
		for (int i = 0; i < 3; i++)
		{
			if (min[i] > box.max[i] || max[i] < box.min[i])
				return false;
		}
		return true;
	}
	float min[3];
	float max[3];
};

struct KDNode
{
	KDNode() :tree(nullptr) {}
	KDNode(struct KDTree* t) :tree(t) {}
	void SplitForDimension(int d, std::vector<uint>& left, std::vector<uint>& right);
	void Split();
	std::unique_ptr<KDNode> left = nullptr;
	std::unique_ptr<KDNode> right = nullptr;
	BBox bbox;
	std::vector<uint> triangles;
	struct KDTree* tree;
};

struct KDNode_GPU
{
	BBox box;
	uint left;
	uint right;
	uint start;
	uint count;
};

struct KDTree
{
	KDTree(size_t numTriangles);
	void AddTriangle(int index, float* v0, float* v1, float* v2);
	void Build(std::vector<KDNode_GPU>& nodes, std::vector<uint>& indices);
	std::vector<BBox> triangleBoxes;
	std::unique_ptr<KDNode> root;
};