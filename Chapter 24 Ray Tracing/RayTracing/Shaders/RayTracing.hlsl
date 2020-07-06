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
Ray CreateRay(float3 origin, float3 direction)
{
	Ray ray;
	ray.origin = origin;
	ray.direction = direction;
	return ray;
}
Ray CreateCameraRay(float2 uv)
{
	// Transform the camera origin to world space
	float3 origin = mul(gView, float4(0.0f, 0.0f, 0.0f, 1.0f)).xyz;
	// Invert the perspective projection of the view-space position
	float3 direction = mul(gInvProj, float4(uv, 0.0f, 1.0f)).xyz;
	// Transform the direction from camera to world space and normalize
	direction = normalize(direction);
	return CreateRay(origin, direction);
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
//	gOutput[id.xy] = float4(ray.direction * 0.5f + 0.5f, 1.0f);
	gOutput[id.xy] = gCubeMap.SampleLevel(gsamLinearWrap, ray.direction, 0);
}
