#pragma once

#include <cstdint>
#include <vector>

namespace SizeDiff::State
{
	/// OStim logical thread id -> actor scales (for that scene thread).
	void SetScales(uint32_t threadId, std::vector<float> scales);
	void ClearScales(uint32_t threadId);
	std::vector<float> GetScales(uint32_t threadId);

	/// OStim thread that belongs to the player (used when UI / fulfilledBy run on a different OS thread).
	void SetPlayerThreadId(uint32_t threadId);
	uint32_t GetPlayerThreadId();

	/// Best-effort thread id for graph hooks (updated from OStim thread listeners).
	void NotifyThreadActive(uint32_t threadId);
	uint32_t GetLastActiveThreadId();
}
