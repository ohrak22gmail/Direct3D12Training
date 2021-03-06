#include "pch.h"
#include "Device.h"
#include "../Common/DirectXHelper.h"

using namespace CreateDevice;
using namespace Microsoft::WRL;
using namespace Windows::Foundation;
using namespace Windows::Graphics::Display;
using namespace Windows::UI::Core;
using namespace Windows::UI::Xaml::Controls;
using namespace Platform;
using namespace DX;

// Device의 생성자입니다.
Device::Device(DXGI_FORMAT backBufferFormat, DXGI_FORMAT depthBufferFormat) :
	m_currentFrame(0),
	m_screenViewport(),
	m_rtvDescriptorSize(0),
	m_fenceEvent(0),
	m_backBufferFormat(backBufferFormat),
	m_fenceValues{},
	m_outputSize(),
	m_deviceRemoved(false)
{
	CreateDeviceResources();
}

// Direct3D 장치를 구성하고 해당 장치에 대한 핸들 및 장치 컨텍스트를 저장합니다.
void Device::CreateDeviceResources()
{
#if defined(_DEBUG)
	// 프로젝트가 디버그 빌드 중인 경우 SDK 레이어를 통한 디버깅이 가능하도록 설정하세요.
	{
		ComPtr<ID3D12Debug> debugController;
		if (SUCCEEDED(D3D12GetDebugInterface(IID_PPV_ARGS(&debugController))))
		{
			debugController->EnableDebugLayer();
		}
	}
#endif

	DX::ThrowIfFailed(CreateDXGIFactory1(IID_PPV_ARGS(&m_dxgiFactory)));

	ComPtr<IDXGIAdapter1> adapter;
	GetHardwareAdapter(&adapter);

	// Direct3D 12 API 장치 개체를 만듭니다.
	HRESULT hr = D3D12CreateDevice(
		adapter.Get(),					// 하드웨어 어댑터입니다.
		D3D_FEATURE_LEVEL_11_0,			// 이 앱이 지원할 수 있는 최대 기능 수준입니다.
		IID_PPV_ARGS(&m_d3dDevice)		// 만들어진 Direct3D 장치를 반환합니다.
	);

#if defined(_DEBUG)
	if (FAILED(hr))
	{
		// 초기화에 실패하면 WARP 장치로 대체됩니다.
		// WARP에 대한 자세한 내용은 다음을 참조하세요. 
		// http://go.microsoft.com/fwlink/?LinkId=286690

		ComPtr<IDXGIAdapter> warpAdapter;
		DX::ThrowIfFailed(m_dxgiFactory->EnumWarpAdapter(IID_PPV_ARGS(&warpAdapter)));

		hr = D3D12CreateDevice(warpAdapter.Get(), D3D_FEATURE_LEVEL_11_0, IID_PPV_ARGS(&m_d3dDevice));
	}
#endif

	DX::ThrowIfFailed(hr);

	// 명령 큐를 만듭니다.
	D3D12_COMMAND_QUEUE_DESC queueDesc = {};
	queueDesc.Flags = D3D12_COMMAND_QUEUE_FLAG_NONE;
	queueDesc.Type = D3D12_COMMAND_LIST_TYPE_DIRECT;

	DX::ThrowIfFailed(m_d3dDevice->CreateCommandQueue(&queueDesc, IID_PPV_ARGS(&m_commandQueue)));
	NAME_D3D12_OBJECT(m_commandQueue);

	// 렌더링 대상 뷰에 대한 설명자 힙을 만듭니다.
	D3D12_DESCRIPTOR_HEAP_DESC rtvHeapDesc = {};
	rtvHeapDesc.NumDescriptors = g_frameCount;
	rtvHeapDesc.Type = D3D12_DESCRIPTOR_HEAP_TYPE_RTV;
	rtvHeapDesc.Flags = D3D12_DESCRIPTOR_HEAP_FLAG_NONE;
	DX::ThrowIfFailed(m_d3dDevice->CreateDescriptorHeap(&rtvHeapDesc, IID_PPV_ARGS(&m_rtvHeap)));
	NAME_D3D12_OBJECT(m_rtvHeap);

	m_rtvDescriptorSize = m_d3dDevice->GetDescriptorHandleIncrementSize(D3D12_DESCRIPTOR_HEAP_TYPE_RTV);

	for (UINT n = 0; n < g_frameCount; n++)
	{
		DX::ThrowIfFailed(
			m_d3dDevice->CreateCommandAllocator(D3D12_COMMAND_LIST_TYPE_DIRECT, IID_PPV_ARGS(&m_commandAllocators[n]))
		);
	}

	// 동기화 개체를 만듭니다.
	DX::ThrowIfFailed(m_d3dDevice->CreateFence(m_fenceValues[m_currentFrame], D3D12_FENCE_FLAG_NONE, IID_PPV_ARGS(&m_fence)));
	m_fenceValues[m_currentFrame]++;

	m_fenceEvent = CreateEventEx(nullptr, FALSE, FALSE, EVENT_ALL_ACCESS);
}

