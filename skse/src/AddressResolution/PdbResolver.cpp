#include "AddressResolution/PdbResolver.h"

#include "PCH.h"
#include "spdlog/spdlog.h"

#pragma comment(lib, "Dbghelp.lib")

std::optional<std::uintptr_t> SizeDiff::AddressResolution::ResolveByPdbSymbol(std::string_view mangledName)
{
	const auto module = GetModuleHandleA("OStim.dll");
	if (!module) {
		return std::nullopt;
	}

	const HANDLE process = GetCurrentProcess();
	SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEFERRED_LOADS);
	if (!SymInitialize(process, nullptr, TRUE)) {
		spdlog::warn("SymInitialize failed");
		return std::nullopt;
	}

	std::array<char, sizeof(SYMBOL_INFO) + 512> storage{};
	auto* symbol = reinterpret_cast<SYMBOL_INFO*>(storage.data());
	symbol->SizeOfStruct = sizeof(SYMBOL_INFO);
	symbol->MaxNameLen = 511;

	if (!SymFromName(process, mangledName.data(), symbol)) {
		spdlog::warn("SymFromName failed for {}", mangledName);
		SymCleanup(process);
		return std::nullopt;
	}

	const auto address = static_cast<std::uintptr_t>(symbol->Address);
	SymCleanup(process);
	return address;
}
