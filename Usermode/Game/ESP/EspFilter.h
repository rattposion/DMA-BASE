#pragma once

#include "Game/cache.h"

namespace EspFilter {
	inline bool HasCodStyleAliveHealth(const PlayerData& player) {
		return player.playerHealth > 0 && player.playerHealth <= 127;
	}

	inline bool IsScoreboardAliveStatus(int status) {
		return status == SDK::STATUS_ALIVE || status == SDK::STATUS_DOWNED;
	}

	inline bool IsPlayerAliveForEsp(const PlayerData& player) {
		if (!player.player) {
			return false;
		}

		if (player.sbStatus == SDK::STATUS_DEAD) {
			return false;
		}

		return HasCodStyleAliveHealth(player);
	}

	inline bool PlayerNeedsLivePositionRefresh(float distanceMeters) {
		if (distanceMeters <= static_cast<float>(Config::ESP::liveRefreshDistance)) {
			return true;
		}

		if (Config::Aim::enable && distanceMeters <= Config::Aim::maxAimbotDistance) {
			return true;
		}

		return false;
	}
}
