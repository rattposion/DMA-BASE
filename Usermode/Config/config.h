#pragma once

enum class HealthBarPosition {
	Left,
	Right,
	Top,
	Bottom
};

enum class AimBone {
	Head,
	Neck,
	Chest,
	Pelvis,
	Smart
};

namespace Config {
	namespace Aim {
		inline bool enable = true;
		
		inline float smoothingX = 10.f;
		inline float smoothingY = 20.f;
		inline bool humanisation = true;

		inline float FOV = 80.f;
		inline bool showFOV = true;

		inline int aimkey = VK_RBUTTON;
		inline int aimbone = 1;

		inline bool targetLine = false;
		inline ImVec4 targetLineColor = ImAdd::Hex2RGBA(0xED5858, 1.f); 
		
		inline bool visibleCheck = true;
		inline bool teamCheck = true;
		inline float maxAimbotDistance = 100.f;

		inline bool useKmbox = true;
		inline int kmboxDevice = 0; // 0=Net, 1=Serial, 2=MAKCU
		inline int kmboxComPort = 0; // 0 = auto
		inline int movementType = 1;
		inline int movementTime = 50;
		extern char kmboxIp[32];
		extern char kmboxPort[8];
		extern char kmboxMac[16];
		inline int kmboxMonitorPort = 8888;
	}

	namespace ESP {
		inline bool enable = true;

		inline bool box = true;
		inline int boxType = 1; // 0 is normal, 1 is corner
		inline ImVec4 boxVisibleOutlineColor = ImAdd::Hex2RGBA(0x58ED6E, 1.f);
		inline ImVec4 boxInvisibleOutlineColor = ImAdd::Hex2RGBA(0xED5858, 1.f);

		inline bool boxFilled = true;
		inline ImVec4 boxVisibleFilledColor = ImAdd::Hex2RGBA(0x121111, 0.5f);
		inline ImVec4 boxInvisibleFilledColor = boxVisibleFilledColor;

		inline bool indicator = true;

		inline bool nickname = false;
		inline ImVec4 nicknameVisibleColor = ImAdd::Hex2RGBA(0x58ED6E, 1.f);
		inline ImVec4 nicknameInvisibleColor = ImAdd::Hex2RGBA(0xED5858, 1.f);

		inline bool health = true;
		inline int healthBarPos = 0;

		inline bool snaplines = false;
		inline ImVec4 snaplinesVisibleColor = ImAdd::Hex2RGBA(0x58ED6E, 1.f);
		inline ImVec4 snaplinesInvisibleColor = ImAdd::Hex2RGBA(0xED5858, 1.f);

		inline bool distance = false;
		inline ImVec4 distanceVisibleColor = ImAdd::Hex2RGBA(0x58ED6E, 1.f);
		inline ImVec4 distanceInvisibleColor = ImAdd::Hex2RGBA(0xED5858, 1.f);

		inline bool skeleton = true;
		inline ImVec4 skeletonVisibleColor = ImAdd::Hex2RGBA(0xF774F1, 1.f);
		inline ImVec4 skeletonInvisibleColor = ImAdd::Hex2RGBA(0xF774F1, 1.f);

		inline int maxESPDistance = 200;
		inline int liveRefreshDistance = 150;
	}

	namespace Settings {
		inline bool debug = false;
		inline bool useLiveRenderReads = false;
	}

	void SaveSettings();
	void LoadSettings();
}