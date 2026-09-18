#include "global.h"
#include "Game/ESP/EspFilter.h"
#include <algorithm>
#include <atomic>
#include <mutex>
#include <mutex>
#include <optional>
#include <thread>
#include <unordered_map>

namespace Cache {
	namespace {
		using namespace SDK;

		constexpr int MAX_PLAYERS = 150;
		constexpr int BONE_BATCH_SIZE = 24;
		constexpr int PLAYER_SCAN_BATCH_SIZE = 24;
		constexpr size_t BONE_DATA_READ_SIZE = (SDK::BONE_POS_RIGHT_FOOT_4 + 1) * 0x20;

		struct PublishedFrameSlot {
			ViewSnapshot view {};
			std::vector<PlayerData> players {};
		};

		std::array<PublishedFrameSlot, 3> frameSlots {};
		std::atomic<int> publishedSlotIndex { -1 };
		int cacheWriteSlot = 0;

		std::mutex framePublishMutex;
		std::mutex debugStatsMutex;
		std::thread g_cacheThread;
		std::atomic<bool> g_cacheRunning { false };

		SDK::RefDef_T workerRefDef {};
		SDK::Vec3 workerCameraPosition {};

		bool ShouldSkipTeammate(UCHAR teamIndex) {
			return Config::Aim::teamCheck && teamIndex == localPlayerTeamIndex;
		}
		CacheDebugStats publishedDebugStats {};
		CacheDebugStats latestDebugStats {};
		std::chrono::steady_clock::time_point lastSnapshotPublishTime {};
		uint64_t snapshotPublishCount = 0;
		std::array<std::chrono::steady_clock::time_point, 32> recentPublishTimes {};
		size_t recentPublishWrite = 0;
		size_t recentPublishCount = 0;

		SDK::ClientBits visibilityBits {};
		int localPlayerIndex = -1;
		UCHAR localPlayerTeamIndex = 0;
		ULONG64 trackedClientBase = 0;
		int cachedGameMode = 0;
		bool matchRediscoveryDone = false;
		std::optional<std::chrono::steady_clock::time_point> matchEnteredAt {};

		constexpr auto MATCH_REDISCOVERY_DELAY = std::chrono::seconds(20);

		thread_local std::vector<uint8_t> s_validByte;
		thread_local std::vector<UCHAR> s_team;
		thread_local std::vector<ULONG64> s_posPtr;
		thread_local std::vector<SDK::Vec3> s_posPrimary;
		thread_local std::vector<SDK::ScoreboardEntry> s_scoreboard;
		thread_local std::vector<SDK::NameEntry> s_nameEntry;
		thread_local std::vector<float> s_structHealth;
		thread_local std::vector<int> s_nameHealth90;
		thread_local std::vector<int> s_nameHealth84;
		thread_local std::vector<uint8_t> s_nameHealthByte;

		bool IsIndexVisible(const SDK::ClientBits& bits, int index) {
			const auto bitmask = 0x80000000 >> (index & 0x1F);
			return bits.array[index >> 5] & bitmask;
		}

		int ResolvePlayerHealth(float playerStructHealth, int nameHealth90, int nameHealth84, uint8_t nameHealthByte) {
			auto resolveNameHealth = [&]() -> int {
				if (nameHealth84 > 0 && nameHealth84 <= 127) {
					return nameHealth84;
				}

				if (nameHealth90 > 0 && nameHealth90 <= 127) {
					return nameHealth90;
				}

				if (nameHealthByte > 0 && nameHealthByte <= 100) {
					return nameHealthByte;
				}

				return 0;
			};

			auto resolveStructHealth = [&]() -> int {
				if (playerStructHealth > 0.f && playerStructHealth <= 200.f) {
					return static_cast<int>(playerStructHealth);
				}

				int structHealthInt = 0;
				std::memcpy(&structHealthInt, &playerStructHealth, sizeof(structHealthInt));
				if (structHealthInt > 0 && structHealthInt <= 127) {
					return structHealthInt;
				}

				return 0;
			};

			const int nameHealth = resolveNameHealth();
			const int structHealth = resolveStructHealth();
			if (nameHealth > 0 && structHealth > 0) {
				return std::min(nameHealth, structHealth);
			}

			if (structHealth > 0) {
				return structHealth;
			}

			return nameHealth;
		}

		void EnsureRefreshBuffers(size_t count) {
			if (s_validByte.size() < count) {
				s_validByte.resize(count);
				s_team.resize(count);
				s_posPtr.resize(count);
				s_posPrimary.resize(count);
				s_scoreboard.resize(count);
				s_nameEntry.resize(count);
				s_structHealth.resize(count);
				s_nameHealth90.resize(count);
				s_nameHealth84.resize(count);
				s_nameHealthByte.resize(count);
			}
		}

		void CacheLocalPlayerIndex() {
			using namespace SDK;

			localPlayerIndex = -1;
			if (!DMAInterface::IsValidPointer(clientInfo)) {
				return;
			}

			const auto localIndexBase = DMAInterface::Read<ULONG64>(clientInfo + Offsets::local_index);
			if (DMAInterface::IsValidPointer(localIndexBase)) {
				localPlayerIndex = DMAInterface::Read<int>(localIndexBase + Offsets::local_index_pos);
			}
		}

		bool NeedsNameEntryReads() {
			return Config::ESP::nickname || Config::ESP::health;
		}

		bool PassesAliveFilter(int sbStatus, int playerHealth) {
			if (sbStatus == STATUS_DEAD) {
				return false;
			}

			return playerHealth > 0 && playerHealth <= 127;
		}

		void ClearBonePersistenceEntry(int playerIndex);
		void ClearPlayerBoneState(PlayerData& player);
		void RefreshPlayerAliveState(std::vector<PlayerData>& players);

