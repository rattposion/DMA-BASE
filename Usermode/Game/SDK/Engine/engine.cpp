#include "global.h"

namespace SDK {
	namespace Engine {
		namespace Player {
            ULONG64 GetLocalPlayer() {
                if (DMAInterface::IsValidPointer(Cache::clientInfo)) {
                    const auto localIndexBase = DMAInterface::Read<ULONG64>(Cache::clientInfo + Offsets::local_index);
                    if (DMAInterface::IsValidPointer(localIndexBase)) {
                        auto localIndex = DMAInterface::Read<int>(localIndexBase + Offsets::local_index_pos);
                        return GetPlayerFromIndex(localIndex);
                    }
                }

                return 0;
            }

            int IsPlayerVisible(const UINT& index) {
                auto sightedEnemyFools = DMAInterface::Read<ClientBits>(Cache::clientInfo + Offsets::o_visible_bit);
                const auto bitmask = 0x80000000 >> (index & 0x1F);
                return sightedEnemyFools.array[index >> 5] & bitmask;
            }

            bool IsPlayerValid(const ULONG64& player) {
                return DMAInterface::Read<bool>(player + Offsets::Player::valid);
            }
            
            UINT GetPlayerIndex(const ULONG64& player) {
                if (!DMAInterface::IsValidPointer(Cache::clientInfo)) {
                    return 0;
                }

                return (player - Cache::clientInfo) / Offsets::Player::size;
            }
            
            ULONG64 GetPlayerFromIndex(const int& index) {
                if (DMAInterface::IsValidPointer(Cache::clientInfo)) {
                    if (DMAInterface::IsValidPointer(Cache::clientBase)) {
                        return Cache::clientBase + (index * Offsets::Player::size);
                    }
                }

                return 0;
            }

            Vec3 GetPlayerPosition(const ULONG64& player) {
                const auto playerPositionPtr = DMAInterface::Read<ULONG64>(player + Offsets::Player::pos);
                if (!DMAInterface::IsValidPointer(playerPositionPtr)) {
                    return Vec3();
                }

                return Vec3(DMAInterface::Read<Vec3Mem>(playerPositionPtr + 0x80));
            }

            UCHAR GetPlayerTeamIndex(const ULONG64& player) {
                return DMAInterface::Read<UCHAR>(player + Offsets::Player::team);
            }

            NameEntry GetPlayerNameEntry(const UINT& index) {
                NameEntry playerNameEntry { };

                if (!DMAInterface::IsValidPointer(Cache::nameArrayPtr)) {
                    return playerNameEntry;
                }
                
                playerNameEntry = DMAInterface::Read<NameEntry>(Cache::nameArrayPtr + Offsets::name_array_pos + (index * Offsets::name_array_size));
                return playerNameEntry;
            }

            ScoreboardEntry GetPlayerScoreboardEntry(const UINT& index) {
                if (!DMAInterface::IsValidPointer(Cache::clientInfo)) {
                    return ScoreboardEntry();
                }

                return DMAInterface::Read<ScoreboardEntry>(Cache::clientInfo + Offsets::scoreboard + (index * Offsets::scoreboardsize));
            }

            std::string GetPlayerNameFromNameEntry(const NameEntry& nameEntry) {
                if (!nameEntry.name) {
                    return "Unknown Player";
                }

                const size_t nameLength = strnlen(nameEntry.name, sizeof(nameEntry.name));
                if (nameLength == 0) {
                    return "Unknown Player";
                }

                for (size_t i = 0; i < nameLength; ++i) {
                    const unsigned char uc = static_cast<unsigned char>(nameEntry.name[i]);
                    const bool isValidChar = isprint(uc) && uc != '\t' && uc != '\n' && uc != '\r';

                    if (!isValidChar) {
                        return "Player";
                    }
                }

                return std::string(nameEntry.name, nameLength);
            }
		} // namespace Player

		namespace Game {
            namespace {
                struct OverlayScreenCache {
                    float scaleX = 1.f;
                    float scaleY = 1.f;
                    float gameWidth = 0.f;
                    float gameHeight = 0.f;
                    bool valid = false;
                };

                thread_local OverlayScreenCache g_overlayScreenCache {};

