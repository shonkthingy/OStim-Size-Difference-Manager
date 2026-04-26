#pragma once

#include "Config/Config.h"

#include <cstdint>

namespace SizeDiff::Filter
{
	/// Thread id used by graph random-node hooks (best-effort).
	uint32_t ResolveGraphHookThreadId();

	/// Player-centric navigation menu (fulfilledBy).
	uint32_t ResolveMenuHookThreadId();

	/// When true, caller should not apply scale filtering for this thread / settings combo.
	bool ShouldBypassFiltering(uint32_t threadId, const Config::Settings& settings);

	bool QueryIsAutoMode(uint32_t threadId);
}
