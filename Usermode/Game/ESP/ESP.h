#pragma once

namespace ESP {
	inline void DrawLine(const int& xMin, const int& yMin, const int& xMax, const int& yMax, const ImColor& color) {
		ImGui::GetBackgroundDrawList()->AddLine(ImVec2(xMin, yMin), ImVec2(xMax, yMax), color);
	}

	void DrawBox(const int& x, const int& y, const int& w, const int& h, const ImColor& color);
	void DrawFilledBox(const int& x, const int& y, const int& w, const int& h, const ImColor& color, const ImColor& filledColor);
	void DrawHealthBar(int id, float x, float y, float w, float h, int health, float barThickness);
	void DrawSkeleton(
		const std::array<SDK::Vec2, SDK::BONE_IDX_COUNT>& screenBones,
		const std::array<SDK::Vec3, SDK::BONE_IDX_COUNT>& worldBones,
		const SDK::Vec3& origin,
		const ImColor& color
	);
	void DrawIndicator(const SDK::Vec3& enemyPosition, const SDK::Vec3Mem viewMatrix[], const float distance);
	void DrawNickname(float playerBoxX, float playerBoxY, float playerBoxWidth, const std::string& text);
	void DrawDistance(float playerBoxX, float playerBoxY, float playerBoxWidth, float playerBoxHeight, const float distance);
}