// 이 메서드는 Direct3D 12를 지원하는 사용 가능한 첫 번째 하드웨어 어댑터를 가져옵니다.
// 그러한 어댑터를 찾을 수 없는 경우 *ppAdapter는 nullptr로 설정됩니다.
void Device::GetHardwareAdapter(IDXGIAdapter1** ppAdapter)
{
	ComPtr<IDXGIAdapter1> adapter;
	*ppAdapter = nullptr;

	for (UINT adapterIndex = 0; DXGI_ERROR_NOT_FOUND != m_dxgiFactory->EnumAdapters1(adapterIndex, &adapter); adapterIndex++)
	{
		DXGI_ADAPTER_DESC1 desc;
		adapter->GetDesc1(&desc);

		if (desc.Flags & DXGI_ADAPTER_FLAG_SOFTWARE)
		{
			// 기본 렌더링 드라이버 어댑터를 선택하지 마세요.
			continue;
		}

		// 어댑터에서 Direct3D 12를 지원하지만 실제 장치를 아직 만들지 않았는지
		// 확인합니다.
		if (SUCCEEDED(D3D12CreateDevice(adapter.Get(), D3D_FEATURE_LEVEL_11_0, _uuidof(ID3D12Device), nullptr)))
		{
			break;
		}
	}

	*ppAdapter = adapter.Detach();
}