		CacheDebugStats BuildDebugStats(int maxPlayers, bool hasClientInfo) {
			CacheDebugStats stats {};
			stats.inGameMode = cachedGameMode;
			stats.slots = maxPlayers;
			stats.inMatch = cachedGameMode > 1;

			for (int i = 0; i < maxPlayers; ++i) {
				if (s_posPtr[i] >= 0x10000 || s_validByte[i] != 0) {
					++stats.populated;
				}

				const bool slotValid = s_validByte[i] != 0 && s_posPtr[i] >= 0x10000;
				if (slotValid) {
					++stats.valid;
				}

				const int playerHealth = ResolvePlayerHealth(
					s_structHealth[i],
					s_nameHealth90[i],
					s_nameHealth84[i],
					s_nameHealthByte[i]
				);
				const int sbStatus = hasClientInfo ? s_scoreboard[i].status : STATUS_ALIVE;

				if (!PassesAliveFilter(sbStatus, playerHealth)) {
					continue;
				}

				++stats.alive;

				if (i == localPlayerIndex || s_team[i] == localPlayerTeamIndex) {
					continue;
				}

				if (!slotValid) {
					continue;
				}

				++stats.enemies;

				SDK::Vec3 playerWorldPosition = s_posPrimary[i];
				if (playerWorldPosition) {
					++stats.espTargets;
				}
			}

			return stats;
		}

		void RefreshAllPlayers(int maxPlayers, std::vector<PlayerData>& outPlayers) {
			using namespace SDK;

			if (!DMAInterface::IsValidPointer(clientBase) || maxPlayers <= 0) {
				outPlayers.clear();
				return;
			}

			const size_t slotCount = static_cast<size_t>(maxPlayers);
			EnsureRefreshBuffers(slotCount);

			const bool hasClientInfo = DMAInterface::IsValidPointer(clientInfo);
			if (DMAInterface::IsValidPointer(globals::g_baseAddress)) {
				const ULONG64 freshNameArrayPtr = DMAInterface::Read<ULONG64>(globals::g_baseAddress + Offsets::name_array);
				if (freshNameArrayPtr >= 0x10000) {
					nameArrayPtr = freshNameArrayPtr;
				}
			}

			const bool hasNameArray = nameArrayPtr >= 0x10000;

			for (int batchStart = 0; batchStart < maxPlayers; batchStart += PLAYER_SCAN_BATCH_SIZE) {
				const int batchEnd = (std::min)(batchStart + PLAYER_SCAN_BATCH_SIZE, maxPlayers);

				auto scatterHandle = mem.GetScatterHandle();
				if (!scatterHandle) {
					return;
				}

				for (int i = batchStart; i < batchEnd; ++i) {
					const auto playerPtr = clientBase + (static_cast<ULONG64>(i) * Offsets::Player::size);

					mem.AddScatterReadRequest(scatterHandle, playerPtr + Offsets::Player::valid, &s_validByte[i], sizeof(s_validByte[i]));
					mem.AddScatterReadRequest(scatterHandle, playerPtr + Offsets::Player::team, &s_team[i], sizeof(s_team[i]));
					mem.AddScatterReadRequest(scatterHandle, playerPtr + Offsets::Player::pos, &s_posPtr[i], sizeof(s_posPtr[i]));
					mem.AddScatterReadRequest(scatterHandle, playerPtr + Offsets::Player::health, &s_structHealth[i], sizeof(s_structHealth[i]));

					if (hasClientInfo) {
						mem.AddScatterReadRequest(
							scatterHandle,
							clientInfo + Offsets::scoreboard + (static_cast<ULONG64>(i) * Offsets::scoreboardsize),
							&s_scoreboard[i],
							sizeof(s_scoreboard[i])
						);
					}

					if (hasNameArray) {
						const auto nameEntryAddress =
							nameArrayPtr + Offsets::name_array_pos + (static_cast<ULONG64>(i) * Offsets::name_array_size);

						mem.AddScatterReadRequest(scatterHandle, nameEntryAddress, &s_nameEntry[i], sizeof(s_nameEntry[i]));
						mem.AddScatterReadRequest(scatterHandle, nameEntryAddress + NameEntryLayout::health, &s_nameHealth90[i], sizeof(s_nameHealth90[i]));
						mem.AddScatterReadRequest(scatterHandle, nameEntryAddress + NameEntryLayout::health_legacy, &s_nameHealth84[i], sizeof(s_nameHealth84[i]));
						mem.AddScatterReadRequest(scatterHandle, nameEntryAddress + NameEntryLayout::health_byte, &s_nameHealthByte[i], sizeof(s_nameHealthByte[i]));
					}
				}

				mem.ExecuteReadScatter(scatterHandle);
			}

			if (localPlayerIndex >= 0 && localPlayerIndex < maxPlayers) {
				localPlayerTeamIndex = s_team[localPlayerIndex];
				Cache::localPlayerTeamIndex = static_cast<int>(localPlayerTeamIndex);
			}

			for (int batchStart = 0; batchStart < maxPlayers; batchStart += PLAYER_SCAN_BATCH_SIZE) {
				const int batchEnd = (std::min)(batchStart + PLAYER_SCAN_BATCH_SIZE, maxPlayers);

				auto scatterHandle = mem.GetScatterHandle();
				if (!scatterHandle) {
					return;
				}

				bool queuedPositions = false;
				for (int i = batchStart; i < batchEnd; ++i) {
					if (s_posPtr[i] < 0x10000) {
						continue;
					}

					if (i != localPlayerIndex) {
						if (!s_validByte[i] || ShouldSkipTeammate(s_team[i])) {
							continue;
						}
					}

					mem.AddScatterReadRequest(scatterHandle, s_posPtr[i] + 0x80, &s_posPrimary[i], sizeof(s_posPrimary[i]));
					queuedPositions = true;
				}

				if (queuedPositions) {
					mem.ExecuteReadScatter(scatterHandle);
				}
			}

			if (localPlayerIndex >= 0 && localPlayerIndex < maxPlayers && s_posPtr[localPlayerIndex] >= 0x10000) {
				const SDK::Vec3 localPos = s_posPrimary[localPlayerIndex];
				if (localPos) {
					localPlayerPosition = localPos;
				}
			}

			const CacheDebugStats debugStats = BuildDebugStats(maxPlayers, hasClientInfo);
			latestDebugStats = debugStats;

			outPlayers.clear();
			outPlayers.reserve(maxPlayers);

			for (int i = 0; i < maxPlayers; ++i) {
				if (i == localPlayerIndex) {
					continue;
				}

				if (!s_validByte[i] || s_posPtr[i] < 0x10000 || ShouldSkipTeammate(s_team[i])) {
					continue;
				}

				SDK::Vec3 playerWorldPosition = s_posPrimary[i];
				if (!playerWorldPosition) {
					continue;
				}

				const int playerHealth = ResolvePlayerHealth(
					s_structHealth[i],
					s_nameHealth90[i],
					s_nameHealth84[i],
					s_nameHealthByte[i]
				);
				const int sbStatus = hasClientInfo ? s_scoreboard[i].status : STATUS_ALIVE;

				if (!PassesAliveFilter(sbStatus, playerHealth)) {
					ClearBonePersistenceEntry(i);
					continue;
				}

				PlayerData cachedPlayer {};
				cachedPlayer.playerIndex = i;
				cachedPlayer.player = clientBase + (static_cast<ULONG64>(i) * Offsets::Player::size);
				cachedPlayer.posPtr = s_posPtr[i];
				cachedPlayer.playerHealth = playerHealth;
				if (hasNameArray) {
					cachedPlayer.playerName = SDK::Engine::Player::GetPlayerNameFromNameEntry(s_nameEntry[i]);
				}
				cachedPlayer.kills = s_scoreboard[i].kills;
				cachedPlayer.deaths = s_scoreboard[i].deaths;
				cachedPlayer.sbStatus = sbStatus;
				cachedPlayer.isBot = s_scoreboard[i].isBot;
				cachedPlayer.isVisible = IsIndexVisible(visibilityBits, i);
				cachedPlayer.distance = localPlayerPosition.Distance(playerWorldPosition) / 40.f;
				cachedPlayer.worldPosition = playerWorldPosition;
				outPlayers.push_back(std::move(cachedPlayer));
			}
		}

