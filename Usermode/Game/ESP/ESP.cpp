#include "global.h"

namespace ESP {
	void DrawBox(const int& x, const int& y, const int& w, const int& h, const ImColor& color) {
		auto drawList = ImGui::GetBackgroundDrawList();

		if (Config::ESP::boxType == 0) {
			drawList->AddRect(ImVec2(x + 1, y + 1), ImVec2(((x + w) - 1), ((y + h) - 1)), color);
			drawList->AddRect(ImVec2(x, y), ImVec2(x + w, y + h), color);
		} else {
			DrawLine(x, y, x, y + (h / 3.5), color);
			DrawLine(x, y, x + (w / 3.5), y, color);
			DrawLine(x + w, y, x + w - (w / 3.5), y, color);
			DrawLine(x + w, y, x + w, y + (h / 3.5), color);
			DrawLine(x, y + h, x + (w / 3.5), y + h, color);
			DrawLine(x, y + h, x, y + h - (h / 3.5), color);
			DrawLine(x + w, y + h, x + w - (w / 3.5), y + h, color);
			DrawLine(x + w, y + h, x + w, y + h - (h / 3.5), color);
		}
	}

	void DrawFilledBox(const int& x, const int& y, const int& w, const int& h, const ImColor& color, const ImColor& filledColor) {
		auto drawList = ImGui::GetBackgroundDrawList();

		drawList->AddRectFilled(ImVec2(x, y), ImVec2(x + w, y + h), filledColor);

		if (Config::ESP::boxType == 0) {
			drawList->AddRect(ImVec2(x + 1, y + 1), ImVec2(((x + w) - 1), ((y + h) - 1)), color);
			drawList->AddRect(ImVec2(x, y), ImVec2(x + w, y + h), color);
		} else {
			DrawLine(x, y, x, y + (h / 3.5), color);
			DrawLine(x, y, x + (w / 3.5), y, color);
			DrawLine(x + w, y, x + w - (w / 3.5), y, color);
			DrawLine(x + w, y, x + w, y + (h / 3.5), color);
			DrawLine(x, y + h, x + (w / 3.5), y + h, color);
			DrawLine(x, y + h, x, y + h - (h / 3.5), color);
			DrawLine(x + w, y + h, x + w - (w / 3.5), y + h, color);
			DrawLine(x + w, y + h, x + w, y + h - (h / 3.5), color);
		}
	}

	static const auto lowHealthColor = ImColor(237, 64, 64);	// red
	static const auto midHealthColor = ImColor(222, 170, 40);	// yellow
	static const auto fullHealthColor = ImColor(64, 237, 81);	// green
	static ImColor GetHealthColor(float percentage) {
		if (percentage > 0.5f) { // green -> yellow
			// remap from [0.5, 1.0] to [0.0, 1.0]
			auto t = (percentage - 0.5f) * 2.f;
			return ImColor(
				midHealthColor.Value.x + (fullHealthColor.Value.x - midHealthColor.Value.x) * t,
				midHealthColor.Value.y + (fullHealthColor.Value.y - midHealthColor.Value.y) * t,
				midHealthColor.Value.z + (fullHealthColor.Value.z - midHealthColor.Value.z) * t
			);
		} else { // yellow -> red
			auto t = percentage * 2.f;
			return ImColor(
				lowHealthColor.Value.x + (midHealthColor.Value.x - lowHealthColor.Value.x) * t,
				lowHealthColor.Value.y + (midHealthColor.Value.y - lowHealthColor.Value.y) * t,
				lowHealthColor.Value.z + (midHealthColor.Value.z - lowHealthColor.Value.z) * t
			);
		}
	}

