#include "global.h"
#include "KmBox/KmboxNetApi.h"
#include <makcu/makcu.h>
#include <setupapi.h>
#include <devguid.h>
#include <iostream>
#include <algorithm>
#include <cstdio>

#pragma comment(lib, "setupapi.lib")

namespace {
	enum class ConnectionType {
		NONE,
		SERIAL,
		NET,
		MAKCU
	};

	bool kmboxInitialized = false;
	ConnectionType connectionType = ConnectionType::NONE;
	HANDLE hSerial = INVALID_HANDLE_VALUE;

	makcu::Device g_makcuDevice;
	bool g_makcuInitialized = false;

	const char* ResolveConfigString(const char* value, const char* fallback) {
		return (value && value[0] != '\0') ? value : fallback;
	}

	int ResolveConfigPort(const char* portValue) {
		if (!portValue || portValue[0] == '\0') {
			return 0;
		}

		return std::atoi(portValue);
	}

	bool ConnectMakcuOnPort(int comPort) {
		if (g_makcuDevice.isConnected()) {
			g_makcuDevice.disconnect();
		}

		if (comPort > 0) {
			const std::string portName = "COM" + std::to_string(comPort);
			std::cout << "[makcu] connecting on " << portName << "..." << std::endl;
			if (!g_makcuDevice.connect(portName)) {
				std::cout << "[makcu] connect failed on " << portName << ", trying auto..." << std::endl;
			} else {
				std::cout << "[makcu] connected on " << portName << std::endl;
				g_makcuDevice.enableHighPerformanceMode(true);
				g_makcuDevice.setBaudRate(4000000, false);
				g_makcuDevice.enableButtonMonitoring(true);
				return true;
			}
		}

		std::cout << "[makcu] auto-detecting device..." << std::endl;
		if (!g_makcuDevice.connect()) {
			std::cout << "[makcu] auto-connect failed" << std::endl;
			return false;
		}

		std::cout << "[makcu] connected (auto-detect)" << std::endl;
		g_makcuDevice.enableHighPerformanceMode(true);
		g_makcuDevice.setBaudRate(4000000, false);
		g_makcuDevice.enableButtonMonitoring(true);
		return true;
	}
}