		void UpdateLivePlayerData(
			std::vector<PlayerData>& players,
			const SDK::ClientBits& visibilityBits,
			const SDK::Vec3& localPosition)
		{
			if (players.empty() || !localPosition) {
				return;
			}

			const size_t playerCount = players.size();
			std::vector<Vec3> positions(playerCount, Vec3 {});

			auto scatterHandle = mem.GetScatterHandle();
			if (!scatterHandle) {
				return;
			}

			bool queuedPtrRefresh = false;
			for (size_t i = 0; i < playerCount; ++i) {
				if (players[i].posPtr >= 0x10000) {
					continue;
				}

				mem.AddScatterReadRequest(
					scatterHandle,
					players[i].player + Offsets::Player::pos,
					&players[i].posPtr,
					sizeof(players[i].posPtr)
				);
				queuedPtrRefresh = true;
			}

			if (queuedPtrRefresh) {
				mem.ExecuteReadScatter(scatterHandle);
				scatterHandle = mem.GetScatterHandle();
				if (!scatterHandle) {
					return;
				}
			}

			bool queuedPositions = false;
			for (size_t i = 0; i < playerCount; ++i) {
				if (players[i].posPtr < 0x10000) {
					continue;
				}

				mem.AddScatterReadRequest(
					scatterHandle,
					players[i].posPtr + 0x80,
					&positions[i],
					sizeof(positions[i])
				);
				queuedPositions = true;
			}

			if (!queuedPositions) {
				return;
			}

			mem.ExecuteReadScatter(scatterHandle);

			for (size_t i = 0; i < playerCount; ++i) {
				const Vec3& position = positions[i];
				if (!position) {
					continue;
				}

				// Keep sticky skeletons glued to live feet between bone DMA ticks.
				if ((Config::ESP::skeleton || Config::Aim::enable)
					&& players[i].hasValidBones
					&& players[i].bones[BONE_IDX_PELVIS])
				{
					const Vec3 previous = players[i].worldPosition;
					if (previous) {
						const Vec3 delta = position - previous;
						if (delta.Length() > 0.01f) {
							for (auto& bone : players[i].bones) {
								if (bone) {
									bone += delta;
								}
							}
							players[i].skeletonSnapshotRoot = position;
						}
					}
				}

				players[i].worldPosition = position;
				players[i].distance = localPosition.Distance(position) / 40.f;
				players[i].isVisible = IsIndexVisible(visibilityBits, players[i].playerIndex);
			}
		}

		struct BonePersistenceEntry {
			std::array<Vec3, BONE_IDX_COUNT> bones {};
			Vec3 anchorPosition {};
			DWORD lastGoodReadMs = 0;
			bool hasHistory = false;
		};

		std::unordered_map<int, BonePersistenceEntry> bonePersistence;
		constexpr DWORD BONE_PERSIST_TIMEOUT_MS = 3500;

		void ClearPublishedDebugStats() {
			latestDebugStats = {};
			publishedDebugStats = {};
			lastSnapshotPublishTime = {};
			snapshotPublishCount = 0;
			recentPublishWrite = 0;
			recentPublishCount = 0;
		}

		void ResetMatchState() {
			clientInfo = 0;
			clientBase = 0;
			localPlayer = 0;
			nameArrayPtr = 0;
			cameraBasePtr = 0;
			boneBase = 0;
			refDefPtr = 0;
			trackedClientBase = 0;
			localPlayerIndex = -1;
			localPlayerTeamIndex = 0;
			Cache::localPlayerTeamIndex = 0;
			cachedGameMode = 0;
			localPlayerPosition = {};
			boneBasePosition = {};
			visibilityBits = {};
			bonePersistence.clear();
			ClearPublishedDebugStats();
			matchRediscoveryDone = false;
			matchEnteredAt.reset();
		}

		void ForcePlayerRediscovery(std::vector<PlayerData>& workingPlayers) {
			workingPlayers.clear();
			bonePersistence.clear();
			trackedClientBase = 0;
			localPlayerIndex = -1;
			localPlayerTeamIndex = 0;
			Cache::localPlayerTeamIndex = 0;
			localPlayer = 0;
			boneBase = 0;
			nameArrayPtr = 0;
		}

		void ClearBonePersistenceEntry(int playerIndex) {
			bonePersistence.erase(playerIndex);
		}

		void ClearPlayerBoneState(PlayerData& player) {
			ClearBonePersistenceEntry(player.playerIndex);
			player.bonePtr = 0;
			player.bones = {};
			player.hasValidBones = false;
		}

