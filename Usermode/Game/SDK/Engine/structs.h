#pragma once

namespace SDK {
	// BO7 layout (m4nuuU / Eternityforks UC struct). Older builds had name at 0x4, health at 0x84.
	namespace NameEntryLayout {
		constexpr auto name = 0x10;
		constexpr auto health = 0x90;        // BO7 name_entry (m4nuuU)
		constexpr auto health_legacy = 0x84; // UC FUSER / Shampiss read path (same name_array offsets)
		constexpr auto health_byte = 0x70;   // stale-health fallback (UC FUSER)
	}

	struct NameEntry {
		char pad_0[12];
		int index;
		char name[36];
		char nameWithHash[64];
		int rank_mp;
		int prestige_mp;
		int rank_alien;
		char clanAbbrev[9];
		uint8_t clanTagType;
		int location;
		int health;
		bool isMLGSpectator;
		bool isMLGFollower;
		int indexMLGFollower;
		char bountyCount;
		uint32_t perkIconName;
		int squadIndex;
	};

	enum ScoreboardEntry_Status : int {
		STATUS_ALIVE	= 0,
		STATUS_DEAD		= 2,
		STATUS_DOWNED	= 5
	};

	struct ScoreboardEntry {
		int		index;
		int		status;
		int		points;
		int		networkPing;
		int		deaths;
		int		teamId;
		int		isBot; // 1 is bot, 0 otherwise
		int		kills;
	};

	struct ClientBits {
		int array[7];
	};

	enum BoneIndex : unsigned long {
		BONE_POS_HELMET = 8,

		BONE_POS_HEAD = 7,
		BONE_POS_NECK = 6,
		BONE_POS_CHEST = 5,
		BONE_POS_MID = 4,
		BONE_POS_TUMMY = 3,
		BONE_POS_PELVIS = 2,

		BONE_POS_RIGHT_FOOT_1 = 21,
		BONE_POS_RIGHT_FOOT_2 = 22,
		BONE_POS_RIGHT_FOOT_3 = 23,
		BONE_POS_RIGHT_FOOT_4 = 24,

		BONE_POS_LEFT_FOOT_1 = 17,
		BONE_POS_LEFT_FOOT_2 = 18,
		BONE_POS_LEFT_FOOT_3 = 19,
		BONE_POS_LEFT_FOOT_4 = 20,

		BONE_POS_LEFT_HAND_1 = 13,
		BONE_POS_LEFT_HAND_2 = 14,
		BONE_POS_LEFT_HAND_3 = 15,
		BONE_POS_LEFT_HAND_4 = 16,

		BONE_POS_RIGHT_HAND_1 = 9,
		BONE_POS_RIGHT_HAND_2 = 10,
		BONE_POS_RIGHT_HAND_3 = 11,
		BONE_POS_RIGHT_HAND_4 = 12
	};
}
