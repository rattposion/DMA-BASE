#include "global.h"
#include "imgui/backends/imgui_impl_win32.h"
#include "GUI/Menu/menu.h"

extern IMGUI_IMPL_API LRESULT ImGui_ImplWin32_WndProcHandler(HWND hWnd, UINT msg, WPARAM wParam, LPARAM lParam);
extern IMGUI_IMPL_API ImGuiKey ImGui_ImplWin32_KeyEventToImGuiKey(WPARAM wParam, LPARAM lParam);

namespace {
	bool s_prevKeyDown[256]{};
	DWORD s_keyRepeatTime[256]{};
	bool s_requestExit = false;

	bool IsPrintableVirtualKey(int vk) {
		return (vk >= '0' && vk <= '9')
			|| (vk >= 'A' && vk <= 'Z')
			|| (vk >= VK_NUMPAD0 && vk <= VK_NUMPAD9)
			|| vk == VK_OEM_PERIOD || vk == VK_OEM_COMMA || vk == VK_OEM_MINUS
			|| vk == VK_OEM_PLUS || vk == VK_DECIMAL;
	}

	bool IsNonPrintableVirtualKey(int vk) {
		return vk == VK_BACK || vk == VK_DELETE || vk == VK_RETURN || vk == VK_TAB || vk == VK_ESCAPE
			|| vk == VK_LEFT || vk == VK_RIGHT || vk == VK_UP || vk == VK_DOWN
			|| vk == VK_HOME || vk == VK_END || vk == VK_PRIOR || vk == VK_NEXT
			|| vk == VK_INSERT || vk == VK_SPACE;
	}

	HWND s_consoleWindow = nullptr;
	bool s_overlayHiddenForTaskSwitch = false;

	void EnsureConsoleTaskbarVisible() {
		s_consoleWindow = GetConsoleWindow();
		if (!s_consoleWindow) {
			return;
		}

		SetWindowTextA(s_consoleWindow, "Usermode Console");

		LONG_PTR exStyle = GetWindowLongPtr(s_consoleWindow, GWL_EXSTYLE);
		exStyle &= ~WS_EX_TOOLWINDOW;
		exStyle |= WS_EX_APPWINDOW;
		SetWindowLongPtr(s_consoleWindow, GWL_EXSTYLE, exStyle);

		ShowWindow(s_consoleWindow, SW_SHOW);
	}

	bool IsSystemWindowPickerOpen() {
		const wchar_t* pickerClasses[] = {
			L"MultitaskingViewFrame",
			L"Task Switching",
		};

		for (const wchar_t* windowClass : pickerClasses) {
			const HWND hwnd = FindWindowW(windowClass, nullptr);
			if (hwnd && IsWindowVisible(hwnd)) {
				return true;
			}
		}

		return false;
	}

	void ShowOverlayTopmost() {
		if (!Render::g_windowHandle) {
			return;
		}

		ShowWindow(Render::g_windowHandle, SW_SHOW);
		SetWindowPos(
			Render::g_windowHandle,
			HWND_TOPMOST,
			0, 0, 0, 0,
			SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_SHOWWINDOW
		);
		s_overlayHiddenForTaskSwitch = false;
	}

	void HideOverlayForTaskSwitch() {
		if (!Render::g_windowHandle || s_overlayHiddenForTaskSwitch) {
			return;
		}

		ShowWindow(Render::g_windowHandle, SW_HIDE);
		s_overlayHiddenForTaskSwitch = true;
	}

	void UpdateConsoleTaskSwitch() {
		const bool winDown =
			((GetAsyncKeyState(VK_LWIN) | GetAsyncKeyState(VK_RWIN)) & 0x8000) != 0;

		if (!s_consoleWindow) {
			s_consoleWindow = GetConsoleWindow();
		}

		const HWND foreground = GetForegroundWindow();
		const bool consoleFocused =
			s_consoleWindow && foreground == s_consoleWindow;
		const bool pickerOpen = IsSystemWindowPickerOpen();
		const bool shouldHideOverlay = winDown || pickerOpen || consoleFocused;

		if (shouldHideOverlay) {
			HideOverlayForTaskSwitch();
			if (s_consoleWindow) {
				ShowWindow(s_consoleWindow, SW_SHOW);
			}
		} else if (s_overlayHiddenForTaskSwitch) {
			ShowOverlayTopmost();
		}
	}

