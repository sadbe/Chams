#include <Windows.h>
#include <d3d11.h>
#include <Windows.h>
#include <stdio.h>
#pragma comment(lib, "d3d11.lib")
 
#include "FW1FontWrapper\FW1FontWrapper.h"
#include "BeaEngine/headers/BeaEngine.h"
 
#pragma comment(lib, "BeaEngineCheetah64.lib")
#include <D3D11Shader.h>
#include <D3Dcompiler.h>//generateshader
#pragma comment(lib, "D3dcompiler.lib")
#include "MinHook\include\MinHook.h"

typedef HRESULT (__stdcall *D3D11PresentHook) (IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags); 
typedef void (__stdcall *D3D11DrawIndexedHook) (ID3D11DeviceContext* pContext, UINT IndexCount, UINT StartIndexLocation, INT BaseVertexLocation);
typedef void (__stdcall *D3D11ClearRenderTargetViewHook) (ID3D11DeviceContext* pContext, ID3D11RenderTargetView *pRenderTargetView, const FLOAT ColorRGBA[4]);
typedef void(__stdcall *D3D11CreateQueryHook) (ID3D11Device* pDevice, const D3D11_QUERY_DESC *pQueryDesc, ID3D11Query **ppQuery);
D3D11CreateQueryHook phookD3D11CreateQuery = NULL;
ID3D11Device *pDevice = NULL;
ID3D11DeviceContext *pContext = NULL;
 
DWORD_PTR* pSwapChainVtable = NULL;
DWORD_PTR* pDeviceContextVTable = NULL;
DWORD_PTR* pDeviceVTable = NULL;

D3D11PresentHook phookD3D11Present = NULL;
D3D11DrawIndexedHook phookD3D11DrawIndexed = NULL;
D3D11ClearRenderTargetViewHook phookD3D11ClearRenderTargetView = NULL;
 
IFW1Factory *pFW1Factory = NULL;
IFW1FontWrapper *pFontWrapper = NULL;
 
bool firstTime = true;
void* detourBuffer[3];
bool acikkapali = false;

HRESULT GenerateShader(ID3D11Device* pD3DDevice, ID3D11PixelShader** pShader, float r, float g, float b)
{
	/*credits raiders*/
	char szCast[] = "struct VS_OUT"
		"{"
		"    float4 Position   : SV_Position;"
		"    float4 Color    : COLOR0;"
		"};"

		"float4 main( VS_OUT input ) : SV_Target"
		"{"
		"    float4 fake;"
		"    fake.a = 1.0;"
		"    fake.r = %f;"
		"    fake.g = %f;"
		"    fake.b = %f;"
		"    return fake;"
		"}";
	ID3D10Blob*    pBlob;
	ID3DBlob* d3dErrorMsgBlob;
	char szPixelShader[1000];

	sprintf(szPixelShader, szCast, r, g, b);

	HRESULT hr = D3DCompile(szPixelShader, sizeof(szPixelShader), "shader", NULL, NULL, "main", "ps_4_0", NULL, NULL, &pBlob, &d3dErrorMsgBlob);

	if (FAILED(hr))
		return hr;

	hr = pD3DDevice->CreatePixelShader((DWORD*)pBlob->GetBufferPointer(), pBlob->GetBufferSize(), NULL, pShader);

	if (FAILED(hr))
		return hr;

	return S_OK;
}


ID3D11RenderTargetView* RenderTargetView = NULL;
ID3D11Texture2D* RenderTargetTexture;
ID3D11DepthStencilState* g_depthEnabled;
ID3D11DepthStencilState* g_depthDisabled;

ID3D11PixelShader* psRed = NULL;
ID3D11PixelShader* psGreen = NULL;
ID3D11Device* pD3DDevicex = 0;
enum eDepthState
{
	ENABLED,
	DISABLED,
	READ_NO_WRITE,
	NO_READ_NO_WRITE,
	_DEPTH_COUNT
};
ID3D11DepthStencilState* myDepthStencilStates[static_cast<int>(eDepthState::_DEPTH_COUNT)];
void SetDepthStencilState(eDepthState aState)
{
	pContext->OMSetDepthStencilState(myDepthStencilStates[aState], 1);
}
ID3D11Buffer *inBuffer;
DXGI_FORMAT inFormat;
UINT        inOffset;
D3D11_BUFFER_DESC indesc;


