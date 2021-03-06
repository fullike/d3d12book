TextureCube gCubeMap : register(t0);
Texture2D gTexture2D : register(t0);
RWTexture2D<float4> gOutput : register(u0);

SamplerState gsamPointWrap        : register(s0);
SamplerState gsamPointClamp       : register(s1);
SamplerState gsamLinearWrap       : register(s2);
SamplerState gsamLinearClamp      : register(s3);
SamplerState gsamAnisotropicWrap  : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);

// Constant data that varies per material.
cbuffer rtPass : register(b0)
{
	float4x4 gView;
	float4x4 gInvView;
	float4x4 gProj;
	float4x4 gInvProj;
	float4x4 gViewProj;
	float4x4 gInvViewProj;
	float4 gDirectionalLight;
	float _Seed;
	int NumSpheres;
	int NumTriangles;
};
cbuffer accPass : register(b0)
{
	int NumSamples;
};

struct Ray
{
	float3 origin;
	float3 direction;
	float3 energy;
};
struct RayHit
{
	float3 position;
	float distance;
	float3 normal;
	float3 albedo;
	float3 specular;
};
struct Sphere
{
	float3 position;
	float radius;
	float3 albedo;
	float3 specular;
};
struct Vertex
{
	float3 position;
	float3 normal;
	float2 uv;
};
struct Triangle
{
	uint indices[3];
	uint material;
};
struct KDNode
{
	float3 min;
	float3 max;
	uint left;
	uint right;
	uint start;
	uint count;
};
StructuredBuffer<Sphere> gSpheres : register(t1);
StructuredBuffer<Vertex> gVertices : register(t2);
StructuredBuffer<Triangle> gTriangles : register(t3);
StructuredBuffer<KDNode> gNodes : register(t4);
StructuredBuffer<uint> gIndices : register(t5);

static float2 _Pixel;
static const float PI = 3.14159265358979;
float rand()
{
	float result = frac(sin(_Seed / 100.0f * dot(_Pixel, float2(12.9898f, 78.233f))) * 43758.5453f);
	_Seed += 1.0f;
	return result;
}
float3x3 GetTangentSpace(float3 normal)
{
	// Choose a helper vector for the cross product
	float3 helper = float3(1, 0, 0);
	if (abs(normal.x) > 0.99f)
		helper = float3(0, 0, 1);
	// Generate vectors
	float3 tangent = normalize(cross(normal, helper));
	float3 binormal = normalize(cross(normal, tangent));
	return float3x3(tangent, binormal, normal);
}
float3 SampleHemisphere(float3 normal)
{
	// Uniformly sample hemisphere direction
	float cosTheta = rand();
	float sinTheta = sqrt(max(0.0f, 1.0f - cosTheta * cosTheta));
	float phi = 2 * PI * rand();
	float3 tangentSpaceDir = float3(cos(phi) * sinTheta, sin(phi) * sinTheta, cosTheta);
	// Transform direction to world space
	return mul(tangentSpaceDir, GetTangentSpace(normal));
}
float sdot(float3 x, float3 y, float f = 1.0f)
{
	return saturate(dot(x, y) * f);
}