	bool ShouldPassthroughSystemKey(UINT msg, WPARAM wParam) {
		const int vk = static_cast<int>(wParam);
		if (vk == VK_LWIN || vk == VK_RWIN || vk == VK_APPS) {
			return true;
		}

		if (msg == WM_SYSKEYDOWN || msg == WM_SYSKEYUP) {
			return vk == VK_TAB || vk == VK_ESCAPE;
		}

		return false;
	}
}

static LRESULT CALLBACK OverlayWndProc(HWND hwnd, UINT msg, WPARAM wParam, LPARAM lParam) {
	if (ShouldPassthroughSystemKey(msg, wParam)) {
		return DefWindowProcA(hwnd, msg, wParam, lParam);
	}

	if (ImGui_ImplWin32_WndProcHandler(hwnd, msg, wParam, lParam)) {
		return true;
	}

	return DefWindowProcA(hwnd, msg, wParam, lParam);
}

int Render::g_screenWidth = 0;
int Render::g_screenHeight = 0;
int Render::g_halfScreenWidth = 0;
int Render::g_halfScreenHeight = 0;

HWND Render::g_windowHandle = nullptr;
ID3D11Device* Render::g_d3dDevice = nullptr;
IDXGISwapChain* Render::g_swapChain = nullptr;
ID3D11DeviceContext* Render::g_deviceContext = nullptr;
ID3D11RenderTargetView* Render::g_mainRenderTargetView = nullptr;

static ULONG DuplicateWinloginToken(ULONG sessionId, ULONG desiredAccess, PHANDLE tokenHandle) {
	PRIVILEGE_SET privilegeSet {
		.PrivilegeCount = 1,
		.Control = PRIVILEGE_SET_ALL_NECESSARY
	};

	if (!LookupPrivilegeValue(NULL, SE_TCB_NAME, &privilegeSet.Privilege[0].Luid)) {
		return GetLastError();
	}

	unique_handle snapshotHandle(CreateToolhelp32Snapshot(TH32CS_SNAPPROCESS, 0));
	if (snapshotHandle.get() == INVALID_HANDLE_VALUE) {
		return GetLastError();
	}

	PROCESSENTRY32 processEntry32 {
		.dwSize = sizeof(processEntry32)
	};

	for (bool continueToNext = Process32First(snapshotHandle.get(), &processEntry32); continueToNext; continueToNext = Process32Next(snapshotHandle.get(), &processEntry32)) {
		if (strcmp(processEntry32.szExeFile, ("winlogon.exe")) != 0) {
			continue;
		}

		unique_handle winlogonHandle(OpenProcess(
			PROCESS_QUERY_LIMITED_INFORMATION, 
			FALSE, 
			processEntry32.th32ProcessID));
		if (!winlogonHandle) {
			continue;
		}

		HANDLE winlogonTokenHandleRaw = INVALID_HANDLE_VALUE;
		if (!OpenProcessToken(
			winlogonHandle.get(),
			TOKEN_QUERY | TOKEN_DUPLICATE,
			&winlogonTokenHandleRaw)) {
			continue;
		}
		unique_handle winlogonTokenHandle(winlogonTokenHandleRaw);

		BOOL doesHavePrivilege = FALSE;
		if (!PrivilegeCheck(
			winlogonTokenHandle.get(),
			&privilegeSet, 
			&doesHavePrivilege) 
			|| !doesHavePrivilege) {
			continue;
		}

		ULONG sid = 0, returnLength = 0;
		if (!GetTokenInformation(
			winlogonTokenHandle.get(), 
			TokenSessionId,
			&sid, 
			sizeof(sid),
			&returnLength) 
			|| sid != sessionId) {
			continue;
		}

		if (DuplicateTokenEx(
			winlogonTokenHandle.get(),
			desiredAccess, 
			NULL,
			SecurityImpersonation, 
			TokenImpersonation,
			tokenHandle)) {
			return ERROR_SUCCESS;
		}

		return GetLastError();
	}

	return ERROR_NOT_FOUND;
}