ID3D11DepthStencilState *one;
D3D11_DEPTH_STENCIL_DESC oneDesc;
UINT ref;
UINT Stride;
ID3D11Buffer *veBuffer;
UINT veBufferOffset = 0;
HRESULT __stdcall hookD3D11Present(IDXGISwapChain* pSwapChain, UINT SyncInterval, UINT Flags)
{
	if (firstTime)
	{
		//get device and context
		if (SUCCEEDED(pSwapChain->GetDevice(__uuidof(ID3D11Device), (void **)&pDevice)))
		{
			pSwapChain->GetDevice(__uuidof(pDevice), (void**)&pDevice);
			pDevice->GetImmediateContext(&pContext);
		}

	
		//create font
		HRESULT hResult = FW1CreateFactory(FW1_VERSION, &pFW1Factory);
		hResult = pFW1Factory->CreateFontWrapper(pDevice, L"Tahoma", &pFontWrapper);
		pFW1Factory->Release();

		// use the back buffer address to create the render target
		//if (SUCCEEDED(pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), reinterpret_cast<LPVOID*>(&RenderTargetTexture))))
		if (SUCCEEDED(pSwapChain->GetBuffer(0, __uuidof(ID3D11Texture2D), (LPVOID*)&RenderTargetTexture)))
		{
			pDevice->CreateRenderTargetView(RenderTargetTexture, NULL, &RenderTargetView);
			RenderTargetTexture->Release();
		}

		firstTime = false;
	}


	if(!psGreen)
	GenerateShader(pDevice, &psGreen, 0.0f, 0.8f, 0.0f);

	if(!psRed)
	GenerateShader(pDevice, &psRed, 0.8f, 0.0f, 0.0f);

	pContext->OMSetRenderTargets(1, &RenderTargetView, NULL);

	//draw
	if (pFontWrapper)
		pFontWrapper->DrawString(pContext, L"D3D11 Hook", 14, 16.0f, 16.0f, 0xffff1414, FW1_RESTORESTATE);

	


	return phookD3D11Present(pSwapChain, SyncInterval, Flags);
}


void __stdcall hookD3D11DrawIndexed(ID3D11DeviceContext* pContext, UINT IndexCount, UINT StartIndexLocation, INT BaseVertexLocation)
{
	
	pContext->IAGetVertexBuffers(0, 1, &veBuffer, &Stride, &veBufferOffset);

	pContext->GetDevice(&pD3DDevicex);
	pContext->IAGetIndexBuffer(&inBuffer, &inFormat, &inOffset);
	if (inBuffer)
		inBuffer->GetDesc(&indesc);

	
	if (acikkapali)
	{    // stride 36 old
		
		if (Stride == 24 || (indesc.ByteWidth == 4096) ) {

			pContext->OMGetDepthStencilState(&one, &ref);
			one->GetDesc(&oneDesc);
			oneDesc.DepthEnable = false;
			pD3DDevicex->CreateDepthStencilState(&oneDesc, &one);
			pContext->OMSetDepthStencilState(one, ref);
			pContext->PSSetShader(psRed, NULL, NULL); //
			phookD3D11DrawIndexed(pContext, IndexCount, StartIndexLocation, BaseVertexLocation);
			oneDesc.DepthEnable = true;
			pD3DDevicex->CreateDepthStencilState(&oneDesc, &one);
			pContext->OMSetDepthStencilState(one, ref);
			pContext->PSSetShader(psGreen, NULL, NULL); //

			///////////////////////////////////////////
			
		}
	
	}
	
	return phookD3D11DrawIndexed(pContext, IndexCount, StartIndexLocation, BaseVertexLocation);
}
 