		void RefreshPlayerAliveState(std::vector<PlayerData>& players) {
			if (players.empty() || !DMAInterface::IsValidPointer(clientInfo)) {
				return;
			}

			const size_t playerCount = players.size();
			thread_local std::vector<ScoreboardEntry> scoreboardEntries;
			thread_local std::vector<float> structHealth;
			thread_local std::vector<int> nameHealth90;
			thread_local std::vector<int> nameHealth84;
			thread_local std::vector<uint8_t> nameHealthByte;
			thread_local std::vector<NameEntry> nameEntries;

			scoreboardEntries.resize(playerCount);
			structHealth.resize(playerCount);
			nameHealth90.resize(playerCount);
			nameHealth84.resize(playerCount);
			nameHealthByte.resize(playerCount);
			nameEntries.resize(playerCount);

			if (DMAInterface::IsValidPointer(globals::g_baseAddress)) {
				const ULONG64 freshNameArrayPtr = DMAInterface::Read<ULONG64>(globals::g_baseAddress + Offsets::name_array);
				if (freshNameArrayPtr >= 0x10000) {
					nameArrayPtr = freshNameArrayPtr;
				}
			}

			const bool hasNameArray = nameArrayPtr >= 0x10000;

			auto scatterHandle = mem.GetScatterHandle();
			if (!scatterHandle) {
				return;
			}

			for (size_t i = 0; i < playerCount; ++i) {
				const int playerIndex = players[i].playerIndex;
				mem.AddScatterReadRequest(
					scatterHandle,
					clientInfo + Offsets::scoreboard + (static_cast<ULONG64>(playerIndex) * Offsets::scoreboardsize),
					&scoreboardEntries[i],
					sizeof(scoreboardEntries[i])
				);
				mem.AddScatterReadRequest(
					scatterHandle,
					players[i].player + Offsets::Player::health,
					&structHealth[i],
					sizeof(structHealth[i])
				);

				if (hasNameArray) {
					const auto nameEntryAddress =
						nameArrayPtr + Offsets::name_array_pos + (static_cast<ULONG64>(playerIndex) * Offsets::name_array_size);

					mem.AddScatterReadRequest(scatterHandle, nameEntryAddress, &nameEntries[i], sizeof(nameEntries[i]));
					mem.AddScatterReadRequest(scatterHandle, nameEntryAddress + NameEntryLayout::health, &nameHealth90[i], sizeof(nameHealth90[i]));
					mem.AddScatterReadRequest(scatterHandle, nameEntryAddress + NameEntryLayout::health_legacy, &nameHealth84[i], sizeof(nameHealth84[i]));
					mem.AddScatterReadRequest(scatterHandle, nameEntryAddress + NameEntryLayout::health_byte, &nameHealthByte[i], sizeof(nameHealthByte[i]));
				}
			}

			mem.ExecuteReadScatter(scatterHandle);

			for (size_t i = 0; i < playerCount; ++i) {
				const int playerIndex = players[i].playerIndex;
				players[i].sbStatus = scoreboardEntries[i].status;
				players[i].playerHealth = ResolvePlayerHealth(
					structHealth[i],
					nameHealth90[i],
					nameHealth84[i],
					nameHealthByte[i]
				);
				players[i].kills = scoreboardEntries[i].kills;
				players[i].deaths = scoreboardEntries[i].deaths;
				players[i].isBot = scoreboardEntries[i].isBot;

				if (hasNameArray) {
					players[i].playerName = SDK::Engine::Player::GetPlayerNameFromNameEntry(nameEntries[i]);
				}
			}

			for (auto it = players.begin(); it != players.end(); ) {
				if (!PassesAliveFilter(it->sbStatus, it->playerHealth)) {
					ClearPlayerBoneState(*it);
					it = players.erase(it);
				} else {
					++it;
				}
			}
		}

		void RecordGoodBones(PlayerData& player, const std::array<Vec3, BONE_IDX_COUNT>& bones) {
			BonePersistenceEntry& entry = bonePersistence[player.playerIndex];
			entry.bones = bones;
			entry.anchorPosition = player.worldPosition;
			entry.lastGoodReadMs = GetTickCount();
			entry.hasHistory = true;
			player.bones = bones;
			player.hasValidBones = true;
			player.skeletonSnapshotRoot = player.worldPosition;
		}

		void ApplyBonePersistenceFallback(PlayerData& player) {
			const auto it = bonePersistence.find(player.playerIndex);
			if (it == bonePersistence.end() || !it->second.hasHistory || !player.worldPosition) {
				player.bonePtr = 0;
				player.bones = {};
				player.hasValidBones = false;
				return;
			}

			const DWORD now = GetTickCount();
			if (now - it->second.lastGoodReadMs > BONE_PERSIST_TIMEOUT_MS) {
				it->second.hasHistory = false;
				player.bonePtr = 0;
				player.bones = {};
				player.hasValidBones = false;
				return;
			}

			const Vec3 deltaMove = player.worldPosition - it->second.anchorPosition;
			std::array<Vec3, BONE_IDX_COUNT> translatedBones = it->second.bones;
			for (auto& bone : translatedBones) {
				bone += deltaMove;
			}

			player.bones = translatedBones;
			player.hasValidBones = true;
			player.skeletonSnapshotRoot = player.worldPosition;
			it->second.bones = translatedBones;
			it->second.anchorPosition = player.worldPosition;
		}

		void RestoreBonesFromPersistence(std::vector<PlayerData>& players) {
			for (auto& player : players) {
				if (!EspFilter::IsPlayerAliveForEsp(player) || !player.worldPosition) {
					continue;
				}
				if (player.hasValidBones && player.bones[BONE_IDX_PELVIS]) {
					continue;
				}
				ApplyBonePersistenceFallback(player);
			}
		}

		size_t GetMaxBoneReads(size_t playerCount) {
			// Cap DMA so 50–100 lobbies still finish a bone tick; sticky covers the rest.
			if (playerCount <= 24) {
				return playerCount;
			}
			if (playerCount <= 50) {
				return 24;
			}
			if (playerCount <= 80) {
				return 18;
			}
			return 14;
		}

		float GetMaxBoneReadDistance() {
			float maxDistance = static_cast<float>(Config::ESP::maxESPDistance);
			if (Config::Aim::enable) {
				maxDistance = (std::max)(maxDistance, Config::Aim::maxAimbotDistance);
			}
			return maxDistance;
		}