static ULONG CreateUIAccessToken(PHANDLE tokenHandle) {
	HANDLE selfTokenHandleRaw = INVALID_HANDLE_VALUE;
	if (!OpenProcessToken(
		GetCurrentProcess(), 
		TOKEN_QUERY | TOKEN_DUPLICATE, 
		&selfTokenHandleRaw)) {
		return GetLastError();
	}
	unique_handle selfTokenHandle(selfTokenHandleRaw);

	ULONG sessionId = 0, returnLength = 0;
	if (!GetTokenInformation(
		selfTokenHandle.get(), 
		TokenSessionId, 
		&sessionId, 
		sizeof(sessionId), 
		&returnLength)) {
		return GetLastError();
	}

	HANDLE systemTokenRaw = INVALID_HANDLE_VALUE;
	ULONG error = DuplicateWinloginToken(
		sessionId, 
		TOKEN_IMPERSONATE, 
		&systemTokenRaw
	);
	if (error != ERROR_SUCCESS) {
		return error;
	}
	unique_handle systemToken(systemTokenRaw);

	if (!SetThreadToken(NULL, systemToken.get())) {
		return GetLastError();
	}

	if (!DuplicateTokenEx(
		selfTokenHandle.get(), 
		TOKEN_QUERY | TOKEN_DUPLICATE | 
		TOKEN_ASSIGN_PRIMARY | TOKEN_ADJUST_DEFAULT, 
		NULL, 
		SecurityAnonymous, TokenPrimary, 
		tokenHandle)) {
		error = GetLastError();
	} else {
		BOOL UIAccess = TRUE;
		if (!SetTokenInformation(
			*tokenHandle, 
			TokenUIAccess, 
			&UIAccess,
			sizeof(UIAccess))) {
			error = GetLastError();
			CloseHandle(*tokenHandle);
			*tokenHandle = NULL;
		}
	}

	RevertToSelf();
	return error;
}

static bool CheckUIAccess(PULONG error, PULONG uiAccess) {
	HANDLE tokenHandleRaw = INVALID_HANDLE_VALUE;
	if (!OpenProcessToken(
		GetCurrentProcess(),
		TOKEN_QUERY, 
		&tokenHandleRaw)) {
		*error = GetLastError();
		return false;
	}
	unique_handle tokenHandle(tokenHandleRaw);

	ULONG returnLength = 0;
	if (!GetTokenInformation(
		tokenHandle.get(),
		TokenUIAccess, 
		uiAccess, 
		sizeof(*uiAccess),
		&returnLength)) {
		*error = GetLastError();
		return false;
	}

	return true;
}

enum ZBID
{
	ZBID_DEFAULT = 0,
	ZBID_DESKTOP = 1,
	ZBID_UIACCESS = 2,
	ZBID_IMMERSIVE_IHM = 3,
	ZBID_IMMERSIVE_NOTIFICATION = 4,
	ZBID_IMMERSIVE_APPCHROME = 5,
	ZBID_IMMERSIVE_MOGO = 6,
	ZBID_IMMERSIVE_EDGY = 7,
	ZBID_IMMERSIVE_INACTIVEMOBODY = 8,
	ZBID_IMMERSIVE_INACTIVEDOCK = 9,
	ZBID_IMMERSIVE_ACTIVEMOBODY = 10,
	ZBID_IMMERSIVE_ACTIVEDOCK = 11,
	ZBID_IMMERSIVE_BACKGROUND = 12,
	ZBID_IMMERSIVE_SEARCH = 13,
	ZBID_GENUINE_WINDOWS = 14,
	ZBID_IMMERSIVE_RESTRICTED = 15,
	ZBID_SYSTEM_TOOLS = 16,
	// Win10
	ZBID_LOCK = 17,
	ZBID_ABOVELOCK_UX = 18,
};

typedef HWND(WINAPI* t_CreateWindowInBand)(
	_In_ DWORD dwExStyle, 
	_In_opt_ ATOM atom, 
	_In_opt_ LPCWSTR lpWindowName, 
	_In_ DWORD dwStyle, 
	_In_ int X, _In_ int Y, 
	_In_ int nWidth, _In_ int nHeight, 
	_In_opt_ HWND hWndParent, 
	_In_opt_ HMENU hMenu, 
	_In_opt_ HINSTANCE hInstance, 
	_In_opt_ LPVOID lpParam, 
	DWORD band
);