void __stdcall hookD3D11ClearRenderTargetView(ID3D11DeviceContext* pContext, ID3D11RenderTargetView *pRenderTargetView, const FLOAT ColorRGBA[4])
{
	return phookD3D11ClearRenderTargetView(pContext, pRenderTargetView, ColorRGBA);
}
void __stdcall hookD3D11CreateQuery(ID3D11Device* pDevice,  D3D11_QUERY_DESC *pQueryDesc, ID3D11Query **ppQuery)
{

	if (acikkapali)
	{
		if (pQueryDesc->Query == D3D11_QUERY_OCCLUSION)
		{
			D3D11_QUERY_DESC oqueryDesc = CD3D11_QUERY_DESC();
			(&oqueryDesc)->MiscFlags = pQueryDesc->MiscFlags;
			(&oqueryDesc)->Query = D3D11_QUERY_TIMESTAMP;
		
			return phookD3D11CreateQuery(pDevice, &oqueryDesc, ppQuery);
		}
	}
	return phookD3D11CreateQuery(pDevice, pQueryDesc, ppQuery);
}

const void* DetourFuncVTable(SIZE_T* src, const BYTE* dest, const DWORD index)
{
	DWORD dwVirtualProtectBackup;
	SIZE_T* const indexPtr = &src[index];
	const void* origFunc = (void*)*indexPtr;
	VirtualProtect(indexPtr, sizeof(SIZE_T), PAGE_EXECUTE_READWRITE, &dwVirtualProtectBackup);
	*indexPtr = (SIZE_T)dest;
	VirtualProtect(indexPtr, sizeof(SIZE_T), dwVirtualProtectBackup, &dwVirtualProtectBackup);
	return origFunc;
}
DWORD_PTR* pContextVTable = NULL;
LRESULT CALLBACK DXGIMsgProc(HWND hwnd, UINT uMsg, WPARAM wParam, LPARAM lParam) { return DefWindowProc(hwnd, uMsg, wParam, lParam); }
#ifdef _WIN64
	struct HookContext
	{
		BYTE original_code[64];
		SIZE_T dst_ptr;
		BYTE far_jmp[6];
	};
 
	HookContext* presenthook64;
 
	const unsigned int DisasmLengthCheck(const SIZE_T address, const unsigned int jumplength)
	{
		DISASM disasm;
		memset(&disasm, 0, sizeof(DISASM));
 
		disasm.EIP = (UIntPtr)address;
		disasm.Archi = 0x40;
 
		unsigned int processed = 0;
		while (processed < jumplength)
		{
			const int len = Disasm(&disasm);
			if (len == UNKNOWN_OPCODE)
			{
				++disasm.EIP;
			}
			else
			{
				processed += len;
				disasm.EIP += len;
			}
		}
 
		return processed;
	}
 
	const void* DetourFunc64(BYTE* const src, const BYTE* dest, const unsigned int jumplength)
	{
		// Allocate a memory page that is going to contain executable code.
		MEMORY_BASIC_INFORMATION mbi;
		for (SIZE_T addr = (SIZE_T)src; addr > (SIZE_T)src - 0x80000000; addr = (SIZE_T)mbi.BaseAddress - 1)
		{
			if (!VirtualQuery((LPCVOID)addr, &mbi, sizeof(mbi)))
			{
				break;
			}
 
			if (mbi.State == MEM_FREE)
			{
				if (presenthook64 = (HookContext*)VirtualAlloc(mbi.BaseAddress, 0x1000, MEM_RESERVE | MEM_COMMIT, PAGE_EXECUTE_READWRITE))
				{
					break;
				}
			}
		}
 
		// If allocating a memory page failed, the function failed.
		if (!presenthook64)
		{
			return NULL;
		}
 
		// Select a pointer slot for the memory page to be freed on unload.
		for (int i = 0; i < sizeof(detourBuffer) / sizeof(void*); ++i)
		{
			if (!detourBuffer[i])
			{
				detourBuffer[i] = presenthook64;
				break;
			}
		}
 
		// Save original code and apply detour. The detour code is:
		// push rax
		// movabs rax, 0xCCCCCCCCCCCCCCCC
		// xchg rax, [rsp]
		// ret
		BYTE detour[] = { 0x50, 0x48, 0xB8, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0xCC, 0x48, 0x87, 0x04, 0x24, 0xC3 };
		const unsigned int length = DisasmLengthCheck((SIZE_T)src, jumplength);
		memcpy(presenthook64->original_code, src, length);
		memcpy(&presenthook64->original_code[length], detour, sizeof(detour));
		*(SIZE_T*)&presenthook64->original_code[length + 3] = (SIZE_T)src + length;
 
		// Build a far jump to the destination function.
		*(WORD*)&presenthook64->far_jmp = 0x25FF;
		*(DWORD*)(presenthook64->far_jmp + 2) = (DWORD)((SIZE_T)presenthook64 - (SIZE_T)src + FIELD_OFFSET(HookContext, dst_ptr) - 6);
		presenthook64->dst_ptr = (SIZE_T)dest;
 
		// Write the hook to the original function.
		DWORD flOld = 0;
		VirtualProtect(src, 6, PAGE_EXECUTE_READWRITE, &flOld);
		memcpy(src, presenthook64->far_jmp, sizeof(presenthook64->far_jmp));
		VirtualProtect(src, 6, flOld, &flOld);
 
		// Return a pointer to the original code.
		return presenthook64->original_code;
	}