Ray CreateRay(float3 origin, float3 direction)
{
	Ray ray;
	ray.origin = origin;
	ray.direction = direction;
	ray.energy = float3(1, 1, 1);
	return ray;
}
RayHit CreateRayHit()
{
	RayHit hit;
	hit.position = float3(0.0f, 0.0f, 0.0f);
	hit.distance = 1.#INF;
	hit.normal = float3(0.0f, 0.0f, 0.0f);
	hit.specular = float3(0.6f, 0.6f, 0.6f);
	hit.albedo = float3(0.8f, 0.8f, 0.8f);
	return hit;
}
Sphere CreateSphere(float3 position, float radius)
{
	Sphere sphere;
	sphere.position = position;
	sphere.radius = radius;
	sphere.specular = float3(0.6f, 0.6f, 0.6f);
	sphere.albedo = float3(0.8f, 0.8f, 0.8f);
	return sphere;
}
Ray CreateCameraRay(float2 uv)
{
	// Transform the camera origin to world space
	float3 origin = mul(gInvView, float4(0.0f, 0.0f, 0.0f, 1.0f)).xyz;
	// Invert the perspective projection of the view-space position
	float3 direction = mul(gInvProj, float4(uv, 0.0f, 1.0f)).xyz;
	// Transform the direction from camera to world space and normalize
	direction = mul(gInvView, float4(direction, 0.0f)).xyz;

	direction = normalize(direction);
	return CreateRay(origin, direction);
}
void swap(inout float a, inout float b)
{
	float c = a;
	a = b;
	b = c;
}
bool IntersectBox(float3 min, float3 max, Ray r)
{
	float tmin = (min.x - r.origin.x) / r.direction.x;
	float tmax = (max.x - r.origin.x) / r.direction.x;
	if (tmin > tmax) swap(tmin, tmax);
	float tymin = (min.y - r.origin.y) / r.direction.y;
	float tymax = (max.y - r.origin.y) / r.direction.y;
	if (tymin > tymax) swap(tymin, tymax);
	if ((tmin > tymax) || (tymin > tmax))
		return false;
	if (tymin > tmin)
		tmin = tymin;
	if (tymax < tmax)
		tmax = tymax;
	float tzmin = (min.z - r.origin.z) / r.direction.z;
	float tzmax = (max.z - r.origin.z) / r.direction.z;
	if (tzmin > tzmax) swap(tzmin, tzmax);
	if ((tmin > tzmax) || (tzmin > tmax))
		return false;
	if (tzmin > tmin)
		tmin = tzmin;
	if (tzmax < tmax)
		tmax = tzmax;
	return true;
}
void IntersectGroundPlane(Ray ray, inout RayHit bestHit)
{
	// Calculate distance along the ray where the ground plane is intersected
	float t = -ray.origin.y / ray.direction.y;
	if (t > 0 && t < bestHit.distance)
	{
		bestHit.distance = t;
		bestHit.position = ray.origin + t * ray.direction;
		bestHit.normal = float3(0.0f, 1.0f, 0.0f);
	}
}
void IntersectSphere(Ray ray, inout RayHit bestHit, Sphere sphere)
{
	// Calculate distance along the ray where the sphere is intersected
	float3 d = ray.origin - sphere.position;
	float p1 = -dot(ray.direction, d);
	float p2sqr = p1 * p1 - dot(d, d) + sphere.radius * sphere.radius;
	if (p2sqr < 0)
		return;
	float p2 = sqrt(p2sqr);
	float t = p1 - p2 > 0 ? p1 - p2 : p1 + p2;
	if (t > 0 && t < bestHit.distance)
	{
		bestHit.distance = t;
		bestHit.position = ray.origin + t * ray.direction;
		bestHit.normal = normalize(bestHit.position - sphere.position);
		bestHit.albedo = sphere.albedo;
		bestHit.specular = sphere.specular;
	}
}
static const float EPSILON = 1e-8;
bool IntersectTriangle_MT97(Ray ray, float3 vert0, float3 vert1, float3 vert2, inout float t, inout float u, inout float v)
{
	// find vectors for two edges sharing vert0
	float3 edge1 = vert1 - vert0;
	float3 edge2 = vert2 - vert0;
	// begin calculating determinant - also used to calculate U parameter
	float3 pvec = cross(ray.direction, edge2);
	// if determinant is near zero, ray lies in plane of triangle
	float det = dot(edge1, pvec);
	// use backface culling
	if (det < EPSILON)
		return false;
	float inv_det = 1.0f / det;
	// calculate distance from vert0 to ray origin
	float3 tvec = ray.origin - vert0;
	// calculate U parameter and test bounds
	u = dot(tvec, pvec) * inv_det;
	if (u < 0.0 || u > 1.0f)
		return false;
	// prepare to test V parameter
	float3 qvec = cross(tvec, edge1);
	// calculate V parameter and test bounds
	v = dot(ray.direction, qvec) * inv_det;
	if (v < 0.0 || u + v > 1.0f)
		return false;
	// calculate t, ray intersects triangle
	t = dot(edge2, qvec) * inv_det;
	return true;
}
/*
RayHit Trace(Ray ray)
{
	RayHit bestHit = CreateRayHit();
	for (int i = 0; i < NumTriangles; i++)
	{
		Triangle tri = gTriangles[i];
		Vertex v0 = gVertices[tri.indices[0]];
		Vertex v1 = gVertices[tri.indices[1]];
		Vertex v2 = gVertices[tri.indices[2]];
		float t, u, v;
		if (IntersectTriangle_MT97(ray, v0.position, v1.position, v2.position, t, u, v) && t > 0 && t < bestHit.distance)
		{
			float w = 1 - u - v;
			bestHit.distance = t;
			bestHit.position = ray.origin + t * ray.direction;
			bestHit.normal = normalize(v0.normal * u + v1.normal * v + v2.normal * w);
		//	bestHit.albedo = sphere.albedo;
		//	bestHit.specular = sphere.specular;
		}
	}
	//	IntersectSphere(ray, bestHit, gSpheres[i]);
	//IntersectGroundPlane(ray, bestHit);
	return bestHit;
}*/
RayHit Trace(Ray ray)
{
	RayHit bestHit = CreateRayHit();
#if KDTREE_TESTING
	uint NumNodes = 0;
	uint Nodes[256];
	Nodes[NumNodes++] = 0;
	for (uint i = 0; i < NumNodes; i++)
	{
		KDNode node = gNodes[Nodes[i]];
		if (node.count > 0)
		{
			for (uint j = 0; j < node.count; j++)
			{
				Triangle tri = gTriangles[gIndices[node.start + j]];
				Vertex v0 = gVertices[tri.indices[0]];
				Vertex v1 = gVertices[tri.indices[1]];
				Vertex v2 = gVertices[tri.indices[2]];
				float t, u, v;
				if (IntersectTriangle_MT97(ray, v0.position, v1.position, v2.position, t, u, v) && t > 0 && t < bestHit.distance)
				{
					float w = 1 - u - v;
					bestHit.distance = t;
					bestHit.position = ray.origin + t * ray.direction;
					bestHit.normal = normalize(v0.normal * u + v1.normal * v + v2.normal * w);
				//	bestHit.albedo = sphere.albedo;
				//	bestHit.specular = sphere.specular;
				}
			}
		}
		else
		{
			KDNode left = gNodes[node.left];
			if (IntersectBox(left.min, left.max, ray))
				Nodes[NumNodes++] = node.left;
			KDNode right = gNodes[node.right];
			if (IntersectBox(right.min, right.max, ray))
				Nodes[NumNodes++] = node.right;
		}
	}
#else
	for (int i = 0; i < NumSpheres; i++)
		IntersectSphere(ray, bestHit, gSpheres[i]);
	IntersectGroundPlane(ray, bestHit);
#endif
	return bestHit;
}