bool Render::Setup() {
	bool status = SetupWindow();
	if (!status) {
		Logger.log(
			LogLevel::ERROR, 
			("Could not create window handle!")
		);

		return false;
	}

	EnsureConsoleTaskbarVisible();

	status = InitDirectD3D();
	if (!status) {
		Logger.log(
			LogLevel::ERROR,
			("Could not initialise DirectD3D!")
		);

		return false;
	}

	
	SetThreadPriority(GetCurrentThread(), THREAD_PRIORITY_HIGHEST);

	IMGUI_CHECKVERSION();
	ImGui::CreateContext();
	ImGuiIO& io = ImGui::GetIO(); (void)io;
	io.IniFilename = nullptr;
	io.LogFilename = nullptr;

	ImGuiStyle& style = ImGui::GetStyle();
	ImGui::StyleColorsDark();

	style.WindowRounding	= 5;
	style.ChildRounding		= 2;
	style.FrameRounding		= 2;
	style.PopupRounding		= 3;
	style.GrabRounding		= 3;
	style.TabRounding		= 2;
	style.ScrollbarRounding = 2;
	style.ButtonTextAlign	= { 0.5f, 0.5f };
	style.WindowTitleAlign	= { 0.5f, 0.5f };
	style.ItemSpacing		= { 10.0f, 10.0f };
	style.WindowPadding		= { 10.0f, 10.0f };
	style.ItemInnerSpacing	= { 10.0f, 5.0f };
	style.FramePadding		= { 6.0f, 6.0f };
	style.WindowBorderSize	= 1;
	style.FrameBorderSize	= 1;
	style.ScrollbarSize		= 12.f;
	style.GrabMinSize		= 8.f;

    style.Colors[ImGuiCol_WindowBg]             = ImAdd::Hex2RGBA(0x0A0A14, 1.0f);
    style.Colors[ImGuiCol_ChildBg]              = ImAdd::Hex2RGBA(0x0F0F19, 1.0f);
    style.Colors[ImGuiCol_PopupBg]              = ImAdd::Hex2RGBA(0x0F0F19, 1.0f);
    style.Colors[ImGuiCol_MenuBarBg]            = ImAdd::Hex2RGBA(0x0F0F19, 1.0f);
    
    style.Colors[ImGuiCol_CheckMark]            = ImAdd::Hex2RGBA(0xC8C8D2, 1.0f);
    style.Colors[ImGuiCol_Text]                 = ImAdd::Hex2RGBA(0xC8C8D2, 1.0f);
    style.Colors[ImGuiCol_TextDisabled]         = ImAdd::Hex2RGBA(0x8C8C96, 1.0f);

    style.Colors[ImGuiCol_SliderGrabActive]     = ImAdd::Hex2RGBA(0x7099FF, 0.8f);
    style.Colors[ImGuiCol_ScrollbarGrabActive]  = ImAdd::Hex2RGBA(0x7099FF, 0.8f);
    style.Colors[ImGuiCol_ScrollbarGrabHovered] = ImAdd::Hex2RGBA(0x7099FF, 0.9f);

    style.Colors[ImGuiCol_Border]               = ImAdd::Hex2RGBA(0x141420, 1.0f);
    style.Colors[ImGuiCol_Separator]            = ImAdd::Hex2RGBA(0x141420, 1.0f);

    style.Colors[ImGuiCol_Button]               = ImAdd::Hex2RGBA(0x12121E, 1.0f);
    style.Colors[ImGuiCol_ButtonHovered]        = ImAdd::Hex2RGBA(0x12122D, 1.0f);
    style.Colors[ImGuiCol_ButtonActive]         = ImAdd::Hex2RGBA(0x0A0A14, 1.0f);

    style.Colors[ImGuiCol_FrameBg]              = ImAdd::Hex2RGBA(0x12121E, 1.0f);
    style.Colors[ImGuiCol_FrameBgHovered]       = ImAdd::Hex2RGBA(0x12122D, 1.0f);
    style.Colors[ImGuiCol_FrameBgActive]        = ImAdd::Hex2RGBA(0x0A0A14, 1.0f);

	status = ImGui_ImplWin32_Init(g_windowHandle);
	if (!status) {
		Logger.log(
			LogLevel::ERROR,
			("Could not initialise Imgui!")
		);

		return false;
	}

	status = ImGui_ImplDX11_Init(g_d3dDevice, g_deviceContext);
	if (!status) {
		Logger.log(
			LogLevel::ERROR,
			("Could not DX11 backend for ImGui!")
		);

		return false;
	}

	ImFont* pFont = io.Fonts->AddFontFromMemoryCompressedTTF(uiMuseoSansData, uiMuseoSansSize, 18, nullptr, io.Fonts->GetGlyphRangesDefault());

	// merge in icons from Font Awesome
	static const ImWchar icons_ranges[] = { ICON_MIN_FA, ICON_MAX_16_FA, 0 };
	static const ImWchar icons_ranges_brands[] = { ICON_MIN_FAB, ICON_MAX_16_FAB, 0 };
	ImFontConfig icons_config; icons_config.MergeMode = true; icons_config.PixelSnapH = true;

	float icon_size = 16;

	ImFont* FontAwesome = io.Fonts->AddFontFromMemoryCompressedTTF(fa6_solid_compressed_data, fa6_solid_compressed_size, icon_size, &icons_config, icons_ranges);
	ImFont* FontAwesomeBrands = io.Fonts->AddFontFromMemoryCompressedTTF(fa_brands_400_compressed_data, fa_brands_400_compressed_size, icon_size, &icons_config, icons_ranges_brands);

	io.Fonts->Build();

	SetWindowDisplayAffinity(g_windowHandle, WDA_EXCLUDEFROMCAPTURE);

	return true;
}