	static std::map<int, float> healthMap;
	void DrawHealthBar(int id, float x, float y, float w, float h, int health, float barThickness) {
		auto drawList = ImGui::GetBackgroundDrawList();
		const auto position = static_cast<HealthBarPosition>(Config::ESP::healthBarPos);

		const float padding = 3.f;

		const int clampedHealth = std::min(std::max(health, 0), 127);
		const float healthPercentage = static_cast<float>(clampedHealth) / 127.0f;

		if (healthMap.find(id) == healthMap.end()) {
			healthMap[id] = healthPercentage;
		}
		float currentVisualPercentage = healthMap[id];
		const float lerpFactor = 8.0f * ImGui::GetIO().DeltaTime;
		currentVisualPercentage += (healthPercentage - currentVisualPercentage) * lerpFactor;

		healthMap[id] = currentVisualPercentage;

		if (std::fabs(healthPercentage - currentVisualPercentage) < 0.001f) {
			currentVisualPercentage = healthPercentage;
		}

		ImVec2 barPos, barSize;
		switch (position) {
			case HealthBarPosition::Left: {
				barPos = ImVec2(x - barThickness - padding, y);
				barSize = ImVec2(barThickness, h);
				break;
			}

			case HealthBarPosition::Right: {
				barPos = ImVec2(x + w + padding, y);
				barSize = ImVec2(barThickness, h);
				break;
			}

			case HealthBarPosition::Top: {
				barPos = ImVec2(x, y - barThickness - padding);
				barSize = ImVec2(w, barThickness);
				break;
			}

			case HealthBarPosition::Bottom: {
				barPos = ImVec2(x, y + h + padding);
				barSize = ImVec2(w, barThickness);
				break;
			}
		}

		// background 
		drawList->AddRectFilled(barPos, ImVec2(barPos.x + barSize.x, barPos.y + barSize.y), IM_COL32(0.f, 0.f, 0.f, 120.f));

		if (currentVisualPercentage > 0.f) {
			auto mainColor = GetHealthColor(currentVisualPercentage);
			auto darkColor = ImColor(mainColor.Value.x * 0.6f, mainColor.Value.y * 0.6f, mainColor.Value.z * 0.6f);

			if (position == HealthBarPosition::Left || position == HealthBarPosition::Right) {
				auto filledHeight = barSize.y * currentVisualPercentage;
				auto barTopY = barPos.y + (barSize.y - filledHeight);
				drawList->AddRectFilledMultiColor(
					ImVec2(barPos.x, barTopY),
					ImVec2(barPos.x + barSize.x, barPos.y + barSize.y),
					mainColor, mainColor,
					darkColor, darkColor
				);
			} else {
				auto filledWidth = barSize.x * currentVisualPercentage;
				drawList->AddRectFilledMultiColor(
					ImVec2(barPos.x, barPos.y),
					ImVec2(barPos.x + filledWidth, barPos.y + barSize.y),
					mainColor, darkColor,
					darkColor, mainColor
				);
			}
		}

		// outline 
		drawList->AddRect(barPos, ImVec2(barPos.x + barSize.x, barPos.y + barSize.y), IM_COL32(0.f, 0.f, 0.f, 255.f));
	}