		void PopulatePlayerBones(std::vector<PlayerData>& players) {
			if (players.empty()) {
				return;
			}

			// Full scans rebuild PlayerData empty — put last-good skeletons back first.
			RestoreBonesFromPersistence(players);

			if (!DMAInterface::IsValidPointer(boneBase)) {
				return;
			}

			if (DMAInterface::IsValidPointer(clientInfo)) {
				const Vec3 freshBoneBase = DMAInterface::Read<Vec3>(clientInfo + Offsets::Bone::bone_base);
				if (freshBoneBase) {
					boneBasePosition = freshBoneBase;
				}
			}

			std::vector<size_t> eligiblePlayerIndices;
			eligiblePlayerIndices.reserve(players.size());

			const float maxDistance = GetMaxBoneReadDistance();
			for (size_t i = 0; i < players.size(); ++i) {
				if (!EspFilter::IsPlayerAliveForEsp(players[i])) {
					ClearPlayerBoneState(players[i]);
					continue;
				}

				if (players[i].distance <= maxDistance) {
					eligiblePlayerIndices.push_back(i);
				}
			}

			if (eligiblePlayerIndices.empty()) {
				return;
			}

			// Nearest-first so close skeletons stay live in big lobbies.
			std::sort(eligiblePlayerIndices.begin(), eligiblePlayerIndices.end(),
				[&](size_t a, size_t b) {
					return players[a].distance < players[b].distance;
				});

			const size_t maxBoneReads = GetMaxBoneReads(players.size());
			if (eligiblePlayerIndices.size() > maxBoneReads) {
				eligiblePlayerIndices.resize(maxBoneReads);
			}

			const Vec3 boneBasePos = boneBasePosition;
			const size_t eligibleCount = eligiblePlayerIndices.size();

			static thread_local std::array<size_t, BONE_BATCH_SIZE> s_batchPlayerIndices {};
			static thread_local std::array<uint16_t, BONE_BATCH_SIZE> s_boneIndices {};
			static thread_local std::array<ULONG64, BONE_BATCH_SIZE> s_bonePtrs {};
			static thread_local std::array<std::array<char, BONE_DATA_READ_SIZE>, BONE_BATCH_SIZE> s_boneBuffers {};

			for (size_t batchStart = 0; batchStart < eligibleCount; batchStart += BONE_BATCH_SIZE) {
				const size_t batchEnd = std::min(eligibleCount, batchStart + BONE_BATCH_SIZE);
				const size_t batchCount = batchEnd - batchStart;

				for (size_t batchIndex = 0; batchIndex < batchCount; ++batchIndex) {
					const size_t playerDataIndex = eligiblePlayerIndices[batchStart + batchIndex];
					s_batchPlayerIndices[batchIndex] = playerDataIndex;
					s_boneIndices[batchIndex] = get_bone_index(players[playerDataIndex].playerIndex);
					s_bonePtrs[batchIndex] = 0;
				}

				auto scatterHandle = mem.GetScatterHandle();
				if (!scatterHandle) {
					continue;
				}

				for (size_t batchIndex = 0; batchIndex < batchCount; ++batchIndex) {
					mem.AddScatterReadRequest(
						scatterHandle,
						boneBase + (static_cast<ULONG64>(s_boneIndices[batchIndex]) * Offsets::Bone::size) + Offsets::Bone::offset,
						&s_bonePtrs[batchIndex],
						sizeof(s_bonePtrs[batchIndex])
					);
				}

				mem.ExecuteReadScatter(scatterHandle);

				scatterHandle = mem.GetScatterHandle();
				if (!scatterHandle) {
					continue;
				}

				for (size_t batchIndex = 0; batchIndex < batchCount; ++batchIndex) {
					if (s_bonePtrs[batchIndex] >= 0x10000) {
						mem.AddScatterReadRequest(
							scatterHandle,
							s_bonePtrs[batchIndex],
							s_boneBuffers[batchIndex].data(),
							BONE_DATA_READ_SIZE
						);
					}
				}

				mem.ExecuteReadScatter(scatterHandle);

				for (size_t batchIndex = 0; batchIndex < batchCount; ++batchIndex) {
					const size_t playerDataIndex = s_batchPlayerIndices[batchIndex];
					if (s_bonePtrs[batchIndex] < 0x10000) {
						ApplyBonePersistenceFallback(players[playerDataIndex]);
						continue;
					}

					if (!players[playerDataIndex].worldPosition) {
						ApplyBonePersistenceFallback(players[playerDataIndex]);
						continue;
					}

					const auto freshBones = Engine::Bones::ParseBonePositionsFromBuffer(
						reinterpret_cast<const uint8_t*>(s_boneBuffers[batchIndex].data()),
						boneBasePos
					);

					players[playerDataIndex].bonePtr = s_bonePtrs[batchIndex];
					if (Engine::Bones::HasRenderableBones(freshBones, players[playerDataIndex].worldPosition)) {
						RecordGoodBones(players[playerDataIndex], freshBones);
					} else {
						ApplyBonePersistenceFallback(players[playerDataIndex]);
					}
				}
			}

			// Players outside this tick's DMA budget still get sticky skeletons.
			RestoreBonesFromPersistence(players);
		}

		void PruneBonePersistence(const std::vector<PlayerData>& players) {
			if (bonePersistence.empty()) {
				return;
			}

			std::unordered_map<int, bool> activeIndices;
			activeIndices.reserve(players.size());
			for (const auto& player : players) {
				if (!EspFilter::IsPlayerAliveForEsp(player)) {
					bonePersistence.erase(player.playerIndex);
					continue;
				}

				activeIndices[player.playerIndex] = true;
			}

			for (auto it = bonePersistence.begin(); it != bonePersistence.end(); ) {
				if (!activeIndices.contains(it->first)) {
					it = bonePersistence.erase(it);
				} else {
					++it;
				}
			}
		}

		struct WorkerViewRefreshResult {
			bool viewValid = false;
			bool positionsRefreshed = false;
		};

		bool UpdateViewSnapshot();