namespace Kmbox {

bool IsNetDevice() {
	return kmboxInitialized && connectionType == ConnectionType::NET;
}

bool IsMakcuDevice() {
	return connectionType == ConnectionType::MAKCU
		&& g_makcuInitialized
		&& g_makcuDevice.isConnected();
}

bool Initialize(const char* ip, const char* port, const char* mac) {
	if (kmboxInitialized) {
		if (Config::Aim::kmboxDevice == 2) {
			if (IsMakcuDevice()) {
				return true;
			}
			kmboxInitialized = false;
			connectionType = ConnectionType::NONE;
			g_makcuInitialized = false;
		} else {
			return true;
		}
	}

	if (!Config::Aim::useKmbox) {
		return false;
	}

	if (Config::Aim::kmboxDevice == 2) {
		if (!ConnectMakcuOnPort(Config::Aim::kmboxComPort)) {
			g_makcuInitialized = false;
			connectionType = ConnectionType::NONE;
			return false;
		}

		g_makcuInitialized = true;
		connectionType = ConnectionType::MAKCU;
		kmboxInitialized = true;
		return true;
	}

	if (Config::Aim::kmboxDevice == 0) {
		const char* connectIp = ResolveConfigString(ip, Config::Aim::kmboxIp);
		const char* connectPort = ResolveConfigString(port, Config::Aim::kmboxPort);
		const char* connectMac = ResolveConfigString(mac, Config::Aim::kmboxMac);
		const int connectPortValue = ResolveConfigPort(connectPort);

		if (connectIp[0] != '\0' && connectPortValue > 0 && connectMac[0] != '\0') {
			if (KMBOXNET::ConnectKMBox(connectMac, connectIp, connectPortValue)) {
				if (kmNet_monitor(static_cast<short>(Config::Aim::kmboxMonitorPort)) == 0) {
					connectionType = ConnectionType::NET;
					kmboxInitialized = true;
					std::cout << "KmboxNet connected at " << connectIp << ":" << connectPort
						<< " (MAC " << connectMac << ")" << std::endl;
					std::cout << "KmboxNet monitor enabled on port " << Config::Aim::kmboxMonitorPort << std::endl;
					return true;
				}

				std::cout << "KmboxNet connected but monitor failed. Aim key reads may not work." << std::endl;
				connectionType = ConnectionType::NET;
				kmboxInitialized = true;
				return true;
			}

			std::cout << "KmboxNet connection failed for " << connectIp << ":" << connectPort << std::endl;
		}

		return false;
	}

	if (Config::Aim::kmboxDevice == 1) {
		const std::string portName = FindDevicePort();
		if (!portName.empty()) {
			hSerial = CreateFileA(portName.c_str(), GENERIC_READ | GENERIC_WRITE, 0, 0, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, 0);
			if (hSerial != INVALID_HANDLE_VALUE) {
				DCB dcbSerialParams { 0 };
				dcbSerialParams.DCBlength = sizeof(dcbSerialParams);

				if (GetCommState(hSerial, &dcbSerialParams)) {
					dcbSerialParams.BaudRate = 115200;
					dcbSerialParams.ByteSize = 8;
					dcbSerialParams.StopBits = ONESTOPBIT;
					dcbSerialParams.Parity = NOPARITY;

					if (SetCommState(hSerial, &dcbSerialParams)) {
						COMMTIMEOUTS timeouts { 0 };
						timeouts.ReadIntervalTimeout = 0xFFFFFFFF;
						timeouts.ReadTotalTimeoutMultiplier = 0;
						timeouts.ReadTotalTimeoutConstant = 0;
						timeouts.WriteTotalTimeoutMultiplier = 0;
						timeouts.WriteTotalTimeoutConstant = 2000;

						if (SetCommTimeouts(hSerial, &timeouts)) {
							connectionType = ConnectionType::SERIAL;
							kmboxInitialized = true;
							std::cout << "Kmbox serial connection established on " << portName << std::endl;
							return true;
						}
					}
				}

				CloseHandle(hSerial);
				hSerial = INVALID_HANDLE_VALUE;
			}
		}
	}

	return false;
}

bool MoveMouse(int x, int y, MovementType type, int time_ms) {
	if (connectionType == ConnectionType::MAKCU && g_makcuInitialized && g_makcuDevice.isConnected()) {
		kmboxInitialized = true;

		const int32_t ix = static_cast<int32_t>((std::clamp)(x, -32767, 32767));
		const int32_t iy = static_cast<int32_t>((std::clamp)(y, -32767, 32767));
		if (ix == 0 && iy == 0) {
			return true;
		}

		switch (type) {
		case MovementType::AUTO: {
			const uint32_t segs = static_cast<uint32_t>((time_ms > 1) ? (std::max)(time_ms / 6, 8) : 8);
			return g_makcuDevice.mouseMoveSmooth(ix, iy, segs);
		}
		case MovementType::BEZIER: {
			const uint32_t segs = static_cast<uint32_t>((time_ms > 1) ? (std::max)(time_ms / 6, 8) : 8);
			return g_makcuDevice.mouseMoveBezier(ix, iy, segs, 0, 0);
		}
		case MovementType::DIRECT:
		default:
			return g_makcuDevice.mouseMove(ix, iy);
		}
	}

	if (!kmboxInitialized) {
		return false;
	}

	if (connectionType == ConnectionType::NET) {
		const auto sx = static_cast<short>((std::clamp)(x, -32767, 32767));
		const auto sy = static_cast<short>((std::clamp)(y, -32767, 32767));
		switch (type) {
		case MovementType::DIRECT:
			return kmNet_mouse_move(sx, sy) == 0;
		case MovementType::AUTO:
			return kmNet_mouse_move_auto(sx, sy, time_ms) == 0;
		case MovementType::BEZIER:
			return kmNet_mouse_move_beizer(sx, sy, time_ms, 0, 0, 0, 0) == 0;
		}
	}

	if (connectionType != ConnectionType::SERIAL || hSerial == INVALID_HANDLE_VALUE) {
		return false;
	}

	std::string command;
	switch (type) {
	case MovementType::DIRECT:
		command = "km.move(" + std::to_string(x) + ", " + std::to_string(y) + ", 1)\r\n";
		break;
	case MovementType::AUTO:
	case MovementType::BEZIER:
		command = "km.move(" + std::to_string(x) + ", " + std::to_string(y) + ", " + std::to_string(time_ms / 10) + ")\r\n";
		break;
	}

	DWORD bytesWritten = 0;
	return WriteFile(hSerial, command.c_str(), static_cast<DWORD>(command.length()), &bytesWritten, NULL) == TRUE;
}

bool ReadKeyState(int virtualKey) {
	if (connectionType == ConnectionType::MAKCU && g_makcuInitialized && g_makcuDevice.isConnected()) {
		const uint8_t mask = g_makcuDevice.getButtonMask();
		auto masked = [&](makcu::MouseButton btn) {
			return (mask & (1u << static_cast<uint8_t>(btn))) != 0;
		};

		switch (virtualKey) {
		case VK_LBUTTON:
			return masked(makcu::MouseButton::LEFT);
		case VK_RBUTTON:
			return masked(makcu::MouseButton::RIGHT);
		case VK_MBUTTON:
			return masked(makcu::MouseButton::MIDDLE);
		case VK_XBUTTON1:
			return masked(makcu::MouseButton::SIDE1);
		case VK_XBUTTON2:
			return masked(makcu::MouseButton::SIDE2);
		default:
			return (GetAsyncKeyState(virtualKey) & 0x8000) != 0;
		}
	}

	if (!kmboxInitialized) {
		return false;
	}

	if (connectionType == ConnectionType::NET) {
		switch (virtualKey) {
		case VK_LBUTTON:
			return kmNet_monitor_mouse_left() == 1;
		case VK_RBUTTON:
			return kmNet_monitor_mouse_right() == 1;
		case VK_MBUTTON:
			return kmNet_monitor_mouse_middle() == 1;
		default:
			return kmNet_monitor_keyboard(static_cast<short>(virtualKey)) == 1;
		}
	}

	if (connectionType != ConnectionType::SERIAL || hSerial == INVALID_HANDLE_VALUE) {
		return false;
	}

	std::string command = "READ_KEY " + std::to_string(virtualKey) + "\n";
	DWORD bytesWritten = 0;
	if (!WriteFile(hSerial, command.c_str(), static_cast<DWORD>(command.length()), &bytesWritten, NULL)) {
		return false;
	}

	char buffer[256] {};
	DWORD bytesRead = 0;
	if (ReadFile(hSerial, buffer, sizeof(buffer) - 1, &bytesRead, NULL) && bytesRead > 0) {
		return std::string(buffer).find("KEY_PRESSED") != std::string::npos;
	}

	return false;
}

bool TestMakcuMovement() {
	if (!IsMakcuDevice()) {
		std::cout << "[makcu] test movement skipped: not connected" << std::endl;
		return false;
	}

	const bool movedOut = g_makcuDevice.mouseMove(25, 25);
	Sleep(50);
	const bool movedBack = g_makcuDevice.mouseMove(-25, -25);
	const bool ok = movedOut && movedBack;

	if (!ok) {
		std::cout << "[makcu] test movement failed" << std::endl;
	} else {
		std::cout << "[makcu] test move sent" << std::endl;
	}

	return ok;
}

bool IsDeviceConnected() {
	if (connectionType == ConnectionType::MAKCU) {
		return IsMakcuDevice();
	}

	return kmboxInitialized;
}

std::string FindDevicePort() {
	HDEVINFO hDevInfo = SetupDiGetClassDevsA(&GUID_DEVCLASS_PORTS, 0, 0, DIGCF_PRESENT);
	if (hDevInfo == INVALID_HANDLE_VALUE) {
		return "";
	}

	SP_DEVINFO_DATA deviceInfoData {};
	deviceInfoData.cbSize = sizeof(SP_DEVINFO_DATA);

	for (DWORD i = 0; SetupDiEnumDeviceInfo(hDevInfo, i, &deviceInfoData); ++i) {
		char buf[512] {};
		DWORD nSize = 0;

		if (SetupDiGetDeviceRegistryPropertyA(
			hDevInfo,
			&deviceInfoData,
			SPDRP_FRIENDLYNAME,
			NULL,
			reinterpret_cast<PBYTE>(buf),
			sizeof(buf),
			&nSize) && nSize > 0)
		{
			const std::string deviceDescription = buf;
			if (deviceDescription.find("USB-SERIAL CH340") != std::string::npos
				|| deviceDescription.find("USB-Enhanced-SERIAL CH343") != std::string::npos
				|| deviceDescription.find("Arduino") != std::string::npos
				|| deviceDescription.find("Kmbox") != std::string::npos)
			{
				const size_t comPos = deviceDescription.find("COM");
				const size_t endPos = deviceDescription.find(")", comPos);
				if (comPos != std::string::npos && endPos != std::string::npos) {
					SetupDiDestroyDeviceInfoList(hDevInfo);
					return deviceDescription.substr(comPos, endPos - comPos);
				}
			}
		}
	}

	SetupDiDestroyDeviceInfoList(hDevInfo);
	return "";
}

void Cleanup() {
	if (connectionType == ConnectionType::NET) {
		kmNet_monitor(0);
	}

	if (connectionType == ConnectionType::MAKCU || g_makcuInitialized) {
		if (g_makcuDevice.isConnected()) {
			g_makcuDevice.disconnect();
		}
		g_makcuInitialized = false;
	}

	if (hSerial != INVALID_HANDLE_VALUE) {
		CloseHandle(hSerial);
		hSerial = INVALID_HANDLE_VALUE;
	}

	connectionType = ConnectionType::NONE;
	kmboxInitialized = false;
}

}