#else
	const void* DetourFunc(BYTE* const src, const BYTE* dest, const DWORD length)
	{
		BYTE* jump = new BYTE[length + 5];
		for (int i = 0; i < sizeof(detourBuffer) / sizeof(void*); ++i)
		{
			if (!detourBuffer[i])
			{
				detourBuffer[i] = jump;
				break;
			}
		}
 
		DWORD dwVirtualProtectBackup;
		VirtualProtect(src, length, PAGE_READWRITE, &dwVirtualProtectBackup);
 
		memcpy(jump, src, length);
		jump += length;
 
		jump[0] = 0xE9;
		*(DWORD*)(jump + 1) = (DWORD)(src + length - jump) - 5;
 
		src[0] = 0xE9;
		*(DWORD*)(src + 1) = (DWORD)(dest - src) - 5;
 
		VirtualProtect(src, length, dwVirtualProtectBackup, &dwVirtualProtectBackup);
 
		return jump - length;
	}
#endif
	DWORD __stdcall InitializeHook(LPVOID)
	{

		HMODULE hDXGIDLL = 0;
		do
		{
			hDXGIDLL = GetModuleHandle("dxgi.dll");
			Sleep(8000);
		} while (!hDXGIDLL);
		Sleep(100);


		IDXGISwapChain* pSwapChain;

		WNDCLASSEXA wc = { sizeof(WNDCLASSEX), CS_CLASSDC, DXGIMsgProc, 0L, 0L, GetModuleHandleA(NULL), NULL, NULL, NULL, NULL, "DX", NULL };
		RegisterClassExA(&wc);
		HWND hWnd = CreateWindowA("DX", NULL, WS_OVERLAPPEDWINDOW, 100, 100, 300, 300, NULL, NULL, wc.hInstance, NULL);

		D3D_FEATURE_LEVEL requestedLevels[] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_1 };
		D3D_FEATURE_LEVEL obtainedLevel;
		ID3D11Device* d3dDevice = nullptr;
		ID3D11DeviceContext* d3dContext = nullptr;

		DXGI_SWAP_CHAIN_DESC scd;
		ZeroMemory(&scd, sizeof(scd));
		scd.BufferCount = 1;
		scd.BufferDesc.Format = DXGI_FORMAT_R8G8B8A8_UNORM;
		scd.BufferDesc.Scaling = DXGI_MODE_SCALING_UNSPECIFIED;
		scd.BufferDesc.ScanlineOrdering = DXGI_MODE_SCANLINE_ORDER_UNSPECIFIED;
		scd.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT;

		scd.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH;
		scd.OutputWindow = hWnd;
		scd.SampleDesc.Count = 1;
		scd.SwapEffect = DXGI_SWAP_EFFECT_DISCARD;
		scd.Windowed = ((GetWindowLongPtr(hWnd, GWL_STYLE) & WS_POPUP) != 0) ? false : true;

		// LibOVR 0.4.3 requires that the width and height for the backbuffer is set even if
		// you use windowed mode, despite being optional according to the D3D11 documentation.
		scd.BufferDesc.Width = 1;
		scd.BufferDesc.Height = 1;
		scd.BufferDesc.RefreshRate.Numerator = 0;
		scd.BufferDesc.RefreshRate.Denominator = 1;

		UINT createFlags = 0;