                void RefreshOverlayScreenCache(const RefDef_T& refDef) {
                    g_overlayScreenCache.gameWidth = refDef.width > 0
                        ? static_cast<float>(refDef.width)
                        : static_cast<float>(Render::g_screenWidth);
                    g_overlayScreenCache.gameHeight = refDef.height > 0
                        ? static_cast<float>(refDef.height)
                        : static_cast<float>(Render::g_screenHeight);

                    if (g_overlayScreenCache.gameWidth <= 0.f
                        || g_overlayScreenCache.gameHeight <= 0.f
                        || Render::g_screenWidth <= 0
                        || Render::g_screenHeight <= 0)
                    {
                        g_overlayScreenCache.valid = false;
                        return;
                    }

                    g_overlayScreenCache.scaleX =
                        static_cast<float>(Render::g_screenWidth) / g_overlayScreenCache.gameWidth;
                    g_overlayScreenCache.scaleY =
                        static_cast<float>(Render::g_screenHeight) / g_overlayScreenCache.gameHeight;
                    g_overlayScreenCache.valid = true;
                }

                bool ProjectWorldToGameScreen(
                    const Vec3& worldPosition,
                    const RefDef_T& refDef,
                    const Vec3& cameraPosition,
                    Vec2& outScreen)
                {
                    if (refDef.fov.X == 0.f || refDef.fov.Y == 0.f) {
                        return false;
                    }

                    const float gameWidth = g_overlayScreenCache.valid
                        ? g_overlayScreenCache.gameWidth
                        : (refDef.width > 0
                            ? static_cast<float>(refDef.width)
                            : static_cast<float>(Render::g_screenWidth));
                    const float gameHeight = g_overlayScreenCache.valid
                        ? g_overlayScreenCache.gameHeight
                        : (refDef.height > 0
                            ? static_cast<float>(refDef.height)
                            : static_cast<float>(Render::g_screenHeight));

                const auto local = worldPosition - cameraPosition;
                const float transformedX = local.Dot(refDef.axis[RIGHT_VEC]);
                const float transformedY = local.Dot(refDef.axis[UP_VEC]);
                const float transformedZ = local.Dot(refDef.axis[FORWARD_VEC]);

                if (transformedZ < 0.01f) {
                    return false;
                }

                const auto zReciprocal = 1.0f / transformedZ;
                outScreen.X = (gameWidth * 0.5f) * (1.0f - (transformedX / refDef.fov.X * zReciprocal));
                outScreen.Y = (gameHeight * 0.5f) * (1.0f - (transformedY / refDef.fov.Y * zReciprocal));
                    return std::isfinite(outScreen.X) && std::isfinite(outScreen.Y);
                }

                Vec2 MapWorldToOverlayScreen(
                    const Vec3& worldPosition,
                    const RefDef_T& refDef,
                    const Vec3& cameraPosition)
                {
                    Vec2 gameScreen {};
                    if (!ProjectWorldToGameScreen(worldPosition, refDef, cameraPosition, gameScreen)) {
                        return {};
                    }

                    if (g_overlayScreenCache.valid) {
                        return {
                            gameScreen.X * g_overlayScreenCache.scaleX,
                            gameScreen.Y * g_overlayScreenCache.scaleY
                        };
                    }

                    const float gameWidth = refDef.width > 0
                        ? static_cast<float>(refDef.width)
                        : static_cast<float>(Render::g_screenWidth);
                    const float gameHeight = refDef.height > 0
                        ? static_cast<float>(refDef.height)
                        : static_cast<float>(Render::g_screenHeight);

                    if (gameWidth <= 0.f || gameHeight <= 0.f) {
                        return {};
                    }

                    return {
                        gameScreen.X * (static_cast<float>(Render::g_screenWidth) / gameWidth),
                        gameScreen.Y * (static_cast<float>(Render::g_screenHeight) / gameHeight)
                    };
                }
            }

            void UpdateOverlayScreenScale(const RefDef_T& refDef) {
                RefreshOverlayScreenCache(refDef);
            }

            bool IsInGame() {
                return (bool)((DMAInterface::Read<int>(globals::g_baseAddress + Offsets::game_mode)) > 1);
            }

