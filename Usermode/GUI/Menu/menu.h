#pragma once

namespace Menu {
	enum Tabs : int {
		AIMBOT,
		ESP,
		SETTINGS,
		CONFIG,
	};

	void RenderMenu();
	bool IsOpen();
	void PrepareOverlayInput();
}