void Render::PollOverlayKeyboardKeys() {
	if (!Menu::IsOpen()) {
		return;
	}

	ImGuiIO& io = ImGui::GetIO();

	io.AddKeyEvent(ImGuiMod_Ctrl, (GetAsyncKeyState(VK_CONTROL) & 0x8000) != 0);
	io.AddKeyEvent(ImGuiMod_Shift, (GetAsyncKeyState(VK_SHIFT) & 0x8000) != 0);
	io.AddKeyEvent(ImGuiMod_Alt, (GetAsyncKeyState(VK_MENU) & 0x8000) != 0);

	const bool textInputActive = io.WantTextInput;

	for (int vk = 0; vk < 256; ++vk) {
		if (vk == VK_LBUTTON || vk == VK_RBUTTON || vk == VK_MBUTTON
			|| vk == VK_LWIN || vk == VK_RWIN || vk == VK_APPS) {
			continue;
		}

		const bool down = (GetAsyncKeyState(vk) & 0x8000) != 0;
		const LPARAM lParam = static_cast<LPARAM>(MapVirtualKey(vk, MAPVK_VK_TO_VSC) << 16);
		const ImGuiKey key = ImGui_ImplWin32_KeyEventToImGuiKey(vk, lParam);
		if (key == ImGuiKey_None) {
			continue;
		}

		// Text fields get characters via PollOverlayTextInput; hotkeys still need raw key events.
		if (textInputActive && IsPrintableVirtualKey(vk) && !IsNonPrintableVirtualKey(vk)) {
			continue;
		}

		io.AddKeyEvent(key, down);
	}
}