            int GetGamePlayerCount() {
                return DMAInterface::Read<int>(globals::g_baseAddress + Offsets::game_mode);
            }

            Vec2 GetCurrentViewAngles() {
                if (!DMAInterface::IsValidPointer(Cache::cameraBasePtr)) {
                    return Vec2();
                }

                return DMAInterface::Read<Vec2>(Cache::cameraBasePtr + Offsets::camera_pos + 0xC);
            }
            
            Vec3 GetCurrentCameraPosition() {
                if (!DMAInterface::IsValidPointer(Cache::cameraBasePtr)) {
                    return Vec3();
                }

                return Vec3(DMAInterface::Read<Vec3Mem>(Cache::cameraBasePtr + Offsets::camera_pos));
            }

            Vec2 WorldToScreen(const Vec3& worldPosition) {
                return MapWorldToOverlayScreen(worldPosition, DecryptRefDef->ref_def_nn, Cache::cameraPosition);
            }

            Vec2 WorldToScreen(const Vec3& worldPosition, const RefDef_T& refDef, const Vec3& cameraPosition) {
                return MapWorldToOverlayScreen(worldPosition, refDef, cameraPosition);
            }

            Vec2 WorldToScreenCached(const Vec3& worldPosition, const RefDef_T& refDef, const Vec3& cameraPosition) {
                if (refDef.fov.X == 0.f || refDef.fov.Y == 0.f) {
                    return {};
                }

                const float gameWidth = g_overlayScreenCache.valid
                    ? g_overlayScreenCache.gameWidth
                    : (refDef.width > 0
                        ? static_cast<float>(refDef.width)
                        : static_cast<float>(Render::g_screenWidth));
                const float gameHeight = g_overlayScreenCache.valid
                    ? g_overlayScreenCache.gameHeight
                    : (refDef.height > 0
                        ? static_cast<float>(refDef.height)
                        : static_cast<float>(Render::g_screenHeight));

                if (gameWidth <= 0.f || gameHeight <= 0.f) {
                    return {};
                }

                const auto local = worldPosition - cameraPosition;
                const float transformedX = local.Dot(refDef.axis[RIGHT_VEC]);
                const float transformedY = local.Dot(refDef.axis[UP_VEC]);
                const float transformedZ = local.Dot(refDef.axis[FORWARD_VEC]);

                if (transformedZ < 0.01f) {
                    return {};
                }

                const auto zReciprocal = 1.0f / transformedZ;
                const Vec2 gameScreen {
                    (gameWidth * 0.5f) * (1.0f - (transformedX / refDef.fov.X * zReciprocal)),
                    (gameHeight * 0.5f) * (1.0f - (transformedY / refDef.fov.Y * zReciprocal))
                };

                if (!std::isfinite(gameScreen.X) || !std::isfinite(gameScreen.Y)) {
                    return {};
                }

                if (g_overlayScreenCache.valid) {
                    return {
                        gameScreen.X * g_overlayScreenCache.scaleX,
                        gameScreen.Y * g_overlayScreenCache.scaleY
                    };
                }

                return {
                    gameScreen.X * (static_cast<float>(Render::g_screenWidth) / gameWidth),
                    gameScreen.Y * (static_cast<float>(Render::g_screenHeight) / gameHeight)
                };
            }

            std::expected<std::array<Vec2, BONE_IDX_COUNT>, bool> WorldToScreen(
                const std::array<Vec3, BONE_IDX_COUNT>& worldPositions,
                const RefDef_T& refDef,
                const Vec3& cameraPosition)
            {
                std::array<Vec2, BONE_IDX_COUNT> screenPositions { };
                int projectedCount = 0;

                for (size_t i = 0; i < worldPositions.size(); ++i) {
                    screenPositions[i] = WorldToScreen(worldPositions[i], refDef, cameraPosition);
                    if (screenPositions[i]) {
                        projectedCount++;
                    }
                }

                if (projectedCount < 2) {
                    return std::unexpected(false);
                }

                return screenPositions;
            }

            std::expected<std::array<Vec2, BONE_IDX_COUNT>, bool> WorldToScreen(const std::array<Vec3, BONE_IDX_COUNT>& worldPositions) {
                return WorldToScreen(worldPositions, DecryptRefDef->ref_def_nn, Cache::cameraPosition);
            }

