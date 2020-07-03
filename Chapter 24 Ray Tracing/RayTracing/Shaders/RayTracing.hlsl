
RWTexture2D<float4> gOutput : register(u0);

[numthreads(8, 8, 1)]
void CS(int3 groupThreadID : SV_GroupThreadID, int3 dispatchThreadID : SV_DispatchThreadID)
{
	float4 Colors[8] = 
	{
		float4(0,0,0,1),
		float4(0,0,1,1),
		float4(0,1,0,1),
		float4(0,1,1,1),
		float4(1,0,0,1),
		float4(1,0,1,1),
		float4(1,1,0,1),
		float4(1,1,1,1),
	};
	gOutput[dispatchThreadID.xy] = Colors[groupThreadID.x + groupThreadID.y];
}
