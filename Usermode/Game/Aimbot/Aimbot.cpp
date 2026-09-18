#include "global.h"
#include "Game/Aimbot/Aimbot.h"

namespace {
	bool aimbotInitialized = false;
	float moveCarryX = 0.f;
	float moveCarryY = 0.f;

	void MoveMouseDirect(const float x, const float y) {
		moveCarryX += x;
		moveCarryY += y;

		const int moveX = static_cast<int>(moveCarryX);
		const int moveY = static_cast<int>(moveCarryY);
		moveCarryX -= static_cast<float>(moveX);
		moveCarryY -= static_cast<float>(moveY);

		if (moveX == 0 && moveY == 0) {
			return;
		}

		if (Config::Aim::useKmbox && Kmbox::IsDeviceConnected()) {
			const int clampedX = (std::clamp)(moveX, -32767, 32767);
			const int clampedY = (std::clamp)(moveY, -32767, 32767);

			// MAKCU always uses DIRECT mouseMove (smooth/bezier stalls aim).
			if (Kmbox::IsMakcuDevice()) {
				Kmbox::MoveMouse(clampedX, clampedY, Kmbox::MovementType::DIRECT, 1);
				return;
			}

			const auto moveType = static_cast<Kmbox::MovementType>(Config::Aim::movementType);
			Kmbox::MoveMouse(
				clampedX,
				clampedY,
				moveType,
				Config::Aim::movementTime
			);
			return;
		}

		INPUT input {
			.type = INPUT_MOUSE,
			.mi = {
				.dx = static_cast<LONG>(moveX),
				.dy = static_cast<LONG>(moveY),
				.mouseData = 0,
				.dwFlags = MOUSEEVENTF_MOVE | MOUSEEVENTF_VIRTUALDESK,
				.time = 0
			}
		};

		SendInput(1, &input, sizeof(input));
	}
}

namespace Aimbot {

bool ReadAimKeyHeld(int virtualKey) {
	if (Config::Aim::useKmbox && Kmbox::IsDeviceConnected()) {
		static int cachedVirtualKey = 0;
		static bool cachedHeld = false;
		static DWORD lastPollMs = 0;
		const DWORD nowMs = GetTickCount();

		if (cachedVirtualKey != virtualKey || (nowMs - lastPollMs) >= 8) {
			cachedHeld = Kmbox::ReadKeyState(virtualKey);
			cachedVirtualKey = virtualKey;
			lastPollMs = nowMs;
		}

		if (cachedHeld) {
			return true;
		}

		// On DMA setups, mouse buttons only exist on the game PC via the hardware device.
		if (Kmbox::IsMakcuDevice() || Kmbox::IsNetDevice()) {
			switch (virtualKey) {
			case VK_LBUTTON:
			case VK_RBUTTON:
			case VK_MBUTTON:
			case VK_XBUTTON1:
			case VK_XBUTTON2:
				return false;
			}
		}
	}

	return (GetAsyncKeyState(virtualKey) & 0x8000) != 0;
}

void Reconnect() {
	Cleanup();
	Initialize();
}

void Initialize() {
	if (aimbotInitialized) {
		return;
	}

	if (!Config::Aim::useKmbox) {
		aimbotInitialized = true;
		return;
	}

	if (Kmbox::Initialize(Config::Aim::kmboxIp, Config::Aim::kmboxPort, Config::Aim::kmboxMac)) {
		if (Kmbox::IsNetDevice()) {
			Logger.log("Aimbot initialized with KmboxNet");
		} else if (Kmbox::IsMakcuDevice()) {
			Logger.log("Aimbot initialized with MAKCU");
		} else {
			Logger.log("Aimbot initialized with serial kmbox");
		}
	} else {
		Logger.log(LogLevel::WARNING, "Aimbot running without kmbox — using local SendInput fallback");
	}

	aimbotInitialized = true;
}

void Cleanup() {
	Kmbox::Cleanup();
	aimbotInitialized = false;
}

void AimTo(const SDK::Vec2& position) {
	if (!Config::Aim::enable || !position) {
		return;
	}

	if (!ReadAimKeyHeld(Config::Aim::aimkey)) {
		return;
	}

	const float screenCenterX = Render::g_screenWidth / 2.f;
	const float screenCenterY = Render::g_screenHeight / 2.f;

	float targetX = 0.f;
	float targetY = 0.f;

	if (position.X != 0.f) {
		if (position.X > screenCenterX) {
			targetX = -(screenCenterX - position.X);
			targetX /= Config::Aim::smoothingX;
			if (targetX + screenCenterX > screenCenterX * 2.f) {
				targetX = 0.f;
			}
		}

		if (position.X < screenCenterX) {
			targetX = position.X - screenCenterX;
			targetX /= Config::Aim::smoothingX;
			if (targetX + screenCenterX < 0.f) {
				targetX = 0.f;
			}
		}
	}

	if (position.Y != 0.f) {
		if (position.Y > screenCenterY) {
			targetY = -(screenCenterY - position.Y);
			targetY /= Config::Aim::smoothingY;
			if (targetY + screenCenterY > screenCenterY * 2.f) {
				targetY = 0.f;
			}
		}

		if (position.Y < screenCenterY) {
			targetY = position.Y - screenCenterY;
			targetY /= Config::Aim::smoothingY;
			if (targetY + screenCenterY < 0.f) {
				targetY = 0.f;
			}
		}
	}

	MoveMouseDirect(targetX, targetY);
}

}