// 이 메서드는 CoreWindow를 만들거나 다시 만들 때 호출됩니다.
void Device::SetWindow(CoreWindow^ window)
{
	DisplayInformation^ currentDisplayInformation = DisplayInformation::GetForCurrentView();

	m_window = window;

	UINT backBufferWidth = lround(m_outputSize.Width);
	UINT backBufferHeight = lround(m_outputSize.Height);

	if (m_swapChain != nullptr)
	{
		// 스왑 체인이 이미 존재할 경우 크기를 조정합니다.
		HRESULT hr = m_swapChain->ResizeBuffers(g_frameCount, backBufferWidth, backBufferHeight, m_backBufferFormat, 0);

		if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
		{
			// 어떤 이유로든 장치가 제거된 경우 새 장치와 스왑 체인을 만들어야 합니다.
			m_deviceRemoved = true;

			// 이 메서드를 계속 실행하지 마세요. Device가 제거되고 다시 만들어집니다.
			return;
		}
		else
		{
			DX::ThrowIfFailed(hr);
		}
	}
	else
	{
		// 그렇지 않으면 기존 Direct3D 장치와 동일한 어댑터를 사용하여 새 항목을 만듭니다.
		DXGI_SWAP_CHAIN_DESC1 swapChainDesc = {};

		swapChainDesc.Width = backBufferWidth;						// 창의 크기를 맞춥니다.
		swapChainDesc.Height = backBufferHeight;
		swapChainDesc.Format = m_backBufferFormat;
		swapChainDesc.Stereo = false;
		swapChainDesc.SampleDesc.Count = 1;							// 다중 샘플링을 사용하지 않습니다.
		swapChainDesc.SampleDesc.Quality = 0;
		swapChainDesc.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;
		swapChainDesc.BufferCount = g_frameCount;					// 3중 버퍼링을 사용하여 대기 시간을 최소화합니다.
		swapChainDesc.SwapEffect = DXGI_SWAP_EFFECT_FLIP_DISCARD;	// 모든 Windows 유니버설 앱은 _FLIP_ SwapEffects를 사용해야 합니다.
		swapChainDesc.Flags = 0;
		swapChainDesc.Scaling = DXGI_SCALING_NONE;
		swapChainDesc.AlphaMode = DXGI_ALPHA_MODE_IGNORE;

		ComPtr<IDXGISwapChain1> swapChain;
		DX::ThrowIfFailed(
			m_dxgiFactory->CreateSwapChainForCoreWindow(
				m_commandQueue.Get(),								// 스왑 체인에는 DirectX 12의 명령 큐에 대한 참조가 필요합니다.
				reinterpret_cast<IUnknown*>(m_window.Get()),
				&swapChainDesc,
				nullptr,
				&swapChain
			)
		);

		DX::ThrowIfFailed(swapChain.As(&m_swapChain));
	}

	// 스왑 체인 백 버퍼의 렌더링 대상 뷰를 만듭니다.
	{
		m_currentFrame = m_swapChain->GetCurrentBackBufferIndex();
		CD3DX12_CPU_DESCRIPTOR_HANDLE rtvDescriptor(m_rtvHeap->GetCPUDescriptorHandleForHeapStart());
		for (UINT n = 0; n < g_frameCount; n++)
		{
			DX::ThrowIfFailed(m_swapChain->GetBuffer(n, IID_PPV_ARGS(&m_renderTargets[n])));
			m_d3dDevice->CreateRenderTargetView(m_renderTargets[n].Get(), nullptr, rtvDescriptor);
			rtvDescriptor.Offset(m_rtvDescriptorSize);

			WCHAR name[25];
			if (swprintf_s(name, L"m_renderTargets[%u]", n) > 0)
			{
				DX::SetName(m_renderTargets[n].Get(), name);
			}
		}
	}

	// 전체 창을 대상으로 하기 위한 3D 렌더링 뷰포트를 설정합니다.
	m_screenViewport = { 0.0f, 0.0f, m_outputSize.Width, m_outputSize.Height, 0.0f, 1.0f };
}

// 스왑 체인의 콘텐츠를 화면에 표시합니다.
void Device::Present()
{
	// 첫 번째 인수는 DXGI에 VSync까지 차단하도록 지시하여 응용 프로그램이
	// 다음 VSync까지 대기하도록 합니다. 이를 통해 화면에 표시되지 않는 프레임을
	// 렌더링하는 주기를 낭비하지 않을 수 있습니다.
	HRESULT hr = m_swapChain->Present(1, 0);

	// 연결이 끊기거나 드라이버 업그레이드로 인해 장치가 제거되면 
	// 모든 장치 리소스를 다시 만들어야 합니다.
	if (hr == DXGI_ERROR_DEVICE_REMOVED || hr == DXGI_ERROR_DEVICE_RESET)
	{
		m_deviceRemoved = true;
	}
	else
	{
		DX::ThrowIfFailed(hr);

		MoveToNextFrame();
	}
}

// 다음 프레임을 렌더링하도록 준비합니다.
void Device::MoveToNextFrame()
{
	// 큐에서 신호 명령을 예약합니다.
	const UINT64 currentFenceValue = m_fenceValues[m_currentFrame];
	DX::ThrowIfFailed(m_commandQueue->Signal(m_fence.Get(), currentFenceValue));

	//프레임 인덱스로 이동합니다.
	m_currentFrame = m_swapChain->GetCurrentBackBufferIndex();

	// 다음 프레임을 시작할 준비가 되었는지 확인하세요.
	if (m_fence->GetCompletedValue() < m_fenceValues[m_currentFrame])
	{
		DX::ThrowIfFailed(m_fence->SetEventOnCompletion(m_fenceValues[m_currentFrame], m_fenceEvent));
		WaitForSingleObjectEx(m_fenceEvent, INFINITE, FALSE);
	}

	// 다음 프레임에 대한 fence 값을 설정합니다.
	m_fenceValues[m_currentFrame] = currentFenceValue + 1;
}