            std::expected<std::array<Vec2, BONE_IDX_COUNT>, bool> WorldToScreenCached(
                const std::array<Vec3, BONE_IDX_COUNT>& worldPositions,
                const RefDef_T& refDef,
                const Vec3& cameraPosition)
            {
                std::array<Vec2, BONE_IDX_COUNT> screenPositions {};
                int projectedCount = 0;

                for (size_t i = 0; i < worldPositions.size(); ++i) {
                    screenPositions[i] = WorldToScreenCached(worldPositions[i], refDef, cameraPosition);
                    if (screenPositions[i]) {
                        projectedCount++;
                    }
                }

                if (projectedCount < 2) {
                    return std::unexpected(false);
                }

                return screenPositions;
            }
		} // namespace Game

		namespace Bones {
            bool IsBoneValid(const Vec3& bone, const Vec3& origin) {
                return origin.DistanceTo(bone) <= 500;
            }

            bool IsSkeletonDistorted(const std::array<Vec3, BONE_IDX_COUNT>& bones) {
                const auto& head = bones[BONE_IDX_HEAD];
                const auto& pelvis = bones[BONE_IDX_PELVIS];

                if (head.IsZero() && pelvis.IsZero()) {
                    return true;
                }

                const float dx = head.X - pelvis.X;
                const float dy = head.Y - pelvis.Y;
                const float dz = head.Z - pelvis.Z;
                const float spineLength = std::sqrtf((dx * dx) + (dy * dy) + (dz * dz));

                // Match FUSER span — allow prone/crouch, reject only garbage.
                return spineLength > 120.f || spineLength < 12.f;
            }

            bool HasRenderableBones(const std::array<Vec3, BONE_IDX_COUNT>& bones, const Vec3& origin) {
                (void)origin;

                if (IsSkeletonDistorted(bones)) {
                    return false;
                }

                const auto& head = bones[BONE_IDX_HEAD];
                const auto& pelvis = bones[BONE_IDX_PELVIS];

                return std::isfinite(head.X) && std::isfinite(head.Y) && std::isfinite(head.Z)
                    && std::isfinite(pelvis.X) && std::isfinite(pelvis.Y) && std::isfinite(pelvis.Z)
                    && (!head.IsZero() || !pelvis.IsZero());
            }

            static bool ValidateBoneChain(
                const std::array<Vec3, BONE_IDX_COUNT>& bones, 
                std::initializer_list<int> chainIndices, 
                const Vec3& origin) {
                std::vector<int> indices(chainIndices);
                
                for (ULONG64 i = 0; i < indices.size() - 1; ++i) {
                    const auto& bone1 = bones[indices[i]];
                    const auto& bone2 = bones[indices[i + 1]];

                    if (bone1.DistanceTo(bone2) >= 40 && origin.DistanceTo(bone1) >= 500) {
                        return false;
                    }
                }

                return true;
            }

            bool AreBonesValid(const std::array<Vec3, BONE_IDX_COUNT>& bones, const Vec3& origin) {
                if (bones.empty()) {
                    return false;
                }

                if (!HasRenderableBones(bones, origin)) {
                    return false;
                }

                if (!ValidateBoneChain(bones, { 0, 1, 2, 3, 4, 5 }, origin))
                    return false;

                if (!ValidateBoneChain(bones, { 5, 14, 15, 16, 17 }, origin))
                    return false;

                if (!ValidateBoneChain(bones, { 5, 6, 7, 8, 9 }, origin))
                    return false;

                if (!ValidateBoneChain(bones, { 3, 10, 11, 12, 13 }, origin))
                    return false;

                return true;
            }

            // TODO, try read whole bone at once, iterate that memory for bones we want.
            Vec3 GetBonePosition(const ULONG64& bonePtr, const int& boneId) {
                if (!DMAInterface::IsValidPointer(Cache::boneBase) || !DMAInterface::IsValidPointer(Cache::clientInfo)) {
                    return Vec3();
                }

                auto bonePosition = Vec3(DMAInterface::Read<Vec3Mem>(bonePtr + (boneId * 0x20) + 0x10));
                if (!bonePosition) {
                    return Vec3();
                }

                bonePosition += Cache::boneBasePosition;
                return bonePosition;
            }

