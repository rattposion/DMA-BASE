#pragma once

#include <string>

namespace SDK {
	struct Vec2;
}

namespace Kmbox {
	enum class MovementType {
		DIRECT,
		AUTO,
		BEZIER
	};

	bool Initialize(const char* ip = nullptr, const char* port = nullptr, const char* mac = nullptr);
	bool IsNetDevice();
	bool IsMakcuDevice();
	bool IsDeviceConnected();
	bool MoveMouse(int x, int y, MovementType type = MovementType::AUTO, int time_ms = 50);
	bool ReadKeyState(int virtualKey);
	std::string FindDevicePort();
	bool TestMakcuMovement();
	void Cleanup();
}

namespace Aimbot {
	void Initialize();
	void Reconnect();
	void Cleanup();
	bool ReadAimKeyHeld(int virtualKey);
	void AimTo(const SDK::Vec2& position);
}