	void DrawSkeleton(
		const std::array<SDK::Vec2, SDK::BONE_IDX_COUNT>& screenBones,
		const std::array<SDK::Vec3, SDK::BONE_IDX_COUNT>& worldBones,
		const SDK::Vec3& origin,
		const ImColor& color
	) {
		using namespace SDK;

		constexpr float kMaxSegmentLength = 40.f;
		constexpr float kMinOriginDistance = 500.f;
		constexpr size_t kMaxConnections = 21;

		auto isChainValid =
			[&](std::initializer_list<PlayerBoneIdx> chain) -> bool {
			const auto indices = chain;
			const auto* begin = indices.begin();
			const auto count = indices.size();
			if (count < 2) {
				return false;
			}

			for (size_t i = 0; i + 1 < count; ++i) {
				const auto& bone1 = worldBones[begin[i]];
				const auto& bone2 = worldBones[begin[i + 1]];

				if (bone1.DistanceTo(bone2) >= kMaxSegmentLength
					&& origin.DistanceTo(bone1) >= kMinOriginDistance) {
					return false;
				}
			}

			return true;
		};

		std::array<std::pair<PlayerBoneIdx, PlayerBoneIdx>, kMaxConnections> connections {};
		size_t connectionCount = 0;

		auto addChain =
			[&](std::initializer_list<PlayerBoneIdx> chain) {
			if (!isChainValid(chain)) {
				return;
			}

			const auto indices = chain;
			const auto* begin = indices.begin();
			const auto count = indices.size();
			for (size_t i = 0; i + 1 < count; ++i) {
				const auto& startPos = screenBones[begin[i]];
				const auto& endPos = screenBones[begin[i + 1]];

				if ((startPos.X == 0.f && startPos.Y == 0.f)
					|| (endPos.X == 0.f && endPos.Y == 0.f)) {
					continue;
				}

				const float dx = endPos.X - startPos.X;
				const float dy = endPos.Y - startPos.Y;
				if ((dx * dx) + (dy * dy) < 4.f) {
					continue;
				}

				if (connectionCount < connections.size()) {
					connections[connectionCount++] = { begin[i], begin[i + 1] };
				}
			}
			};

		addChain({ BONE_IDX_HEAD, BONE_IDX_NECK, BONE_IDX_CHEST, BONE_IDX_MID, BONE_IDX_TUMMY, BONE_IDX_PELVIS });
		addChain({ BONE_IDX_MID, BONE_IDX_LEFT_HAND_1, BONE_IDX_LEFT_HAND_2, BONE_IDX_LEFT_HAND_3, BONE_IDX_LEFT_HAND_4 });
		addChain({ BONE_IDX_MID, BONE_IDX_RIGHT_HAND_1, BONE_IDX_RIGHT_HAND_2, BONE_IDX_RIGHT_HAND_3, BONE_IDX_RIGHT_HAND_4 });
		addChain({ BONE_IDX_PELVIS, BONE_IDX_LEFT_FOOT_1, BONE_IDX_LEFT_FOOT_2, BONE_IDX_LEFT_FOOT_3, BONE_IDX_LEFT_FOOT_4 });
		addChain({ BONE_IDX_PELVIS, BONE_IDX_RIGHT_FOOT_1, BONE_IDX_RIGHT_FOOT_2, BONE_IDX_RIGHT_FOOT_3, BONE_IDX_RIGHT_FOOT_4 });

		auto* drawList = ImGui::GetBackgroundDrawList();
		if (!drawList || connectionCount == 0) {
			return;
		}

		const ImVec4 core = color.Value;
		const ImU32 coreColor = ImGui::ColorConvertFloat4ToU32(core);
		const ImU32 shadowColor = ImGui::ColorConvertFloat4ToU32(
			ImVec4(0.f, 0.f, 0.f, std::clamp(core.w * 0.55f, 0.f, 1.f))
		);

		for (size_t i = 0; i < connectionCount; ++i) {
			const auto& [startIndex, endIndex] = connections[i];
			const auto& start = screenBones[startIndex];
			const auto& end = screenBones[endIndex];
			const ImVec2 p1(start.X, start.Y);
			const ImVec2 p2(end.X, end.Y);

			drawList->AddLine(p1, p2, shadowColor, 2.5f);
			drawList->AddLine(p1, p2, coreColor, 1.5f);
		}
	}

	constexpr float maxAlpha = 1.0f;
	constexpr float minAlpha = 0.3f;
	constexpr float maxDistForAlpha = 400.0f;
	constexpr float minDistForAlpha = 50.0f;
	constexpr float indicatorSize = 15.f;
	static const auto indicatorColor = ImColor(90, 34, 245);

