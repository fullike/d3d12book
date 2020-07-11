#include "KDTree.h"

void KDNode::Split()
{
	if (triangles.size() <= 64)
		return;
	std::vector<uint> leftTris[3];
	std::vector<uint> rightTris[3];
	int MinSum = 0x7FFFFFFF;
	int Best = -1;
	for (int i = 0; i < 3; i++)
	{
		SplitForDimension(i, leftTris[i], rightTris[i]);
		int Sum = leftTris[i].size() + rightTris[i].size();
		if (MinSum > Sum)
		{
			MinSum = Sum;
			Best = i;
		}
	}
	if (leftTris[Best].size() < triangles.size() && rightTris[Best].size() < triangles.size())
	{
		float center[3];
		bbox.GetCenter(center);
		left = std::make_unique<KDNode>(tree);
		left->bbox = bbox;
		left->bbox.max[Best] = center[Best];
		left->triangles = leftTris[Best];
		left->Split();

		right = std::make_unique<KDNode>(tree);
		right->bbox = bbox;
		right->bbox.min[Best] = center[Best];
		right->triangles = rightTris[Best];
		right->Split();
	}
}

void KDNode::SplitForDimension(int d, std::vector<uint>& left, std::vector<uint>& right)
{
	float center[3];
	bbox.GetCenter(center);
	BBox leftBox = bbox;
	BBox rightBox = bbox;
	leftBox.max[d] = rightBox.min[d] = center[d];
	for (int i = 0; i < triangles.size(); i++)
	{
		BBox& box = tree->triangleBoxes[triangles[i]];
		if (leftBox.Intersect(box))
			left.push_back(triangles[i]);
		if (rightBox.Intersect(box))
			right.push_back(triangles[i]);
	}
}

KDTree::KDTree(size_t numTriangles)
{
	triangleBoxes.resize(numTriangles);
}

void KDTree::AddTriangle(int index, float* v0, float* v1, float* v2)
{
	triangleBoxes[index] = BBox(v0, v1, v2);
}

void KDTree::Build(std::vector<KDNode_GPU>& nodes, std::vector<uint>& indices)
{
	root = std::make_unique<KDNode>(this);
	root->triangles.push_back(0);
	root->bbox = triangleBoxes[0];
	for (int i = 1; i < triangleBoxes.size(); i++)
	{
		root->triangles.push_back(i);
		root->bbox.Join(triangleBoxes[i]);
	}
	root->Split();
	std::vector<KDNode*> tmpNodes;
	tmpNodes.push_back(root.get());
	for (int i = 0; i < tmpNodes.size(); i++)
	{
		KDNode_GPU gpu;
		KDNode* node = tmpNodes[i];
		gpu.box = node->bbox;
		if (node->left.get() || node->right.get())
		{
			BBox leftBox = node->left->bbox;
			BBox rightBox = node->right->bbox;


			gpu.start = 0;
			gpu.count = 0;
			gpu.left = tmpNodes.size();
			tmpNodes.push_back(node->left.get());
			gpu.right = tmpNodes.size();
			tmpNodes.push_back(node->right.get());
		}
		else
		{
			gpu.start = indices.size();
			gpu.count = node->triangles.size();
			gpu.left = 0;
			gpu.right = 0;
			for (int j = 0; j < node->triangles.size(); j++)
				indices.push_back(node->triangles[j]);
		}
		nodes.push_back(gpu);
	}
}