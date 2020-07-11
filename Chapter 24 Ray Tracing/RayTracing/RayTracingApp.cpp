#include "../../Common/d3dApp.h"
#include "../../Common/Camera.h"
#include "../../Common/UploadBuffer.h"
#include "KDTree.h"
using Microsoft::WRL::ComPtr;
using namespace DirectX;

struct PassConstants
{
	XMMATRIX View;
	XMMATRIX InvView;
	XMMATRIX Proj;
	XMMATRIX InvProj;
	XMMATRIX ViewProj;
	XMMATRIX InvViewProj;
	XMFLOAT4 DirectionalLight;
	int NumSpheres;
	int NumTriangles;
};
struct Sphere
{
	XMFLOAT3 position;
	float radius;
	XMFLOAT3 albedo;
	XMFLOAT3 specular;
};
struct Vertex
{
	XMFLOAT3 position;
	XMFLOAT3 normal;
	XMFLOAT2 uv;
};
struct Triangle
{
	UINT indices[3];
	UINT material;
};
class RayTracingApp : public D3DApp
{
public:
	RayTracingApp(HINSTANCE hInstance);
	virtual bool Initialize()override;
private:
	virtual void OnResize()override;
	virtual void Update(const GameTimer& gt)override;
	virtual void Draw(const GameTimer& gt)override;
	virtual void OnMouseDown(WPARAM btnState, int x, int y)override;
	virtual void OnMouseUp(WPARAM btnState, int x, int y)override;
	virtual void OnMouseMove(WPARAM btnState, int x, int y)override;
	void OnKeyboardInput(const GameTimer& gt);
	void LoadTextures();
	void LoadModel(const char* file, std::vector<Vertex>& Vertices, std::vector<Triangle>& Triangles);
	void BuildBuffers();
	void BuildRootSignature();
	void BuildDescriptorHeaps();
	void BuildConstantBuffers();
	void BuildShadersAndInputLayout();
	void BuildPSOs();
	std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> GetStaticSamplers();
	std::unordered_map<std::string, std::unique_ptr<Texture>> mTextures;
	std::unordered_map<std::string, ComPtr<ID3DBlob>> mShaders;
	std::unordered_map<std::string, ComPtr<ID3D12PipelineState>> mPSOs;
	std::unique_ptr<UploadBuffer<PassConstants>> mPassCB = nullptr;
	std::unique_ptr<UploadBuffer<Sphere>> mSpheres = nullptr;
	std::unique_ptr<UploadBuffer<Vertex>> mVertices = nullptr;
	std::unique_ptr<UploadBuffer<Triangle>> mTriangles = nullptr;
	std::unique_ptr<KDTree> mKDTree = nullptr;
	ComPtr<ID3D12Resource> mOutputBuffer = nullptr;
	ComPtr<ID3D12RootSignature> mRootSignature = nullptr;
	ComPtr<ID3D12DescriptorHeap> mSrvDescriptorHeap = nullptr;
	ComPtr<ID3D12PipelineState> mPSO = nullptr;
	UINT mCbvSrvDescriptorSize = 0;
	UINT NumTriangles;
	Camera mCamera;
	POINT mLastMousePos;
};
RayTracingApp::RayTracingApp(HINSTANCE hInstance) : D3DApp(hInstance)
{
}
bool RayTracingApp::Initialize()
{
	if (!D3DApp::Initialize())
		return false;

	// Reset the command list to prep for initialization commands.
	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), nullptr));

	// Get the increment size of a descriptor in this heap type.  This is hardware specific, 
	// so we have to query this information.
	mCbvSrvDescriptorSize = md3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV);

	mCamera.SetPosition(0, 0, 0);

	LoadTextures();
	BuildBuffers();
	BuildRootSignature();
	BuildDescriptorHeaps();
	BuildConstantBuffers();
	BuildShadersAndInputLayout();
	/*	BuildShapeGeometry();
	BuildSkullGeometry();
	BuildMaterials();
	BuildRenderItems();
	BuildFrameResources();
*/
	BuildPSOs();
	// Execute the initialization commands.
	ThrowIfFailed(mCommandList->Close());
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);

	// Wait until initialization is complete.
	FlushCommandQueue();

	return true;
}
void RayTracingApp::LoadTextures()
{
	auto texMap = std::make_unique<Texture>();
	texMap->Name = "skyCubeMap";
	texMap->Filename = L"../../Textures/grasscube1024.dds";
	ThrowIfFailed(DirectX::CreateDDSTextureFromFile12(md3dDevice.Get(), mCommandList.Get(), texMap->Filename.c_str(), texMap->Resource, texMap->UploadHeap));
	mTextures[texMap->Name] = std::move(texMap);
}
void RayTracingApp::BuildBuffers()
{
	D3D12_RESOURCE_DESC texDesc;
	ZeroMemory(&texDesc, sizeof(D3D12_RESOURCE_DESC));
	texDesc.Dimension = D3D12_RESOURCE_DIMENSION_TEXTURE2D;
	texDesc.Alignment = 0;
	texDesc.Width = mClientWidth;
	texDesc.Height = mClientHeight;
	texDesc.DepthOrArraySize = 1;
	texDesc.MipLevels = 1;
	texDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	texDesc.SampleDesc.Count = 1;
	texDesc.SampleDesc.Quality = 0;
	texDesc.Layout = D3D12_TEXTURE_LAYOUT_UNKNOWN;
	texDesc.Flags = D3D12_RESOURCE_FLAG_ALLOW_UNORDERED_ACCESS;

	ThrowIfFailed(md3dDevice->CreateCommittedResource(
		&CD3DX12_HEAP_PROPERTIES(D3D12_HEAP_TYPE_DEFAULT),
		D3D12_HEAP_FLAG_NONE,
		&texDesc,
		D3D12_RESOURCE_STATE_COMMON,
		nullptr,
		IID_PPV_ARGS(&mOutputBuffer)));
}
void RayTracingApp::BuildRootSignature()
{
	CD3DX12_DESCRIPTOR_RANGE texTable;
	texTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_SRV, 1, 0, 0);

	CD3DX12_DESCRIPTOR_RANGE uavTable;
	uavTable.Init(D3D12_DESCRIPTOR_RANGE_TYPE_UAV, 1, 0);

	// Root parameter can be a table, root descriptor or root constants.
	CD3DX12_ROOT_PARAMETER slotRootParameter[8];

	// Perfomance TIP: Order from most frequent to least frequent.
	slotRootParameter[0].InitAsConstantBufferView(0);
	slotRootParameter[1].InitAsShaderResourceView(1);
	slotRootParameter[2].InitAsShaderResourceView(2);
	slotRootParameter[3].InitAsShaderResourceView(3);
	slotRootParameter[4].InitAsShaderResourceView(4);
	slotRootParameter[5].InitAsShaderResourceView(5);
	slotRootParameter[6].InitAsDescriptorTable(1, &texTable );
	slotRootParameter[7].InitAsDescriptorTable(1, &uavTable	);


	auto staticSamplers = GetStaticSamplers();

	// A root signature is an array of root parameters.
	CD3DX12_ROOT_SIGNATURE_DESC rootSigDesc(8, slotRootParameter,
		(UINT)staticSamplers.size(), staticSamplers.data(),
		D3D12_ROOT_SIGNATURE_FLAG_ALLOW_INPUT_ASSEMBLER_INPUT_LAYOUT);

	// create a root signature with a single slot which points to a descriptor range consisting of a single constant buffer
	ComPtr<ID3DBlob> serializedRootSig = nullptr;
	ComPtr<ID3DBlob> errorBlob = nullptr;
	HRESULT hr = D3D12SerializeRootSignature(&rootSigDesc, D3D_ROOT_SIGNATURE_VERSION_1,
		serializedRootSig.GetAddressOf(), errorBlob.GetAddressOf());

	if (errorBlob != nullptr)
	{
		::OutputDebugStringA((char*)errorBlob->GetBufferPointer());
	}
	ThrowIfFailed(hr);

	ThrowIfFailed(md3dDevice->CreateRootSignature(
		0,
		serializedRootSig->GetBufferPointer(),
		serializedRootSig->GetBufferSize(),
		IID_PPV_ARGS(mRootSignature.GetAddressOf())));
}
void RayTracingApp::BuildDescriptorHeaps()
{
	//
	// Create the SRV heap.
	//
	D3D12_DESCRIPTOR_HEAP_DESC srvHeapDesc = {};
	srvHeapDesc.NumDescriptors = 2;
	srvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_CBV_SRV_UAV;
	srvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_SHADER_VISIBLE;
	ThrowIfFailed(md3dDevice->CreateDescriptorHeap(&srvHeapDesc, IID_PPV_ARGS(&mSrvDescriptorHeap)));
	//
	// Fill out the heap with actual descriptors.
	//
	CD3DX12_CPU_DESCRIPTOR_HANDLE hDescriptor(mSrvDescriptorHeap->GetCPUDescriptorHandleForHeapStart());
	auto skyTex = mTextures["skyCubeMap"]->Resource;

	D3D12_SHADER_RESOURCE_VIEW_DESC srvDesc = {};
	srvDesc.Shader4ComponentMapping = D3D12_DEFAULT_SHADER_4_COMPONENT_MAPPING;
	srvDesc.ViewDimension = D3D12_SRV_DIMENSION_TEXTURECUBE;
	srvDesc.TextureCube.MostDetailedMip = 0;
	srvDesc.TextureCube.MipLevels = skyTex->GetDesc().MipLevels;
	srvDesc.TextureCube.ResourceMinLODClamp = 0.0f;
	srvDesc.Format = skyTex->GetDesc().Format;
	md3dDevice->CreateShaderResourceView(skyTex.Get(), &srvDesc, hDescriptor);


	hDescriptor.Offset(1, mCbvSrvDescriptorSize);
	D3D12_UNORDERED_ACCESS_VIEW_DESC uavDesc = {};
	uavDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
	uavDesc.ViewDimension = D3D12_UAV_DIMENSION_TEXTURE2D;
	uavDesc.Texture2D.MipSlice = 0;
	md3dDevice->CreateUnorderedAccessView(mOutputBuffer.Get(), nullptr, &uavDesc, hDescriptor);
}
void RayTracingApp::LoadModel(const char* file, std::vector<Vertex>& Vertices, std::vector<Triangle>& Triangles)
{
	std::ifstream fin(file);
	if (!fin)
	{
		MessageBox(0, L"Models/skull.txt not found.", 0, 0);
		return;
	}
	UINT vcount = 0;
	UINT tcount = 0;
	std::string ignore;
	fin >> ignore >> vcount;
	fin >> ignore >> tcount;
	fin >> ignore >> ignore >> ignore >> ignore;
	Vertices.resize(vcount);
	for (UINT i = 0; i < vcount; ++i)
	{
		Vertex& vertex = Vertices[i];
		fin >> vertex.position.x >> vertex.position.y >> vertex.position.z;
		fin >> vertex.normal.x >> vertex.normal.y >> vertex.normal.z;
		// Model does not have texture coordinates, so just zero them out.
		vertex.uv = { 0.0f, 0.0f };
	}
	fin >> ignore;
	fin >> ignore;
	fin >> ignore;
//	mTriangles = std::make_unique<UploadBuffer<Triangle>>(md3dDevice.Get(), tcount, false);
	Triangles.resize(tcount);
	for (UINT i = 0; i < tcount; ++i)
	{
		Triangle& triangle = Triangles[i];
		fin >> triangle.indices[0] >> triangle.indices[1] >> triangle.indices[2];
	//	mTriangles->CopyData(i, triangle);
	}
	fin.close();
}
void RayTracingApp::BuildConstantBuffers()
{
	mPassCB = std::make_unique<UploadBuffer<PassConstants>>(md3dDevice.Get(), 1, true);
	mSpheres = std::make_unique<UploadBuffer<Sphere>>(md3dDevice.Get(), 64, false);
	for (int i = 0; i < 64; i++)
	{
		int x = i % 8;
		int y = i / 8;
		Sphere sphere;
		sphere.radius = MathHelper::RandF(1, 4);
		sphere.position = XMFLOAT3(x * 8.f, sphere.radius, y * 8.f);
		sphere.albedo = XMFLOAT3(MathHelper::RandF(), MathHelper::RandF(), MathHelper::RandF());
		sphere.specular = XMFLOAT3(MathHelper::RandF(), MathHelper::RandF(), MathHelper::RandF());
		mSpheres->CopyData(i, sphere);
	}
	std::vector<Vertex> Vertices;
	std::vector<Triangle> Triangles;
	LoadModel("Models/car.txt", Vertices, Triangles);
	mVertices = std::make_unique<UploadBuffer<Vertex>>(md3dDevice.Get(), Vertices.size(), false);
	for (UINT i = 0; i < Vertices.size(); ++i)
		mVertices->CopyData(i, Vertices[i]);
	mTriangles = std::make_unique<UploadBuffer<Triangle>>(md3dDevice.Get(), Triangles.size(), false);
	for (UINT i = 0; i < Triangles.size(); ++i)
		mTriangles->CopyData(i, Triangles[i]);
	
	mKDTree = std::make_unique<KDTree>(Triangles.size());
	for (int i = 0; i < Triangles.size(); i++)
	{
		Triangle& tri = Triangles[i];
		mKDTree->AddTriangle(i, &Vertices[tri.indices[0]].position.x, &Vertices[tri.indices[1]].position.x, &Vertices[tri.indices[2]].position.x);
	}
	mKDTree->Build();
	NumTriangles = Triangles.size();
}
void RayTracingApp::BuildShadersAndInputLayout()
{
	mShaders["RayTracing"] = d3dUtil::CompileShader(L"Shaders\\RayTracing.hlsl", nullptr, "CS", "cs_5_0");
}
void RayTracingApp::BuildPSOs()
{
	D3D12_COMPUTE_PIPELINE_STATE_DESC computePsoDesc = {};
	computePsoDesc.pRootSignature = mRootSignature.Get();
	computePsoDesc.CS =
	{
		reinterpret_cast<BYTE*>(mShaders["RayTracing"]->GetBufferPointer()),
		mShaders["RayTracing"]->GetBufferSize()
	};
	computePsoDesc.Flags = D3D12_PIPELINE_STATE_FLAG_NONE;
	ThrowIfFailed(md3dDevice->CreateComputePipelineState(&computePsoDesc, IID_PPV_ARGS(&mPSOs["RayTracing"])));
}
void RayTracingApp::OnResize()
{
	D3DApp::OnResize();

	mCamera.SetLens(0.25f*MathHelper::Pi, AspectRatio(), 1.0f, 1000.0f);
}
void RayTracingApp::OnMouseDown(WPARAM btnState, int x, int y)
{
	mLastMousePos.x = x;
	mLastMousePos.y = y;

	SetCapture(mhMainWnd);
}