	void DrawIndicator(const SDK::Vec3& enemyPosition, const SDK::Vec3Mem viewMatrix[], const float distance) {
		float indicatorRadius = Config::Aim::FOV + 15.f;

		float t = (distance - minDistForAlpha) / (maxDistForAlpha - minDistForAlpha);
		t = std::clamp(t, 0.0f, 1.0f);
		const float finalAlpha = maxAlpha - (t * (maxAlpha - minAlpha));

		auto finalIndicatorColor = indicatorColor;
		finalIndicatorColor.Value.z = finalAlpha;

		const ImVec2 radarCenter = ImVec2(ImGui::GetIO().DisplaySize.x / 2.0f, ImGui::GetIO().DisplaySize.y / 2.0f);
		auto drawList = ImGui::GetBackgroundDrawList();
		const auto directionToEnemey = enemyPosition - Cache::localPlayerPosition;

		const auto rightComponent = directionToEnemey.Dot(viewMatrix[SDK::RIGHT_VEC]);
		const auto forwardComponent = directionToEnemey.Dot(viewMatrix[SDK::FORWARD_VEC]);

		auto enemyRadarPos = ImVec2(-rightComponent, -forwardComponent);
		const auto length = std::sqrtf((enemyRadarPos.x * enemyRadarPos.x) + (enemyRadarPos.y * enemyRadarPos.y));

		enemyRadarPos.x /= length;
		enemyRadarPos.y /= length;

		ImVec2 enemyIndicatorPos = ImVec2(
			radarCenter.x + enemyRadarPos.x * indicatorRadius,
			radarCenter.y + enemyRadarPos.y * indicatorRadius
		);

		const auto pos1 = ImVec2(
			enemyIndicatorPos.x + enemyRadarPos.x * indicatorSize,
			enemyIndicatorPos.y + enemyRadarPos.y * indicatorSize
		);

		const auto pos2 = ImVec2(
			enemyIndicatorPos.x - enemyRadarPos.y * (indicatorSize * 0.6f),
			enemyIndicatorPos.y + enemyRadarPos.x * (indicatorSize * 0.6f)
		);

		const auto pos3 = ImVec2(
			enemyIndicatorPos.x + enemyRadarPos.y * (indicatorSize * 0.6f),
			enemyIndicatorPos.y - enemyRadarPos.x * (indicatorSize * 0.6f)
		);

		drawList->AddTriangleFilled(pos1, pos2, pos3, finalIndicatorColor);
	}

	static const auto textColor = ImColor(200, 200, 210);
	static const auto outlineColor = ImColor(123, 66, 245, 100);
	static const auto backgroundColor = ImColor(0, 0, 0, 80);
	void DrawNickname(float playerBoxX, float playerBoxY, float playerBoxWidth, const std::string& text) {
		auto drawList = ImGui::GetBackgroundDrawList();

		const float fontSize = 12.f;
		ImVec2 textSize = ImGui::CalcTextSize(text.c_str());

		const float rounding = 2.f;
		const float padding = 5.f;

		ImVec2 boxSize(textSize.x + 2 * padding, textSize.y + 2 * padding);

		ImVec2 boxPosition(
			playerBoxX + (playerBoxWidth - boxSize.x) / 2.f,
			playerBoxY - boxSize.y - 5.f
		);

		ImVec2 boxMax = ImVec2(boxPosition.x + boxSize.x, boxPosition.y + boxSize.y);
		drawList->AddRectFilled(boxPosition, boxMax, backgroundColor, rounding);
		drawList->AddRect(boxPosition, boxMax, outlineColor, rounding, 0, 0.85f);

		ImVec2 textPosition(boxPosition.x + padding, boxPosition.y + padding);
		drawList->AddText(textPosition, textColor, text.c_str());
	}


	void DrawDistance(float playerBoxX, float playerBoxY, float playerBoxWidth, float playerBoxHeight, const float distance) {
		auto drawList = ImGui::GetBackgroundDrawList();

		char buf[64] {};
		ImFormatString(buf, sizeof(buf), "[ %.0fm ]", distance);

		ImVec2 textSize = ImGui::CalcTextSize(buf);

		ImVec2 textPosition(
			playerBoxX + (playerBoxWidth - textSize.x) / 2.f,
			playerBoxY + playerBoxHeight + 5.f
		);

		drawList->AddText(textPosition, textColor, buf);
	}
}