            constexpr int maxBoneId = BONE_POS_RIGHT_FOOT_4;
            constexpr ULONG64 boneDataReadSize = (maxBoneId + 1) * 0x20;

            constexpr std::array<int, BONE_IDX_COUNT> kDesiredBoneIds = {
                BONE_POS_HEAD, BONE_POS_NECK, BONE_POS_CHEST, BONE_POS_MID, BONE_POS_TUMMY, BONE_POS_PELVIS,

                BONE_POS_LEFT_FOOT_1, BONE_POS_LEFT_FOOT_2, BONE_POS_LEFT_FOOT_3, BONE_POS_LEFT_FOOT_4,
                BONE_POS_LEFT_HAND_1, BONE_POS_LEFT_HAND_2, BONE_POS_LEFT_HAND_3, BONE_POS_LEFT_HAND_4,

                BONE_POS_RIGHT_FOOT_1, BONE_POS_RIGHT_FOOT_2, BONE_POS_RIGHT_FOOT_3, BONE_POS_RIGHT_FOOT_4,
                BONE_POS_RIGHT_HAND_1, BONE_POS_RIGHT_HAND_2, BONE_POS_RIGHT_HAND_3, BONE_POS_RIGHT_HAND_4
            };

            std::array<Vec3, BONE_IDX_COUNT> ParseBonePositionsFromBuffer(
                const void* boneDataBuffer,
                const Vec3& boneBasePosition
            ) {
                std::array<Vec3, BONE_IDX_COUNT> bonePositions { };
                if (!boneDataBuffer) {
                    return bonePositions;
                }

                const auto* bytes = static_cast<const char*>(boneDataBuffer);
                for (ULONG64 i = 0; i < BONE_IDX_COUNT; ++i) {
                    const int boneId = kDesiredBoneIds[i];
                    const auto offset = static_cast<ULONG64>(boneId) * 0x20 + 0x10;
                    const Vec3Mem rawBone = *reinterpret_cast<const Vec3Mem*>(bytes + offset);
                    Vec3 bonePosition(rawBone);
                    bonePosition += boneBasePosition;
                    bonePositions[i] = bonePosition;
                }

                return bonePositions;
            }

            std::array<Vec3, BONE_IDX_COUNT> GetBonePositions(const ULONG64& bonePtr, const Vec3& boneBasePosition) {
                std::array<Vec3, BONE_IDX_COUNT> bonePositions { };
                if (!DMAInterface::IsLikelyValidPointer(bonePtr)) {
                    return bonePositions;
                }

                std::array<char, boneDataReadSize> boneDataBuffer { };
                DMAInterface::ReadMemory(bonePtr, boneDataBuffer.data(), boneDataBuffer.size());
                return ParseBonePositionsFromBuffer(boneDataBuffer.data(), boneBasePosition);
            }

            std::array<Vec3, BONE_IDX_COUNT> GetBonePositions(const ULONG64& bonePtr) {
                return GetBonePositions(bonePtr, Cache::boneBasePosition);
            }
		} // namespace Bones

        namespace Utils {
            std::optional<ULONG64> FindClosestBoneToCentreScreen(const std::array<Vec2, BONE_IDX_COUNT>& bones) {
                const auto screenCentre = Vec2(
                    static_cast<float>(Render::g_halfScreenWidth),
                    static_cast<float>(Render::g_halfScreenHeight)
                );

                float minDistanceSqrd = FLT_MAX;
                std::optional<ULONG64> closestBoneIdx = std::nullopt;

                for (ULONG64 i = 0; i < bones.size(); ++i) {
                    const auto& bonePosition = bones[i];

                    float dx = bonePosition.X - screenCentre.X;
                    float dy = bonePosition.Y - screenCentre.Y;
                    float distanceSqrd = (dx * dx) + (dy * dy);

                    if (distanceSqrd < minDistanceSqrd) {
                        minDistanceSqrd = distanceSqrd;
                        closestBoneIdx = i;
                    }
                }

                return closestBoneIdx;
            }
        } // namespace Utils
	}
}