float3 Shade(inout Ray ray, RayHit hit)
{
	if (hit.distance < 1.#INF)
	{
		// Diffuse shading
		ray.origin = hit.position + hit.normal * 0.001f;
		ray.direction = SampleHemisphere(hit.normal);
	//	ray.direction = reflect(ray.direction, hit.normal);
		ray.energy *= 2 * hit.albedo * sdot(hit.normal, ray.direction);
		return 0.0f;
	}
	else
	{
		// Erase the ray's energy - the sky doesn't reflect anything
		ray.energy = 0.0f;
		return gCubeMap.SampleLevel(gsamLinearWrap, ray.direction, 0).xyz;
	}
}



[numthreads(8, 8, 1)]
void CS(int3 groupThreadID : SV_GroupID, int3 id : SV_DispatchThreadID)
{
	_Pixel = id.xy;
	// Get the dimensions of the RenderTexture
	uint width, height;
	gOutput.GetDimensions(width, height);
	// Transform pixel to [-1,1] range
	float2 uv = float2((id.xy + float2(rand(), rand())) / float2(width, height) * 2.0f - 1.0f);
	// Get a ray for the UVs
	Ray ray = CreateCameraRay(float2(uv.x,-uv.y));
	// Write some colors
	float3 result = float3(0, 0, 0);
	for (int i = 0; i < 8; i++)
	{
		RayHit hit = Trace(ray);
		result += ray.energy * Shade(ray, hit);
		if (!any(ray.energy))
			break;
	}
	gOutput[id.xy] = float4(result, 1);
}


static const float2 gTexCoords[6] =
{
	float2(0.0f, 1.0f),
	float2(0.0f, 0.0f),
	float2(1.0f, 0.0f),
	float2(0.0f, 1.0f),
	float2(1.0f, 0.0f),
	float2(1.0f, 1.0f)
};

struct VertexOut
{
	float4 PosH    : SV_POSITION;
	float2 TexC    : TEXCOORD;
};

VertexOut VS(uint vid : SV_VertexID)
{
	VertexOut vout;

	vout.TexC = gTexCoords[vid];

	// Map [0,1]^2 to NDC space.
	vout.PosH = float4(2.0f*vout.TexC.x - 1.0f, 1.0f - 2.0f*vout.TexC.y, 0.0f, 1.0f);

	return vout;
}

float4 PS(VertexOut pin) : SV_Target
{
	float4 c = gTexture2D.SampleLevel(gsamPointWrap, pin.TexC, 0.0f);
	return float4(c.rgb, 1.0 / NumSamples);
}