void Render::PollOverlayTextInput() {
	if (!Menu::IsOpen() || !ImGui::GetIO().WantTextInput) {
		return;
	}

	ImGuiIO& io = ImGui::GetIO();
	BYTE keyboardState[256] = { };
	GetKeyboardState(keyboardState);

	const DWORD now = GetTickCount();

	for (int vk = 0; vk < 256; ++vk) {
		if (vk == VK_LBUTTON || vk == VK_RBUTTON || vk == VK_MBUTTON) {
			continue;
		}

		const bool down = (keyboardState[vk] & 0x80) != 0;
		if (!down) {
			s_prevKeyDown[vk] = false;
			continue;
		}

		const bool newPress = !s_prevKeyDown[vk];
		const bool repeat = s_prevKeyDown[vk] && (now - s_keyRepeatTime[vk] >= 33);
		if (!newPress && !repeat) {
			s_prevKeyDown[vk] = true;
			continue;
		}

		WCHAR chars[4] = { };
		const UINT scancode = MapVirtualKey(vk, MAPVK_VK_TO_VSC);
		const int count = ToUnicode(vk, scancode, keyboardState, chars, 4, 0);
		for (int i = 0; i < count; ++i) {
			if (chars[i] > 0 && chars[i] < 0x10000) {
				io.InputQueueCharacters.push_back(static_cast<ImWchar>(chars[i]));
			}
		}

		if (count > 0) {
			s_keyRepeatTime[vk] = now;
		}

		s_prevKeyDown[vk] = true;
	}
}

void Render::RequestExit() {
	s_requestExit = true;
}

void Render::MainLoop() {
	MSG msg { };
	ZeroMemory(&msg, sizeof(msg));

	ImGuiIO& io = ImGui::GetIO(); (void)io;
	while (!s_requestExit && msg.message != WM_QUIT) {
		while (PeekMessageA(&msg, nullptr, 0, 0, PM_REMOVE)) {
			if (msg.message == WM_QUIT) {
				break;
			}

			TranslateMessage(&msg);
			DispatchMessageA(&msg);
		}

		if (s_requestExit || msg.message == WM_QUIT) {
			break;
		}

		UpdateConsoleTaskSwitch();
		if (s_overlayHiddenForTaskSwitch) {
			Sleep(10);
			continue;
		}

		POINT cursorPosition { };
		if (GetCursorPos(&cursorPosition)) {
			io.MousePos.x = static_cast<float>(cursorPosition.x);
			io.MousePos.y = static_cast<float>(cursorPosition.y);
		}

		Menu::PrepareOverlayInput();

		if (Menu::IsOpen() || (Config::Settings::debug && DebugOverlay::WantsMouseCapture())) {
			io.MouseDown[0] = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;
			io.MouseDown[1] = (GetAsyncKeyState(VK_RBUTTON) & 0x8000) != 0;
			io.MouseDown[2] = (GetAsyncKeyState(VK_MBUTTON) & 0x8000) != 0;
		} else if (GetAsyncKeyState(VK_LBUTTON) & 0x8000) {
			io.MouseDown[0] = true;
			io.MouseClicked[0] = true;
			io.MouseClickedPos[0].x = io.MousePos.x;
			io.MouseClickedPos[0].y = io.MousePos.y;
		} else {
			io.MouseDown[0] = false;
		}

		Menu::RenderMenu();
	}
	
	Cache::Stop();
	Config::SaveSettings();

	ImGui_ImplDX11_Shutdown();
	ImGui_ImplWin32_Shutdown();
	ImGui::DestroyContext();

	ShutDownDirectD3D();
	DestroyWindow(g_windowHandle);
}

