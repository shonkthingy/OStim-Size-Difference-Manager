// One-off: build with cl /EHsc /Fe:resolve.exe main.cpp /link Dbghelp.lib
#include <windows.h>
#include <dbghelp.h>
#include <cstdio>
#include <cstdint>
#include <cstring>

int main() {
  HMODULE m = LoadLibraryA("F:\\Games\\steamapps\\common\\Skyrim Special Edition\\Data\\SKSE\\Plugins\\OStim.dll");
  if (!m) { printf("LoadLibrary failed %lu\n", GetLastError()); return 1; }
  printf("OStim base %p\n", m);
  HANDLE p = GetCurrentProcess();
  SymSetOptions(SYMOPT_UNDNAME | SYMOPT_DEBUG);
  if (!SymInitialize(p, "F:\\Games\\steamapps\\common\\Skyrim Special Edition\\Data\\SKSE\\Plugins;SRV*C:\\SymCache*https://msdl.microsoft.com/download/symbols", FALSE)) { printf("SymInit fail\n"); return 1; }
  const char* kDll = "F:\\Games\\steamapps\\common\\Skyrim Special Edition\\Data\\SKSE\\Plugins\\OStim.dll";
  if (!SymLoadModuleEx(p, nullptr, kDll, nullptr, (DWORD64)m, 0, nullptr, 0)) {
    printf("SymLoadModuleEx fail %lu (module may use embedded PDB path)\n", GetLastError());
  } else {
    printf("SymLoadModuleEx OK\n");
  }
  const char* names[] = {
    "?fulfilledBy@Navigation@Graph@@QEAA_NV?$vector@UActorCondition@Trait@@V?$allocator@UActorCondition@Trait@@@std@@@std@@@Z",
  };
  for (const char* name : names) {
    char buf[sizeof(SYMBOL_INFO) + 256];
    auto* sym = (SYMBOL_INFO*)buf;
    sym->SizeOfStruct = sizeof(SYMBOL_INFO);
    sym->MaxNameLen = 255;
    if (SymFromName(p, name, sym)) {
      const auto rva = sym->Address - (DWORD64)m;
      printf("FOUND %s at module + 0x%llX (abs %p)\n", name, (unsigned long long)rva, (void*)sym->Address);
      const auto* pbytes = (const std::uint8_t*)sym->Address;
      printf("First 32 bytes: ");
      for (int i = 0; i < 32; ++i) {
        printf("%02X ", pbytes[i]);
      }
      printf("\n");
    } else {
      printf("NOT FOUND %s err %lu\n", name, GetLastError());
    }
  }

  printf("\n-- Enumerate *fulfilledBy* in OStim --\n");
  struct Ctx { DWORD64 base; };
  Ctx ctx{ (DWORD64)m };
  if (!SymEnumSymbols(p, (ULONG64)m, "*fulfilledBy*", [](PSYMBOL_INFO s, ULONG, PVOID user) -> BOOL {
        const auto* c = static_cast<Ctx*>(user);
        if (s->Name && std::strstr(s->Name, "Navigation")) {
          printf("  %s @ +0x%llX\n", s->Name, (unsigned long long)(s->Address - c->base));
        }
        return TRUE;
      },
      &ctx)) {
    printf("SymEnumSymbols failed %lu\n", GetLastError());
  }

  SymCleanup(p);
  return 0;
}
