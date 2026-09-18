#pragma once

#include "Game/cache.h"

namespace DebugOverlay {
	void Render(const CacheDebugStats& stats);
	bool WantsMouseCapture();
}
