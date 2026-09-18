#pragma once

namespace Render {
	extern HWND g_windowHandle;

	extern ID3D11Device* g_d3dDevice;
	extern IDXGISwapChain* g_swapChain;
	extern ID3D11DeviceContext* g_deviceContext;
	extern ID3D11RenderTargetView* g_mainRenderTargetView;

	extern int g_screenWidth, g_screenHeight;
	extern int g_halfScreenWidth, g_halfScreenHeight;
	
	bool Setup();
	void MainLoop();
	
	bool SetupWindow();
	bool InitDirectD3D();
	bool ShutDownDirectD3D();
	void PresentImGuiFrame();
	void PollOverlayKeyboardKeys();
	void PollOverlayTextInput();
	void RequestExit();

	ULONG PrepareForUIAccess();
}