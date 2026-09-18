#include "global.h"

namespace DebugOverlay {
namespace {

ImVec2 g_panelPos { -1.f, -1.f };
ImVec2 g_panelSize {};
ImRect g_panelRect {};
bool g_dragging = false;
ImVec2 g_dragOffset {};

bool PointInPanel(const ImVec2& point) {
	return g_panelRect.GetWidth() > 0.f
		&& g_panelRect.GetHeight() > 0.f
		&& g_panelRect.Contains(point);
}

void ClampPanelToScreen(const ImVec2& displaySize) {
	if (g_panelSize.x <= 0.f || g_panelSize.y <= 0.f) {
		return;
	}

	g_panelPos.x = (std::clamp)(g_panelPos.x, 0.f, (std::max)(0.f, displaySize.x - g_panelSize.x));
	g_panelPos.y = (std::clamp)(g_panelPos.y, 0.f, (std::max)(0.f, displaySize.y - g_panelSize.y));
	g_panelRect = ImRect(g_panelPos, ImVec2(g_panelPos.x + g_panelSize.x, g_panelPos.y + g_panelSize.y));
}

void RenderHudLine(
	ImDrawList* drawList,
	ImFont* font,
	float fontSize,
	float x,
	float& y,
	const char* text,
	ImU32 color)
{
	if (!drawList || !font || !text) {
		return;
	}

	const ImVec2 textSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.f, text);
	drawList->AddText(font, fontSize, ImVec2(x, y), color, text);
	y += textSize.y + 2.f;
}

void HandleDrag(const ImVec2& displaySize) {
	ImGuiIO& io = ImGui::GetIO();
	static bool prevMouseDown = false;
	const bool mouseDown = (GetAsyncKeyState(VK_LBUTTON) & 0x8000) != 0;

	if (!g_dragging && mouseDown && !prevMouseDown && PointInPanel(io.MousePos)) {
		g_dragging = true;
		g_dragOffset = ImVec2(io.MousePos.x - g_panelPos.x, io.MousePos.y - g_panelPos.y);
	}

	if (g_dragging) {
		if (mouseDown) {
			g_panelPos.x = io.MousePos.x - g_dragOffset.x;
			g_panelPos.y = io.MousePos.y - g_dragOffset.y;
			ClampPanelToScreen(displaySize);
		} else {
			g_dragging = false;
		}
	}

	prevMouseDown = mouseDown;
}

} // namespace

bool WantsMouseCapture() {
	if (g_dragging) {
		return true;
	}

	POINT cursor {};
	if (GetCursorPos(&cursor)) {
		return PointInPanel(ImVec2(static_cast<float>(cursor.x), static_cast<float>(cursor.y)));
	}

	return false;
}

void Render(const CacheDebugStats& stats) {
	if (!stats.inMatch) {
		g_dragging = false;
		return;
	}

	char titleLine[32] { "Debug" };
	char gameLine[64] {};
	char slotsLine[64] {};
	char validLine[64] {};
	char aliveLine[64] {};
	char espLine[64] {};

	std::snprintf(gameLine, sizeof(gameLine), "Game: %d", stats.inGameMode);
	std::snprintf(slotsLine, sizeof(slotsLine), "Slots: %d", stats.slots);
	std::snprintf(validLine, sizeof(validLine), "Valid: %d", stats.valid);
	std::snprintf(aliveLine, sizeof(aliveLine), "Alive: %d", stats.alive);
	std::snprintf(espLine, sizeof(espLine), "ESP: %d", stats.espTargets);

	ImDrawList* drawList = ImGui::GetBackgroundDrawList();
	ImFont* font = ImGui::GetFont();
	if (!drawList || !font) {
		return;
	}

	const float fontSize = ImGui::GetFontSize();
	const ImVec2 displaySize = ImGui::GetIO().DisplaySize;

	const char* lines[] = { titleLine, gameLine, slotsLine, validLine, aliveLine, espLine };
	float maxWidth = 0.f;
	float totalHeight = 0.f;
	for (const char* line : lines) {
		const ImVec2 textSize = font->CalcTextSizeA(fontSize, FLT_MAX, 0.f, line);
		maxWidth = (std::max)(maxWidth, textSize.x);
		totalHeight += textSize.y + 2.f;
	}
	totalHeight -= 2.f;

	const float paddingX = 8.f;
	const float paddingY = 6.f;
	const float margin = 12.f;
	g_panelSize = ImVec2(maxWidth + (paddingX * 2.f), totalHeight + (paddingY * 2.f));

	if (g_panelPos.x < 0.f || g_panelPos.y < 0.f) {
		g_panelPos = ImVec2(displaySize.x - g_panelSize.x - margin, margin);
	}

	g_panelRect = ImRect(g_panelPos, ImVec2(g_panelPos.x + g_panelSize.x, g_panelPos.y + g_panelSize.y));
	HandleDrag(displaySize);

	const ImVec2 bgMin = g_panelPos;
	const ImVec2 bgMax(g_panelPos.x + g_panelSize.x, g_panelPos.y + g_panelSize.y);
	const float textX = bgMin.x + paddingX;
	float textY = bgMin.y + paddingY;

	const bool hovered = WantsMouseCapture();
	const ImU32 borderColor = hovered || g_dragging
		? IM_COL32(96, 200, 255, 255)
		: IM_COL32(64, 170, 255, 220);

	drawList->AddRectFilled(bgMin, bgMax, IM_COL32(8, 12, 20, 190), 4.f);
	drawList->AddRect(bgMin, bgMax, borderColor, 4.f);

	const ImU32 titleColor = IM_COL32(96, 200, 255, 255);
	const ImU32 valueColor = IM_COL32(220, 235, 255, 255);

	RenderHudLine(drawList, font, fontSize, textX, textY, titleLine, titleColor);
	RenderHudLine(drawList, font, fontSize, textX, textY, gameLine, valueColor);
	RenderHudLine(drawList, font, fontSize, textX, textY, slotsLine, valueColor);
	RenderHudLine(drawList, font, fontSize, textX, textY, validLine, valueColor);
	RenderHudLine(drawList, font, fontSize, textX, textY, aliveLine, valueColor);
	RenderHudLine(drawList, font, fontSize, textX, textY, espLine, valueColor);
}

} // namespace DebugOverlay