#ifdef _DEBUG
		// This flag gives you some quite wonderful debug text. Not wonderful for performance, though!
		createFlags |= D3D11_CREATE_DEVICE_DEBUG;
#endif

		IDXGISwapChain* d3dSwapChain = 0;

		if (FAILED(D3D11CreateDeviceAndSwapChain(
			nullptr,
			D3D_DRIVER_TYPE_HARDWARE,
			nullptr,
			createFlags,
			requestedLevels,
			sizeof(requestedLevels) / sizeof(D3D_FEATURE_LEVEL),
			D3D11_SDK_VERSION,
			&scd,
			&pSwapChain,
			&pDevice,
			&obtainedLevel,
			&pContext)))
		{
			MessageBox(hWnd, "Failed to create directX device and swapchain!", "Error", MB_ICONERROR);
			return NULL;
		}


		pSwapChainVtable = (DWORD_PTR*)pSwapChain;
		pSwapChainVtable = (DWORD_PTR*)pSwapChainVtable[0];

		pContextVTable = (DWORD_PTR*)pContext;
		pContextVTable = (DWORD_PTR*)pContextVTable[0];

		pDeviceVTable = (DWORD_PTR*)pDevice;
		pDeviceVTable = (DWORD_PTR*)pDeviceVTable[0];

		if (MH_Initialize() != MH_OK) { return 1; }
		if (MH_CreateHook((DWORD_PTR*)pSwapChainVtable[8], hookD3D11Present, reinterpret_cast<void**>(&phookD3D11Present)) != MH_OK) { return 1; }
		if (MH_EnableHook((DWORD_PTR*)pSwapChainVtable[8]) != MH_OK) { return 1; }
		if (MH_CreateHook((DWORD_PTR*)pContextVTable[12], hookD3D11DrawIndexed, reinterpret_cast<void**>(&phookD3D11DrawIndexed)) != MH_OK) { return 1; }
		if (MH_EnableHook((DWORD_PTR*)pContextVTable[12]) != MH_OK) { return 1; }
		if (MH_CreateHook((DWORD_PTR*)pDeviceVTable[24], hookD3D11CreateQuery, reinterpret_cast<void**>(&phookD3D11CreateQuery)) != MH_OK) { return 1;}
		if (MH_EnableHook((DWORD_PTR*)pDeviceVTable[24]) != MH_OK) { return 1; }

		pDevice->Release();
		pContext->Release();
		pSwapChain->Release();

		return NULL;
	}
	DWORD __stdcall KeyHandler(LPVOID)
	{
		while (true)
		{
			if (GetAsyncKeyState(VK_INSERT)) {
				acikkapali = !acikkapali;
			}

			Sleep(100);
		}
		return 0;
	}

BOOL __stdcall DllMain(HINSTANCE hModule, DWORD dwReason, LPVOID lpReserved)
{ 
    if (dwReason == DLL_PROCESS_ATTACH)
    {
		memset(detourBuffer, 0, sizeof(detourBuffer) * sizeof(void*));
        CreateThread(NULL, 0, InitializeHook, NULL, 0, NULL);
		CreateThread(NULL, 0, KeyHandler, NULL, 0, NULL);
		
    }
    else if (dwReason == DLL_PROCESS_DETACH)
    {
		if (pFontWrapper)
		{
			pFontWrapper->Release();
		}
		
		for (int i = 0; i < sizeof(detourBuffer) / sizeof(void*); ++i)
		{
			if (detourBuffer[i])
			{
#ifdef _WIN64
				VirtualFree(detourBuffer[i], 0, MEM_RELEASE);
#else
				delete[] detourBuffer[i];
#endif
			}
		}
    }
	
    return TRUE; 
}