		void RefreshLocalPlayerPosition() {
			if (!DMAInterface::IsValidPointer(localPlayer)) {
				return;
			}

			const auto posPtr = DMAInterface::Read<ULONG64>(localPlayer + Offsets::Player::pos);
			if (posPtr < 0x10000) {
				return;
			}

			Vec3 localPos = DMAInterface::Read<Vec3>(posPtr + 0x80);
			if (localPos) {
				localPlayerPosition = localPos;
			}
		}

		WorkerViewRefreshResult RefreshWorkerViewAndPositions(
			std::vector<PlayerData>& players,
			bool skipPositionRefresh)
		{
			WorkerViewRefreshResult result {};
			result.viewValid = UpdateViewSnapshot();
			if (!result.viewValid) {
				return result;
			}

			RefreshLocalPlayerPosition();

			if (!skipPositionRefresh && !players.empty() && localPlayerPosition) {
				UpdateLivePlayerData(players, visibilityBits, localPlayerPosition);
				result.positionsRefreshed = true;
			}

			return result;
		}

		bool UpdateViewSnapshot() {
			if (!DMAInterface::IsValidPointer(cameraBasePtr)) {
				return false;
			}

			ULONG64 refDefAddress = refDefPtr;
			if (refDefAddress < 0x10000) {
				refDefAddress = DecryptRefDef->GetRefDef();
			}
			if (refDefAddress < 0x10000) {
				return false;
			}

			Vec3 cameraPosition {};
			Vec3 boneBasePos {};
			SDK::ClientBits visibilityBitsRead {};
			RefDef_T refDef {};
			int gameMode = 0;

			auto scatterHandle = mem.GetScatterHandle();
			if (!scatterHandle) {
				return false;
			}

			mem.AddScatterReadRequest(scatterHandle, cameraBasePtr + Offsets::camera_pos, &cameraPosition, sizeof(cameraPosition));
			mem.AddScatterReadRequest(scatterHandle, globals::g_baseAddress + Offsets::game_mode, &gameMode, sizeof(gameMode));
			mem.AddScatterReadRequest(scatterHandle, refDefAddress, &refDef, sizeof(refDef));

			if (DMAInterface::IsValidPointer(clientInfo)) {
				mem.AddScatterReadRequest(scatterHandle, clientInfo + Offsets::Bone::bone_base, &boneBasePos, sizeof(boneBasePos));
				mem.AddScatterReadRequest(scatterHandle, clientInfo + Offsets::o_visible_bit, &visibilityBitsRead, sizeof(visibilityBitsRead));
			}

			mem.ExecuteReadScatter(scatterHandle);

			if (refDef.fov.X == 0.f || refDef.fov.Y == 0.f) {
				refDefPtr = 0;
				refDefAddress = DecryptRefDef->GetRefDef();
				if (refDefAddress < 0x10000) {
					return false;
				}

				scatterHandle = mem.GetScatterHandle();
				if (!scatterHandle) {
					return false;
				}

				mem.AddScatterReadRequest(scatterHandle, refDefAddress, &refDef, sizeof(refDef));
				mem.ExecuteReadScatter(scatterHandle);
			}

			if (refDef.fov.X == 0.f || refDef.fov.Y == 0.f) {
				return false;
			}

			visibilityBits = visibilityBitsRead;
			if (boneBasePos) {
				boneBasePosition = boneBasePos;
			}

			cachedGameMode = gameMode;
			if (gameMode <= 1) {
				refDefPtr = 0;
				return false;
			}

			refDefPtr = refDefAddress;
			workerRefDef = refDef;
			workerCameraPosition = cameraPosition;
			return true;
		}

		void StageUpdateBases() {
			using namespace SDK;

			clientInfo = decrypt_client_info();
			clientBase = decrypt_client_base(clientInfo);

			if (trackedClientBase != 0 && clientBase != trackedClientBase) {
				localPlayerIndex = -1;
				localPlayerTeamIndex = 0;
			Cache::localPlayerTeamIndex = 0;
				bonePersistence.clear();
			}
			if (DMAInterface::IsValidPointer(clientBase)) {
				trackedClientBase = clientBase;
			}

			cameraBasePtr = DMAInterface::Read<ULONG64>(globals::g_baseAddress + Offsets::camera_base);

			if (DMAInterface::IsValidPointer(globals::g_baseAddress)) {
				const ULONG64 freshNameArrayPtr = DMAInterface::Read<ULONG64>(globals::g_baseAddress + Offsets::name_array);
				if (freshNameArrayPtr >= 0x10000) {
					nameArrayPtr = freshNameArrayPtr;
				}
			}

			CacheLocalPlayerIndex();

			if (localPlayerIndex >= 0 && DMAInterface::IsValidPointer(clientBase)) {
				localPlayer = clientBase + (static_cast<ULONG64>(localPlayerIndex) * Offsets::Player::size);
			} else {
				localPlayer = 0;
			}
		}

		void PublishFrameSnapshot(
			const std::vector<PlayerData>& players,
			const ViewSnapshot& view,
			bool updateDebugStats)
		{
			if (players.empty() || !view.inMatch) {
				return;
			}

			PublishedFrameSlot& slot = frameSlots[cacheWriteSlot];
			{
				std::lock_guard<std::mutex> lock(framePublishMutex);
				slot.view = view;
				slot.players = players;
			}

			publishedSlotIndex.store(cacheWriteSlot, std::memory_order_release);
			cacheWriteSlot = (cacheWriteSlot + 1) % 3;

			const auto publishNow = std::chrono::steady_clock::now();
			lastSnapshotPublishTime = publishNow;
			++snapshotPublishCount;
			recentPublishTimes[recentPublishWrite] = publishNow;
			recentPublishWrite = (recentPublishWrite + 1) % recentPublishTimes.size();
			if (recentPublishCount < recentPublishTimes.size()) {
				++recentPublishCount;
			}

			if (updateDebugStats) {
				std::lock_guard<std::mutex> lock(debugStatsMutex);
				publishedDebugStats = latestDebugStats;
				publishedDebugStats.inMatch = view.inMatch;
				publishedDebugStats.snapshotCount = snapshotPublishCount;
			}
		}

