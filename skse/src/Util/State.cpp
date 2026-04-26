#include "Util/State.h"

#include <atomic>
#include <mutex>
#include <unordered_map>

namespace
{
	std::mutex g_mutex;
	std::unordered_map<uint32_t, std::vector<float>> g_scalesByThread;
	std::atomic<uint32_t> g_playerOStimThreadId{ 0 };
}

void SizeDiff::State::SetScales(uint32_t threadId, std::vector<float> scales)
{
	std::lock_guard lock(g_mutex);
	g_scalesByThread[threadId] = std::move(scales);
}

void SizeDiff::State::ClearScales(uint32_t threadId)
{
	std::lock_guard lock(g_mutex);
	g_scalesByThread.erase(threadId);
}

std::vector<float> SizeDiff::State::GetScales(uint32_t threadId)
{
	std::lock_guard lock(g_mutex);
	const auto it = g_scalesByThread.find(threadId);
	if (it == g_scalesByThread.end()) {
		return {};
	}
	return it->second;
}

void SizeDiff::State::SetPlayerThreadId(uint32_t threadId)
{
	g_playerOStimThreadId.store(threadId, std::memory_order_release);
}

uint32_t SizeDiff::State::GetPlayerThreadId()
{
	return g_playerOStimThreadId.load(std::memory_order_acquire);
}
