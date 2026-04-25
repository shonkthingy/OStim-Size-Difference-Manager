#pragma once

#include <vector>

namespace SizeDiff::Tls
{
	inline thread_local std::vector<std::vector<float>> g_scaleStack;

	inline void PushScales(std::vector<float> scales)
	{
		g_scaleStack.push_back(std::move(scales));
	}

	inline void PopScales()
	{
		if (!g_scaleStack.empty()) {
			g_scaleStack.pop_back();
		}
	}

	inline std::vector<float> CurrentScales()
	{
		if (g_scaleStack.empty()) {
			return {};
		}
		return g_scaleStack.back();
	}
}