		void PublishFreshFrameSnapshot(
			std::vector<PlayerData>& players,
			bool updateDebugStats)
		{
			if (players.empty() || !DMAInterface::IsValidPointer(localPlayer)) {
				return;
			}

			if (!UpdateViewSnapshot()) {
				return;
			}

			RefreshLocalPlayerPosition();
			if (localPlayerPosition) {
				UpdateLivePlayerData(players, visibilityBits, localPlayerPosition);
			}

			ViewSnapshot view {};
			view.refDef = workerRefDef;
			view.cameraPosition = workerCameraPosition;
			view.localPlayerPosition = localPlayerPosition;
			view.inMatch = view.refDef.fov.X != 0.f && view.refDef.fov.Y != 0.f;
			if (!view.inMatch) {
				return;
			}

			PublishFrameSnapshot(players, view, updateDebugStats);
		}

		void CacheLoop() {
			timeBeginPeriod(1);
			std::vector<PlayerData> workingPlayers;
			workingPlayers.reserve(MAX_PLAYERS);

			// Match the ~100Hz budget: rare full discovery, light cam/pos publish first.
			// Do not change UpdateLivePlayerData / PopulatePlayerBones / ESP draw paths.
			constexpr auto FULL_SCAN_INTERVAL = std::chrono::milliseconds(350);
			constexpr auto FAST_UPDATE_INTERVAL = std::chrono::milliseconds(15);
			constexpr auto BONE_REFRESH_INTERVAL = std::chrono::milliseconds(40);
			constexpr auto VIEW_POLL_INTERVAL = std::chrono::milliseconds(6);
			auto lastFullScan = std::chrono::steady_clock::now() - FULL_SCAN_INTERVAL;
			auto lastFastUpdate = std::chrono::steady_clock::now() - FAST_UPDATE_INTERVAL;
			auto lastBoneRefresh = std::chrono::steady_clock::now() - BONE_REFRESH_INTERVAL;
			auto lastBoneBaseDecrypt = std::chrono::steady_clock::time_point {};
			auto lastViewPoll = std::chrono::steady_clock::now() - VIEW_POLL_INTERVAL;
			bool wasInGame = false;

			while (g_cacheRunning.load(std::memory_order_acquire)) {
				const bool inGameNow = Engine::Game::IsInGame();
				if (!inGameNow) {
					wasInGame = false;
					workingPlayers.clear();
					ResetMatchState();
					publishedSlotIndex.store(-1, std::memory_order_release);
					std::this_thread::sleep_for(std::chrono::milliseconds(10));
					continue;
				}

				const auto now = std::chrono::steady_clock::now();
				if (!wasInGame) {
					matchEnteredAt = now;
					matchRediscoveryDone = false;
					wasInGame = true;
				}

				if (!matchRediscoveryDone && matchEnteredAt.has_value()) {
					if ((now - *matchEnteredAt) >= MATCH_REDISCOVERY_DELAY) {
						ForcePlayerRediscovery(workingPlayers);
						matchRediscoveryDone = true;
						lastFullScan = std::chrono::steady_clock::time_point {};
					}
				}

				const bool needsViewPoll =
					!workingPlayers.empty()
					&& (now - lastViewPoll) >= VIEW_POLL_INTERVAL;
				const bool needsFullScan =
					workingPlayers.empty()
					|| (now - lastFullScan) >= FULL_SCAN_INTERVAL
					|| !DMAInterface::IsValidPointer(clientInfo)
					|| !DMAInterface::IsValidPointer(localPlayer);

				const bool needsFastUpdate =
					!needsFullScan && (now - lastFastUpdate) >= FAST_UPDATE_INTERVAL;

				if (!needsViewPoll && !needsFullScan && !needsFastUpdate) {
					std::this_thread::yield();
					continue;
				}

				bool frameDirty = false;
				bool runFullScanDebug = false;
				bool viewOnly = false;

				// Light publish FIRST so full/fast discovery never starves ~8ms cadence.
				if (needsViewPoll) {
					lastViewPoll = now;
					frameDirty = true;
					viewOnly = true;
				} else if (needsFullScan) {
					StageUpdateBases();
					if (!DMAInterface::IsValidPointer(localPlayer)) {
						workingPlayers.clear();
						bonePersistence.clear();
						std::this_thread::yield();
						continue;
					}

					RefreshAllPlayers(MAX_PLAYERS, workingPlayers);
					lastFullScan = now;
					lastFastUpdate = now;
					lastBoneRefresh = now;
					// Do not reset lastViewPoll — light must keep 8ms cadence.
					frameDirty = true;
					runFullScanDebug = true;
				} else if (needsFastUpdate) {
					RefreshPlayerAliveState(workingPlayers);
					lastFastUpdate = now;
					frameDirty = true;
				}

				if (frameDirty && !workingPlayers.empty()) {
					if (!viewOnly) {
						PruneBonePersistence(workingPlayers);
					}

					// Full scan rebuilds PlayerData without bones — restore immediately
					// even if bone DMA is skipped this tick.
					if (!viewOnly && needsFullScan) {
						RestoreBonesFromPersistence(workingPlayers);
					}

					const bool needsBoneRefresh =
						!viewOnly
						&& (Config::ESP::skeleton || Config::Aim::enable)
						&& ((now - lastBoneRefresh) >= BONE_REFRESH_INTERVAL || needsFullScan);

					if (needsBoneRefresh) {
						constexpr auto BONE_BASE_REFRESH_INTERVAL = std::chrono::milliseconds(1000);
						if (!DMAInterface::IsValidPointer(boneBase)
							|| lastBoneBaseDecrypt == std::chrono::steady_clock::time_point {}
							|| (now - lastBoneBaseDecrypt) >= BONE_BASE_REFRESH_INTERVAL)
						{
							boneBase = decrypt_bone_base();
							lastBoneBaseDecrypt = now;
						}

						if (DMAInterface::IsValidPointer(boneBase)) {
							PopulatePlayerBones(workingPlayers);
							lastBoneRefresh = now;
						} else {
							RestoreBonesFromPersistence(workingPlayers);
						}
					}

					PublishFreshFrameSnapshot(workingPlayers, runFullScanDebug);
				}

				std::this_thread::yield();
			}

			timeEndPeriod(1);
		}