void RayTracingApp::OnMouseUp(WPARAM btnState, int x, int y)
{
	ReleaseCapture();
}

void RayTracingApp::OnMouseMove(WPARAM btnState, int x, int y)
{
	if ((btnState & MK_LBUTTON) != 0)
	{
		// Make each pixel correspond to a quarter of a degree.
		float dx = XMConvertToRadians(0.25f*static_cast<float>(x - mLastMousePos.x));
		float dy = XMConvertToRadians(0.25f*static_cast<float>(y - mLastMousePos.y));

		mCamera.Pitch(dy);
		mCamera.RotateY(dx);
	}

	mLastMousePos.x = x;
	mLastMousePos.y = y;
}

void RayTracingApp::OnKeyboardInput(const GameTimer& gt)
{
	const float dt = gt.DeltaTime();

	if (GetAsyncKeyState('W') & 0x8000)
		mCamera.Walk(10.0f*dt);

	if (GetAsyncKeyState('S') & 0x8000)
		mCamera.Walk(-10.0f*dt);

	if (GetAsyncKeyState('A') & 0x8000)
		mCamera.Strafe(-10.0f*dt);

	if (GetAsyncKeyState('D') & 0x8000)
		mCamera.Strafe(10.0f*dt);

	mCamera.UpdateViewMatrix();
}
void RayTracingApp::Update(const GameTimer& gt)
{
	OnKeyboardInput(gt);

	mCamera.UpdateViewMatrix();

	PassConstants passConstants;

	XMMATRIX view = mCamera.GetView();
	XMMATRIX proj = mCamera.GetProj();
	XMMATRIX viewProj = XMMatrixMultiply(view, proj);
	XMMATRIX invView = XMMatrixInverse(&XMMatrixDeterminant(view), view);
	XMMATRIX invProj = XMMatrixInverse(&XMMatrixDeterminant(proj), proj);
	XMMATRIX invViewProj = XMMatrixInverse(&XMMatrixDeterminant(viewProj), viewProj);

	passConstants.View = (view);
	passConstants.Proj = (proj);
	passConstants.ViewProj = (viewProj);
	passConstants.InvView = (invView);
	passConstants.InvProj = (invProj);
	passConstants.InvViewProj = (invViewProj);
	passConstants.DirectionalLight = XMFLOAT4(0.707f, -0.707f, 0, 1);
	passConstants.NumSpheres = 64;
	passConstants.NumTriangles = NumTriangles;
	mPassCB->CopyData(0, passConstants);
}
void RayTracingApp::Draw(const GameTimer& gt)
{
	// Reuse the memory associated with command recording.
	// We can only reset when the associated command lists have finished execution on the GPU.
	ThrowIfFailed(mDirectCmdListAlloc->Reset());

	// A command list can be reset after it has been added to the command queue via ExecuteCommandList.
	// Reusing the command list reuses memory.
	ThrowIfFailed(mCommandList->Reset(mDirectCmdListAlloc.Get(), mPSOs["RayTracing"].Get()));

	ID3D12DescriptorHeap* descriptorHeaps[] = { mSrvDescriptorHeap.Get() };
	mCommandList->SetDescriptorHeaps(_countof(descriptorHeaps), descriptorHeaps);

	mCommandList->SetComputeRootSignature(mRootSignature.Get());

	mCommandList->SetComputeRootConstantBufferView(0, mPassCB->Resource()->GetGPUVirtualAddress());
//	mCommandList->SetComputeRootConstantBufferView(1, mSpheres->Resource()->GetGPUVirtualAddress());
	mCommandList->SetComputeRootShaderResourceView(1, mSpheres->Resource()->GetGPUVirtualAddress());
	mCommandList->SetComputeRootShaderResourceView(2, mVertices->Resource()->GetGPUVirtualAddress());
	mCommandList->SetComputeRootShaderResourceView(3, mTriangles->Resource()->GetGPUVirtualAddress());

	CD3DX12_GPU_DESCRIPTOR_HANDLE hGpuDescriptor(mSrvDescriptorHeap->GetGPUDescriptorHandleForHeapStart());
	
	mCommandList->SetComputeRootDescriptorTable(6, hGpuDescriptor);
	hGpuDescriptor.Offset(1, mCbvSrvDescriptorSize);
	mCommandList->SetComputeRootDescriptorTable(7, hGpuDescriptor);

	mCommandList->Dispatch((UINT)ceilf(mClientWidth / 8.f), (UINT)ceilf(mClientHeight / 8.f), 1);

	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_PRESENT, D3D12_RESOURCE_STATE_COPY_DEST));

	// Schedule to copy the data to the default buffer to the readback buffer.
	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mOutputBuffer.Get(),
		D3D12_RESOURCE_STATE_COMMON, D3D12_RESOURCE_STATE_COPY_SOURCE));

	mCommandList->CopyResource(CurrentBackBuffer(), mOutputBuffer.Get());

	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(mOutputBuffer.Get(),
		D3D12_RESOURCE_STATE_COPY_SOURCE, D3D12_RESOURCE_STATE_COMMON));

	mCommandList->ResourceBarrier(1, &CD3DX12_RESOURCE_BARRIER::Transition(CurrentBackBuffer(),
		D3D12_RESOURCE_STATE_COPY_DEST, D3D12_RESOURCE_STATE_PRESENT));


	// Done recording commands.
	ThrowIfFailed(mCommandList->Close());

	// Add the command list to the queue for execution.
	ID3D12CommandList* cmdsLists[] = { mCommandList.Get() };
	mCommandQueue->ExecuteCommandLists(_countof(cmdsLists), cmdsLists);
	ThrowIfFailed(mSwapChain->Present(0, 0));
	mCurrBackBuffer = (mCurrBackBuffer + 1) % SwapChainBufferCount;

	// Wait until frame commands are complete.  This waiting is inefficient and is
	// done for simplicity.  Later we will show how to organize our rendering code
	// so we do not have to wait per frame.
	FlushCommandQueue();
}
std::array<const CD3DX12_STATIC_SAMPLER_DESC, 6> RayTracingApp::GetStaticSamplers()
{
	// Applications usually only need a handful of samplers.  So just define them all up front
	// and keep them available as part of the root signature.  

	const CD3DX12_STATIC_SAMPLER_DESC pointWrap(
		0, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC pointClamp(
		1, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_POINT, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearWrap(
		2, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC linearClamp(
		3, // shaderRegister
		D3D12_FILTER_MIN_MAG_MIP_LINEAR, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP); // addressW

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicWrap(
		4, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_WRAP,  // addressW
		0.0f,                             // mipLODBias
		8);                               // maxAnisotropy

	const CD3DX12_STATIC_SAMPLER_DESC anisotropicClamp(
		5, // shaderRegister
		D3D12_FILTER_ANISOTROPIC, // filter
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressU
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressV
		D3D12_TEXTURE_ADDRESS_MODE_CLAMP,  // addressW
		0.0f,                              // mipLODBias
		8);                                // maxAnisotropy

	return {
		pointWrap, pointClamp,
		linearWrap, linearClamp,
		anisotropicWrap, anisotropicClamp };
}

int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE prevInstance, PSTR cmdLine, int showCmd)
{
	// Enable run-time memory check for debug builds.
#if defined(DEBUG) | defined(_DEBUG)
	_CrtSetDbgFlag(_CRTDBG_ALLOC_MEM_DF | _CRTDBG_LEAK_CHECK_DF);
#endif

	try
	{
		RayTracingApp theApp(hInstance);
		if (!theApp.Initialize())
			return 0;

		return theApp.Run();
	}
	catch (DxException& e)
	{
		MessageBox(nullptr, e.ToString().c_str(), L"HR Failed", MB_OK);
		return 0;
	}
}