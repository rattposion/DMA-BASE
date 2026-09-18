#include "global.h"

using namespace SDK;
void Loop::RunLoop() {
	Vec2 closestScreenPosition = Vec2();
	float closestDistance = FLT_MAX;

	FrameSnapshot frame = Cache::GetFrameSnapshot();
	if (!frame.view.inMatch || frame.players.empty()) {
		return;
	}

	// Snapshot only — no render-thread DMA. Live cam/pos here fights the cache
	// worker and tanks ESP Hz + Draw (was ~60/70). Cache light path already
	// publishes cam+feet together every ~8ms.
	const ViewSnapshot& renderView = frame.view;
	const auto& refDef = renderView.refDef;
	const auto& cameraPos = renderView.cameraPosition;

	DecryptRefDef->ref_def_nn = refDef;
	Cache::cameraPosition = cameraPos;
	Engine::Game::UpdateOverlayScreenScale(refDef);

	const std::vector<PlayerData>& livePlayers = frame.players;

	for (const auto& player : livePlayers) {
		const auto& playerWorldPosition = player.worldPosition;
		if (!std::isfinite(playerWorldPosition.X)
			|| !std::isfinite(playerWorldPosition.Y)
			|| !std::isfinite(playerWorldPosition.Z)) {
			continue;
		}
		
		if (Config::ESP::indicator) {
			ESP::DrawIndicator(
				playerWorldPosition,
				refDef.axis,
				player.distance
			);
		}

		if (player.distance <= Config::ESP::maxESPDistance) {
			auto playerHeadWorldPosition = playerWorldPosition;
			playerHeadWorldPosition.Z += 60.f;

			const auto playerScreenPosition = Engine::Game::WorldToScreenCached(playerWorldPosition, refDef, cameraPos);
			const auto playerHeadScreenPosition = Engine::Game::WorldToScreenCached(playerHeadWorldPosition, refDef, cameraPos);
			if (!playerScreenPosition || !playerHeadScreenPosition) {
				continue;
			}

			const auto playerBoxHeight = std::fabsf(playerHeadScreenPosition.Y - playerScreenPosition.Y);
			const auto playerBoxWidth = playerBoxHeight * 0.55f;
			const auto playerBoxX = playerHeadScreenPosition.X - (playerBoxWidth / 2.f);
			const auto playerBoxY = playerHeadScreenPosition.Y;

			if (Config::ESP::box) {
				if (Config::ESP::boxFilled) {
					ESP::DrawFilledBox(
						playerBoxX,
						playerBoxY,
						playerBoxWidth, playerBoxHeight,
						player.isVisible ? Config::ESP::boxVisibleOutlineColor : Config::ESP::boxInvisibleOutlineColor,
						player.isVisible ? Config::ESP::boxVisibleFilledColor : Config::ESP::boxInvisibleFilledColor
					);
				} else {
					ESP::DrawBox(
						playerBoxX,
						playerBoxY,
						playerBoxWidth, playerBoxHeight,
						player.isVisible ? Config::ESP::boxVisibleOutlineColor : Config::ESP::boxInvisibleOutlineColor
					);
				}
			}

			if (Config::ESP::nickname) {
				ESP::DrawNickname(
					playerBoxX,
					playerBoxY,
					playerBoxWidth,
					player.playerName
				);
			}

			if (Config::ESP::distance) {
				ESP::DrawDistance(
					playerBoxX,
					playerBoxY,
					playerBoxWidth,
					playerBoxHeight,
					player.distance
				);
			}
			if (Config::ESP::health) {
				const float minThickness = 3.0f;
				const float baseThickness = 5.0f;
				const float minScalingDistance = 50.0f;
				const float maxScalingDistance = 250.0f;

				float scaledBarThickness = baseThickness;

				if (player.distance > minScalingDistance) {
					float clampedDistance = std::min(player.distance, maxScalingDistance);
					float distanceT = (clampedDistance - minScalingDistance) / (maxScalingDistance - minScalingDistance);
					scaledBarThickness = baseThickness - distanceT * (baseThickness - minThickness);
				}

				ESP::DrawHealthBar(
					player.playerIndex,
					playerBoxX,
					playerBoxY,
					playerBoxWidth, playerBoxHeight,
					player.playerHealth,
					scaledBarThickness
				);
			}
		}

		const bool needsBones =
			player.hasValidBones
			&& (
				(Config::ESP::skeleton && player.distance <= Config::ESP::maxESPDistance)
				|| (Config::Aim::enable && player.distance <= Config::Aim::maxAimbotDistance)
			);
		if (!needsBones) {
			continue;
		}

		const Vec3 positionDelta = playerWorldPosition - player.skeletonSnapshotRoot;

		std::array<Vec3, BONE_IDX_COUNT> correctedBones = player.bones;
		for (int i = 0; i < BONE_IDX_COUNT; ++i) {
			correctedBones[i] = player.bones[i] + positionDelta;
		}

		auto boneScreenPositionsResult = Engine::Game::WorldToScreenCached(
			correctedBones,
			refDef,
			cameraPos
		);
		if (!boneScreenPositionsResult) {
			continue;
		}

		const auto& playerBoneScreenPositions = *boneScreenPositionsResult;
		if (Config::ESP::skeleton && (player.distance <= Config::ESP::maxESPDistance)) {
			ESP::DrawSkeleton(
				playerBoneScreenPositions,
				correctedBones,
				renderView.localPlayerPosition,
				player.isVisible ? Config::ESP::skeletonVisibleColor : Config::ESP::skeletonInvisibleColor
			);
		}
	
		if (Config::Aim::enable) {
			if (player.distance <= Config::Aim::maxAimbotDistance) {

				if (Config::Aim::visibleCheck && !player.isVisible) {
					continue;
				}

				Vec2 targetBonePosition = Vec2();
				switch (Config::Aim::aimbone) {
					case 0: { // head
						targetBonePosition = playerBoneScreenPositions[BONE_IDX_HEAD];
						break;
					}

					case 1: { // neck
						targetBonePosition = playerBoneScreenPositions[BONE_IDX_NECK];
						break;
					}

					case 2: { // chest
						targetBonePosition = playerBoneScreenPositions[BONE_IDX_CHEST];
						break;
					}

					case 3: { // pelvis
						targetBonePosition = playerBoneScreenPositions[BONE_IDX_PELVIS];
						break;
					}

					case 4: { // smart
						const auto closestBoneOpt = Engine::Utils::FindClosestBoneToCentreScreen(playerBoneScreenPositions);
						if (closestBoneOpt.has_value()) {
							targetBonePosition = playerBoneScreenPositions[*closestBoneOpt];
						}

						break;
					}
				}

				if (!targetBonePosition.Empty()) {
					const auto dx = targetBonePosition.X - Render::g_halfScreenWidth;
					const auto dy = targetBonePosition.Y - Render::g_halfScreenHeight;
					const auto distSqrd = std::sqrtf((dx * dx) + (dy * dy));

					if (distSqrd < Config::Aim::FOV && distSqrd < closestDistance) {
						closestDistance = distSqrd;
						closestScreenPosition = targetBonePosition;
					}
				}
			}
		}
	}

	if (Config::Aim::enable) {
		if (Config::Aim::showFOV) {
			ImGui::GetBackgroundDrawList()->AddCircle(
				ImVec2(Render::g_halfScreenWidth, Render::g_halfScreenHeight),
				Config::Aim::FOV, 
				IM_COL32_WHITE, 
				200, 
				2.f
			);
		}

		if (closestScreenPosition) {
			if (Config::Aim::targetLine) {
				ImGui::GetBackgroundDrawList()->AddLine(
					ImVec2(Render::g_halfScreenWidth, Render::g_halfScreenHeight),
					closestScreenPosition,
					ImColor(Config::Aim::targetLineColor)
				);
			}

			Aimbot::AimTo(closestScreenPosition);
		}
	}
}