		FrameSnapshot ReadPublishedFrameSnapshot() {
			const int slot = publishedSlotIndex.load(std::memory_order_acquire);
			if (slot < 0) {
				return {};
			}

			std::lock_guard<std::mutex> lock(framePublishMutex);
			const PublishedFrameSlot& published = frameSlots[slot];
			if (!published.view.inMatch || published.players.empty()) {
				return {};
			}

			FrameSnapshot frame {};
			frame.view = published.view;
			frame.players = published.players;
			return frame;
		}

		bool ReadPublishedView(ViewSnapshot& outView) {
			const int slot = publishedSlotIndex.load(std::memory_order_acquire);
			if (slot < 0) {
				return false;
			}

			std::lock_guard<std::mutex> lock(framePublishMutex);
			const ViewSnapshot& view = frameSlots[slot].view;
			if (!view.inMatch) {
				return false;
			}

			outView = view;
			return true;
		}
	}

	FrameSnapshot GetFrameSnapshot() {
		return ReadPublishedFrameSnapshot();
	}

	CacheDebugStats GetDebugStats() {
		std::lock_guard<std::mutex> lock(debugStatsMutex);
		CacheDebugStats stats = publishedDebugStats;

		const auto now = std::chrono::steady_clock::now();
		if (lastSnapshotPublishTime.time_since_epoch().count() != 0) {
			stats.dataAgeMs = static_cast<float>(
				std::chrono::duration<double, std::milli>(now - lastSnapshotPublishTime).count());
		}

		if (recentPublishCount >= 2) {
			const size_t oldestIndex =
				(recentPublishWrite + recentPublishTimes.size() - recentPublishCount) % recentPublishTimes.size();
			const auto span = std::chrono::duration<double>(
				recentPublishTimes[(recentPublishWrite + recentPublishTimes.size() - 1) % recentPublishTimes.size()]
				- recentPublishTimes[oldestIndex]).count();
			if (span > 0.0001) {
				stats.snapshotHz = static_cast<float>((recentPublishCount - 1) / span);
			}
		}

		stats.snapshotCount = snapshotPublishCount;
		return stats;
	}

	bool TryReadLiveView(ViewSnapshot& view) {
		return TryReadLiveRenderView(view);
	}

	bool TryReadLiveRenderView(ViewSnapshot& outView) {
		if (!DMAInterface::IsValidPointer(cameraBasePtr)) {
			return false;
		}

		ULONG64 refDefAddress = refDefPtr;
		if (refDefAddress < 0x10000) {
			refDefAddress = DecryptRefDef->GetRefDef();
		}
		if (refDefAddress < 0x10000) {
			return false;
		}

		SDK::Vec3 cameraPosition {};
		SDK::RefDef_T refDef {};

		auto scatterHandle = mem.GetScatterHandle();
		if (!scatterHandle) {
			return false;
		}

		mem.AddScatterReadRequest(
			scatterHandle,
			cameraBasePtr + Offsets::camera_pos,
			&cameraPosition,
			sizeof(cameraPosition));
		mem.AddScatterReadRequest(scatterHandle, refDefAddress, &refDef, sizeof(refDef));
		mem.ExecuteReadScatter(scatterHandle);

		if (refDef.fov.X == 0.f || refDef.fov.Y == 0.f) {
			refDefAddress = DecryptRefDef->GetRefDef();
			if (refDefAddress < 0x10000) {
				return false;
			}

			scatterHandle = mem.GetScatterHandle();
			if (!scatterHandle) {
				return false;
			}

			mem.AddScatterReadRequest(scatterHandle, refDefAddress, &refDef, sizeof(refDef));
			mem.ExecuteReadScatter(scatterHandle);
		}

		if (refDef.fov.X == 0.f || refDef.fov.Y == 0.f) {
			return false;
		}

		outView.refDef = refDef;
		outView.cameraPosition = cameraPosition;
		outView.inMatch = true;
		return true;
	}

	bool RefreshLivePlayerPositions(std::vector<PlayerData>& players) {
		if (players.empty()) {
			return false;
		}

		if (DMAInterface::IsValidPointer(localPlayer)) {
			const auto posPtr = DMAInterface::Read<ULONG64>(localPlayer + Offsets::Player::pos);
			if (posPtr >= 0x10000) {
				const Vec3 localPos = DMAInterface::Read<Vec3>(posPtr + 0x80);
				if (localPos) {
					localPlayerPosition = localPos;
				}
			}
		}

		const SDK::Vec3 localPosition = localPlayerPosition;
		if (!localPosition) {
			return false;
		}

		thread_local std::vector<SDK::Vec3> livePositions;
		thread_local std::vector<size_t> refreshIndices;

		livePositions.assign(players.size(), SDK::Vec3 {});
		refreshIndices.clear();
		refreshIndices.reserve(players.size());

		for (size_t i = 0; i < players.size(); ++i) {
			const auto& player = players[i];
			if (player.posPtr < 0x10000) {
				continue;
			}

			if (!EspFilter::PlayerNeedsLivePositionRefresh(player.distance)) {
				continue;
			}

			refreshIndices.push_back(i);
		}

		if (refreshIndices.empty()) {
			return false;
		}

		auto scatterHandle = mem.GetScatterHandle();
		if (!scatterHandle) {
			return false;
		}

		for (const size_t index : refreshIndices) {
			const auto& player = players[index];
			mem.AddScatterReadRequest(
				scatterHandle,
				player.posPtr + 0x80,
				&livePositions[index],
				sizeof(livePositions[index])
			);
		}

		mem.ExecuteReadScatter(scatterHandle);

		for (const size_t index : refreshIndices) {
			auto& player = players[index];
			const SDK::Vec3& position = livePositions[index];
			if (!position) {
				continue;
			}

			player.worldPosition = position;
			player.distance = localPosition.Distance(position) / 40.f;
		}

		return true;
	}

	void Start() {
		static std::once_flag startOnce;
		std::call_once(startOnce, []() {
			g_cacheRunning.store(true, std::memory_order_release);
			g_cacheThread = std::thread(CacheLoop);
			SetThreadPriority(g_cacheThread.native_handle(), THREAD_PRIORITY_HIGHEST);
		});
	}

	void Stop() {
		g_cacheRunning.store(false, std::memory_order_release);
		if (g_cacheThread.joinable()) {
			g_cacheThread.join();
		}
	}
}
