#pragma once

#include <atomic>
#include <array>
#include <string>
#include <vector>

struct PlayerData {
	int playerIndex = -1;
	ULONG64 player = 0;
	ULONG64 posPtr = 0;
	ULONG64 bonePtr = 0;

	SDK::Vec3 worldPosition {};
	std::array<SDK::Vec3, SDK::BONE_IDX_COUNT> bones {};
	bool hasValidBones = false;
	SDK::Vec3 skeletonSnapshotRoot {};

	int playerHealth = 0;
	std::string playerName;

	int kills = 0;
	int deaths = 0;
	int sbStatus = 0;
	int isBot = 0;

	bool isVisible = false;
	float distance = 0.f;
};

struct CacheDebugStats {
	int inGameMode = 0;
	int slots = 0;
	int populated = 0;
	int valid = 0;
	int alive = 0;
	int enemies = 0;
	int espTargets = 0;
	float snapshotHz = 0.f;
	float dataAgeMs = 0.f;
	uint64_t snapshotCount = 0;
	bool inMatch = false;
};

struct ViewSnapshot {
	SDK::RefDef_T refDef {};
	SDK::Vec3 cameraPosition {};
	SDK::Vec3 localPlayerPosition {};
	bool inMatch = false;
};

struct FrameSnapshot {
	std::vector<PlayerData> players;
	ViewSnapshot view {};
};

namespace Cache {
	inline ULONG64 boneBase = 0;
	inline ULONG64 clientInfo = 0;
	inline ULONG64 clientBase = 0;
	inline ULONG64 localPlayer = 0;
	inline ULONG64 nameArrayPtr = 0;
	inline ULONG64 cameraBasePtr = 0;
	inline ULONG64 refDefPtr = 0;

	inline SDK::Vec3 cameraPosition = SDK::Vec3();
	inline SDK::Vec3 boneBasePosition = SDK::Vec3();
	inline SDK::Vec3 localPlayerPosition = SDK::Vec3();
	inline int localPlayerTeamIndex = 0;

	void Start();
	void Stop();

	FrameSnapshot GetFrameSnapshot();
	bool TryReadLiveRenderView(ViewSnapshot& outView);
	bool RefreshLivePlayerPositions(std::vector<PlayerData>& players);
	CacheDebugStats GetDebugStats();
}
