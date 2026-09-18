#pragma once
#define NOMINMAX
#include <Windows.h>
#include <tlhelp32.h>

#include <print>
#include <format>
#include <fstream>
#include <iostream>

#include <string>
#include <string_view>

#include <chrono>
#include <mutex>
#include <thread>

#include <queue>
#include <array>
#include <span>

#include <cmath>
#include <random>
#include <limits>

#include <expected>
#include <functional>

#include <tchar.h>
#include <Dwmapi.h>

#include <imgui.h>
#include <imgui_impl_win32.h>
#include <imgui_impl_dx11.h>

#include <ImAdd.h>
#include <d3d11.h>
#include <dxgi1_2.h>

#pragma comment(lib, "winmm.lib")

struct HandleRAII {
	void operator()(HANDLE handle) const {
		if (handle != NULL && handle != INVALID_HANDLE_VALUE) {
			CloseHandle(handle);
		}
	}
}; using unique_handle = std::unique_ptr<void, HandleRAII>;

namespace globals {
	inline ULONG64 g_peb = 0;
	inline ULONG64 g_baseAddress = 0;
}

#include "Log/log.h"

#include "Config/config.h"

#include "GUI/Menu/Resource/Font/MuseoSans.h"
#include "GUI/Menu/Resource/Font/IconsFontAwesome6.h"
#include "GUI/Menu/Resource/Font/IconsFontAwesome6Brands.h"



#include "GUI/Menu/menu.h"
#include "GUI/Render/render.h"

#include "DMAInterface.h"

#include "Game/SDK/SDK.h"
#include "Game/SDK/Engine/structs.h"
#include "Game/SDK/Engine/offsets.h"
#include "Game/SDK/Engine/engine.h"

#include "Game/ESP/ESP.h"
#include "Game/ESP/DebugOverlay.h"
#include "Game/Aimbot/Aimbot.h"

#include "Game/cache.h"
#include "Game/loop.h"

