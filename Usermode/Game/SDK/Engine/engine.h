#pragma once

#include "../../cache.h"

namespace SDK {
    namespace Engine {
        namespace Player {
            ULONG64 GetLocalPlayer();
            int IsPlayerVisible(const UINT& index);
            bool IsPlayerValid(const ULONG64& player);
            UINT GetPlayerIndex(const ULONG64& player);
            ULONG64 GetPlayerFromIndex(const int& index);
            Vec3 GetPlayerPosition(const ULONG64& player);
            UCHAR GetPlayerTeamIndex(const ULONG64& player);
            NameEntry GetPlayerNameEntry(const UINT& index);
            ScoreboardEntry GetPlayerScoreboardEntry(const UINT& index);
            std::string GetPlayerNameFromNameEntry(const NameEntry& nameEntry);
        }

        namespace Game {
            void UpdateOverlayScreenScale(const RefDef_T& refDef);

            bool IsInGame();
            int GetGamePlayerCount();
            Vec2 GetCurrentViewAngles();
            Vec3 GetCurrentCameraPosition();
            Vec2 WorldToScreen(const Vec3& worldPosition);
            Vec2 WorldToScreen(const Vec3& worldPosition, const RefDef_T& refDef, const Vec3& cameraPosition);
            Vec2 WorldToScreenCached(const Vec3& worldPosition, const RefDef_T& refDef, const Vec3& cameraPosition);

            std::expected<std::array<Vec2, BONE_IDX_COUNT>, bool> 
                WorldToScreen(const std::array<Vec3, BONE_IDX_COUNT>& worldPositions);
            std::expected<std::array<Vec2, BONE_IDX_COUNT>, bool>
                WorldToScreen(const std::array<Vec3, BONE_IDX_COUNT>& worldPositions, const RefDef_T& refDef, const Vec3& cameraPosition);
            std::expected<std::array<Vec2, BONE_IDX_COUNT>, bool>
                WorldToScreenCached(const std::array<Vec3, BONE_IDX_COUNT>& worldPositions, const RefDef_T& refDef, const Vec3& cameraPosition);

        }

        namespace Bones {
            bool IsBoneValid(const Vec3& bone, const Vec3& origin);
            bool IsSkeletonDistorted(const std::array<Vec3, BONE_IDX_COUNT>& bones);
            bool HasRenderableBones(const std::array<Vec3, BONE_IDX_COUNT>& bones, const Vec3& origin);
            bool AreBonesValid(const std::array<Vec3, BONE_IDX_COUNT>& bones, const Vec3& origin);
            Vec3 GetBonePosition(const ULONG64& bonePtr, const int& boneId);
            std::array<Vec3, BONE_IDX_COUNT> GetBonePositions(const ULONG64& bonePtr);
            std::array<Vec3, BONE_IDX_COUNT> GetBonePositions(const ULONG64& bonePtr, const Vec3& boneBasePosition);
            std::array<Vec3, BONE_IDX_COUNT> ParseBonePositionsFromBuffer(
                const void* boneDataBuffer,
                const Vec3& boneBasePosition
            );
        }

        namespace Utils {
            std::optional<ULONG64> FindClosestBoneToCentreScreen(const std::array<Vec2, BONE_IDX_COUNT>& bones);
        }
    }
}