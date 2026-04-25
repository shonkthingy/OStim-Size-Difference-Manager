#pragma once

#include <optional>
#include <string_view>

namespace SizeDiff::AddressResolution
{
	std::optional<std::uintptr_t> ResolveByPdbSymbol(std::string_view mangledName);
}
