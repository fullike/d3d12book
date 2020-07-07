TextureCube gCubeMap : register(t0);
RWTexture2D<float4> gOutput : register(u0);

SamplerState gsamPointWrap        : register(s0);
SamplerState gsamPointClamp       : register(s1);
SamplerState gsamLinearWrap       : register(s2);
SamplerState gsamLinearClamp      : register(s3);
SamplerState gsamAnisotropicWrap  : register(s4);
SamplerState gsamAnisotropicClamp : register(s5);

// Constant data that varies per material.
cbuffer cbPass : register(b0)
{
	float4x4 gView;
	float4x4 gInvView;
	float4x4 gProj;
	float4x4 gInvProj;
	float4x4 gViewProj;
	float4x4 gInvViewProj;
};

struct Ray
{
	float3 origin;
	float3 direction;
};
struct RayHit
{
	float3 position;
	float distance;
	float3 normal;
};
Ray CreateRay(float3 origin, float3 direction)
{
	Ray ray;
	ray.origin = origin;
	ray.direction = direction;
	return ray;
}
RayHit CreateRayHit()
{
	RayHit hit;
	hit.position = float3(0.0f, 0.0f, 0.0f);
	hit.distance = 1.#INF;
	hit.normal = float3(0.0f, 0.0f, 0.0f);
	return hit;
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
void IntersectSphere(Ray ray, inout RayHit bestHit, float4 sphere)
{
	// Calculate distance along the ray where the sphere is intersected
	float3 d = ray.origin - sphere.xyz;
	float p1 = -dot(ray.direction, d);
	float p2sqr = p1 * p1 - dot(d, d) + sphere.w * sphere.w;
	if (p2sqr < 0)
		return;
	float p2 = sqrt(p2sqr);
	float t = p1 - p2 > 0 ? p1 - p2 : p1 + p2;
	if (t > 0 && t < bestHit.distance)
	{
		bestHit.distance = t;
		bestHit.position = ray.origin + t * ray.direction;
		bestHit.normal = normalize(bestHit.position - sphere.xyz);
	}
}
RayHit Trace(Ray ray)
{
	RayHit bestHit = CreateRayHit();
	IntersectSphere(ray, bestHit, float4(0, 0.0f, 3, 1.0f));
	IntersectGroundPlane(ray, bestHit);
	return bestHit;
}
float3 Shade(inout Ray ray, RayHit hit)
{
	if (hit.distance < 1.#INF)
	{
		// Return the normal
		return hit.normal * 0.5f + 0.5f;
	}
	else
	{
		return gCubeMap.SampleLevel(gsamLinearWrap, ray.direction, 0).xyz;
	}
}

[numthreads(8, 8, 1)]
void CS(int3 groupThreadID : SV_GroupID, int3 id : SV_DispatchThreadID)
{
	// Get the dimensions of the RenderTexture
	uint width, height;
	gOutput.GetDimensions(width, height);
	// Transform pixel to [-1,1] range
	float2 uv = float2((id.xy + float2(0.5f, 0.5f)) / float2(width, height) * 2.0f - 1.0f);
	// Get a ray for the UVs
	Ray ray = CreateCameraRay(float2(uv.x,-uv.y));
	// Write some colors
	RayHit hit = Trace(ray);
	float3 result = Shade(ray, hit);
	gOutput[id.xy] = float4(result, 1);
}