bool Render::SetupWindow() {
	g_screenWidth = GetSystemMetrics(SM_CXSCREEN); g_halfScreenWidth = g_screenWidth / 2;
	g_screenHeight = GetSystemMetrics(SM_CYSCREEN); g_halfScreenHeight = g_screenHeight / 2;

	auto hr = CoInitializeEx(
		NULL, 
		COINIT_APARTMENTTHREADED | COINIT_DISABLE_OLE1DDE
	);

	if (!SUCCEEDED(hr)) {
		Logger.log(
			LogLevel::ERROR,
			("CoInitializeEx(...) failed with status: {:#x}"),
			hr
		);

		return false;
	}

	auto error = PrepareForUIAccess();
	if (error != ERROR_SUCCESS) {
		Logger.log(
			LogLevel::ERROR, 
			("Could Not Gain UI Access, make sure you are running as admin! Error: {:#x}"),
			error
		);

		return false;
	}

	const auto user32DLL = LoadLibraryA(("user32.dll"));
	if (!user32DLL) {
		error = GetLastError();

		Logger.log(
			LogLevel::ERROR,
			("Failed to load \"user32.dll\"! Error: {:#x}"),
			error
		);

		return false;
	}

	auto createWindowInBandAddress = GetProcAddress(user32DLL, ("CreateWindowInBand"));
	if (!createWindowInBandAddress) {
		error = GetLastError();

		Logger.log(
			LogLevel::ERROR,
			("Failed to find function \"CreateWindowInBand(...)\"! Error: {:#x}"),
			error
		);

		return false;
	}

	auto CreateWindowInBand = reinterpret_cast<t_CreateWindowInBand>(createWindowInBandAddress);

	WNDCLASSEX windowClass = {
		.cbSize			= sizeof(windowClass),
		.style			= 0,
		.lpfnWndProc	= OverlayWndProc,
		.cbClsExtra		= 0,
		.cbWndExtra		= 0,
		.hInstance		= nullptr,
		.hIcon			= nullptr,
		.hCursor		= nullptr,
		.hbrBackground	= nullptr,
		.lpszMenuName	= nullptr,
		.lpszClassName	= ("Microsoft Recall"),
		.hIconSm		= nullptr
	};

	auto res = RegisterClassEx(&windowClass);
	g_windowHandle = CreateWindowInBand(
		WS_EX_TOPMOST | WS_EX_NOACTIVATE | WS_EX_LAYERED,
		res,
		(L"Microsoft Recall ParentWind"),
		0x80000000,
		0, 0, 
		g_screenWidth, 
		g_screenHeight,
		NULL,
		NULL,
		windowClass.hInstance,
		LPVOID(res),
		ZBID_UIACCESS
	);

	/*g_windowHandle = CreateWindowExA(
		WS_EX_TOPMOST | WS_EX_TRANSPARENT | WS_EX_TOOLWINDOW | WS_EX_LAYERED,
		windowClass.lpszClassName,
		"Testing",
		WS_POPUP,
		0, 0,
		g_screenWidth, g_screenHeight,
		NULL, NULL,
		windowClass.hInstance,
		NULL
	);*/

	if (!g_windowHandle) {
		return false;
	}

	HWND hwnd = Render::g_windowHandle;

	MARGINS margins = { -1 };
	DwmExtendFrameIntoClientArea(g_windowHandle, &margins);
	SetWindowPos(g_windowHandle, HWND_TOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE);
	SetLayeredWindowAttributes(hwnd, 25, RGB(0, 0, 0), LWA_COLORKEY);
	ShowWindow(g_windowHandle, SW_SHOW);
	UpdateWindow(g_windowHandle);

	return true;
}

