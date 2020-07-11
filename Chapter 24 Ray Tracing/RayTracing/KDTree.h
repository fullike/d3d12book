#pragma once
#include <vector>
#include <memory>
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
	KDTree(size_t numTriangles);
	void AddTriangle(int index, float* v0, float* v1, float* v2);
	void Build();
	std::vector<BBox> triangleBoxes;
	std::unique_ptr<KDNode> root;
};