bool Render::InitDirectD3D() {
	DXGI_SWAP_CHAIN_DESC swapChainDescription {
		.BufferDesc {
			.Width = 0,
			.Height = 0,
			.RefreshRate {
				.Numerator = 0,
				.Denominator = 0
			},
			.Format = DXGI_FORMAT_B8G8R8A8_UNORM
		},
		.SampleDesc {
			.Count = 1,
			.Quality = 0
		},
		.BufferUsage = DXGI_USAGE_RENDER_TARGET_OUTPUT,
		.BufferCount = 2,
		.OutputWindow = g_windowHandle,
		.Windowed = TRUE,
		.SwapEffect = DXGI_SWAP_EFFECT_DISCARD,
		.Flags = DXGI_SWAP_CHAIN_FLAG_ALLOW_MODE_SWITCH
	};

	UINT createDeviceFlags = 0;
	D3D_FEATURE_LEVEL featureLevel { };
	const D3D_FEATURE_LEVEL featureLevelArray[2] = { D3D_FEATURE_LEVEL_11_0, D3D_FEATURE_LEVEL_10_0, };

	HRESULT res = D3D11CreateDeviceAndSwapChain(
		nullptr,
		D3D_DRIVER_TYPE_HARDWARE,
		nullptr,
		createDeviceFlags,
		featureLevelArray,
		2,
		D3D11_SDK_VERSION,
		&swapChainDescription,
		&g_swapChain,
		&g_d3dDevice,
		&featureLevel,
		&g_deviceContext
	);

	if (res == DXGI_ERROR_UNSUPPORTED) {
		res = D3D11CreateDeviceAndSwapChain(
			nullptr,
			D3D_DRIVER_TYPE_WARP,
			nullptr,
			createDeviceFlags,
			featureLevelArray,
			2,
			D3D11_SDK_VERSION,
			&swapChainDescription,
			&g_swapChain,
			&g_d3dDevice,
			&featureLevel,
			&g_deviceContext
		);

		if (res != S_OK) {
			Logger.log(
				LogLevel::ERROR, 
				("Could not create D3D11 Device and Swap Chain! Error: {:#x}"),
				res
			);

			return false;
		}
	}

	IDXGISwapChain1* pSwapChain1 = nullptr;
	HRESULT hr = g_swapChain->QueryInterface(__uuidof(IDXGISwapChain1), (void**)&pSwapChain1);
	if (SUCCEEDED(hr)) {
		DXGI_RGBA color = { 0.0f, 0.0f, 0.0f, 0.0f };
		pSwapChain1->SetBackgroundColor(&color);
		pSwapChain1->Release();
	} else {
		Logger.log(
			LogLevel::WARNING,
			("Could not get IDXGISwapChain1 interface. Transparency may not work.")
		);
	}

	ID3D11Texture2D* backBuffer = nullptr;
	hr = g_swapChain->GetBuffer(0, IID_PPV_ARGS(&backBuffer));
	if (!SUCCEEDED(hr)) {
		Logger.log(
			LogLevel::ERROR, 
			("Failed to get back buffer! Status: {:#x}"),
			hr
		);

		return false;
	}

	hr = g_d3dDevice->CreateRenderTargetView(backBuffer, nullptr, &g_mainRenderTargetView);
	backBuffer->Release();
	if (!SUCCEEDED(hr)) {
		Logger.log(
			LogLevel::ERROR,
			("Failed to create main render target view! Status: {:#x}"),
			hr
		);

		return false;
	}
	

	return true;
}

bool Render::ShutDownDirectD3D() {
	if (g_mainRenderTargetView) { 
		g_mainRenderTargetView->Release(); 
		g_mainRenderTargetView = nullptr;
	}

	if (g_swapChain) { 
		g_swapChain->Release(); 
		g_swapChain = nullptr; 
	}

	if (g_deviceContext) { 
		g_deviceContext->Release(); 
		g_deviceContext = nullptr; 
	}

	if (g_d3dDevice) {
		g_d3dDevice->Release(); 
		g_d3dDevice = nullptr;
	}

	return true;
}

void Render::PresentImGuiFrame() {
	const float clearColorWithAlpha[4] = {
		0.f * 0.f,
		0.f * 0.f,
		0.f * 0.f,
		0.f
	};

	ImGui::Render();
	Render::g_deviceContext->OMSetRenderTargets(1, &Render::g_mainRenderTargetView, nullptr);
	Render::g_deviceContext->ClearRenderTargetView(Render::g_mainRenderTargetView, clearColorWithAlpha);
	ImGui_ImplDX11_RenderDrawData(ImGui::GetDrawData());
	Render::g_swapChain->Present(0, 0);
}

ULONG Render::PrepareForUIAccess() {
	ULONG error = 0;
	BOOL hasUIAccess = 0;

	if (!CheckUIAccess(&error, (ULONG*)&hasUIAccess)) {
		return error;
	}

	if (hasUIAccess) {
		return ERROR_SUCCESS;
	}

	HANDLE tokenUIAccessHandleRaw = INVALID_HANDLE_VALUE;
	error = CreateUIAccessToken(&tokenUIAccessHandleRaw);
	if (error != ERROR_SUCCESS) {
		return error;
	}
	unique_handle tokenUIAccessHandle(tokenUIAccessHandleRaw);

	STARTUPINFO startupInfo { };
	PROCESS_INFORMATION processInformation { };
	GetStartupInfo(&startupInfo);

	if (!CreateProcessAsUser(
		tokenUIAccessHandle.get(),
		NULL,
		GetCommandLine(),
		NULL, NULL,
		FALSE,
		0, NULL, NULL,
		&startupInfo,
		&processInformation)
		) {
		return GetLastError();
	}

	CloseHandle(processInformation.hProcess);
	CloseHandle(processInformation.hThread);
	ExitProcess(0);

	return ERROR_SUCCESS;
}
