#include <windows.h>
#include <dbghelp.h>

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

struct ModuleInfo
{
    uint64_t base = 0;
    uint32_t size = 0;
    std::wstring name;
};

static std::wstring ReadMinidumpString(void* base, RVA rva)
{
    if (!rva)
        return L"";

    auto* text = reinterpret_cast<MINIDUMP_STRING*>(static_cast<BYTE*>(base) + rva);
    return std::wstring(text->Buffer, text->Length / sizeof(wchar_t));
}

static const ModuleInfo* FindModule(std::vector<ModuleInfo> const& modules, uint64_t address)
{
    for (ModuleInfo const& module : modules)
    {
        if (address >= module.base && address < module.base + module.size)
            return &module;
    }

    return nullptr;
}

static bool ReadDumpMemory(void* dumpBase, uint64_t address, BYTE* output, size_t outputSize)
{
    MINIDUMP_MEMORY_LIST* memoryList = nullptr;
    ULONG streamSize = 0;
    if (!MiniDumpReadDumpStream(dumpBase, MemoryListStream, nullptr, reinterpret_cast<void**>(&memoryList), &streamSize) || !memoryList)
        return false;

    for (ULONG32 i = 0; i < memoryList->NumberOfMemoryRanges; ++i)
    {
        MINIDUMP_MEMORY_DESCRIPTOR const& range = memoryList->MemoryRanges[i];
        uint64_t start = range.StartOfMemoryRange;
        uint64_t end = start + range.Memory.DataSize;
        if (address >= start && address + outputSize <= end)
        {
            CopyMemory(output, static_cast<BYTE*>(dumpBase) + range.Memory.Rva + (address - start), outputSize);
            return true;
        }
    }

    return false;
}

static void PrintAddress(std::vector<ModuleInfo> const& modules, char const* label, uint64_t address)
{
    ModuleInfo const* module = FindModule(modules, address);
    if (module)
        std::printf("%s=0x%08llX moduleBase=0x%08llX rva=0x%08llX module=%ls\n", label, address, module->base, address - module->base, module->name.c_str());
    else
        std::printf("%s=0x%08llX module=<none>\n", label, address);
}

static void PrintStackCandidates(void* dumpBase, std::vector<ModuleInfo> const& modules, DWORD esp)
{
    BYTE stackBytes[512] = {};
    if (!ReadDumpMemory(dumpBase, esp, stackBytes, sizeof(stackBytes)))
    {
        std::printf("stack@ESP=<not in dump memory list>\n");
        return;
    }

    std::printf("stackCandidates:\n");
    for (size_t offset = 0; offset + sizeof(DWORD) <= sizeof(stackBytes); offset += sizeof(DWORD))
    {
        DWORD value = *reinterpret_cast<DWORD*>(stackBytes + offset);
        ModuleInfo const* module = FindModule(modules, value);
        if (!module)
            continue;

        std::printf("  [ESP+0x%03IX]=0x%08lX rva=0x%08llX module=%ls\n",
            offset, value, static_cast<uint64_t>(value) - module->base, module->name.c_str());
    }
}

int wmain(int argc, wchar_t** argv)
{
    if (argc < 2)
    {
        std::fwprintf(stderr, L"usage: minidump_pe_analyze.exe <dump.dmp>\n");
        return 2;
    }

    HANDLE file = CreateFileW(argv[1], GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE)
    {
        std::fwprintf(stderr, L"open failed: %ls error=%lu\n", argv[1], GetLastError());
        return 1;
    }

    HANDLE mapping = CreateFileMappingW(file, nullptr, PAGE_READONLY, 0, 0, nullptr);
    if (!mapping)
    {
        std::fprintf(stderr, "CreateFileMapping failed: %lu\n", GetLastError());
        CloseHandle(file);
        return 1;
    }

    void* dumpBase = MapViewOfFile(mapping, FILE_MAP_READ, 0, 0, 0);
    if (!dumpBase)
    {
        std::fprintf(stderr, "MapViewOfFile failed: %lu\n", GetLastError());
        CloseHandle(mapping);
        CloseHandle(file);
        return 1;
    }

    std::vector<ModuleInfo> modules;
    MINIDUMP_MODULE_LIST* moduleList = nullptr;
    ULONG streamSize = 0;
    if (MiniDumpReadDumpStream(dumpBase, ModuleListStream, nullptr, reinterpret_cast<void**>(&moduleList), &streamSize) && moduleList)
    {
        for (ULONG32 i = 0; i < moduleList->NumberOfModules; ++i)
        {
            MINIDUMP_MODULE const& item = moduleList->Modules[i];
            ModuleInfo module;
            module.base = item.BaseOfImage;
            module.size = item.SizeOfImage;
            module.name = ReadMinidumpString(dumpBase, item.ModuleNameRva);
            modules.push_back(module);
        }

        std::sort(modules.begin(), modules.end(), [](ModuleInfo const& left, ModuleInfo const& right) {
            return left.base < right.base;
        });
    }

    MINIDUMP_EXCEPTION_STREAM* exceptionStream = nullptr;
    if (MiniDumpReadDumpStream(dumpBase, ExceptionStream, nullptr, reinterpret_cast<void**>(&exceptionStream), &streamSize) && exceptionStream)
    {
        MINIDUMP_EXCEPTION const& ex = exceptionStream->ExceptionRecord;
        std::printf("threadId=%lu exceptionCode=0x%08lX flags=0x%08lX parameters=%lu\n",
            exceptionStream->ThreadId, ex.ExceptionCode, ex.ExceptionFlags, ex.NumberParameters);
        PrintAddress(modules, "exceptionAddress", ex.ExceptionAddress);

        CONTEXT* context = reinterpret_cast<CONTEXT*>(static_cast<BYTE*>(dumpBase) + exceptionStream->ThreadContext.Rva);
        if (context)
        {
            PrintAddress(modules, "EIP", context->Eip);
            std::printf("EAX=0x%08lX EBX=0x%08lX ECX=0x%08lX EDX=0x%08lX ESI=0x%08lX EDI=0x%08lX ESP=0x%08lX EBP=0x%08lX EFLAGS=0x%08lX\n",
                context->Eax, context->Ebx, context->Ecx, context->Edx, context->Esi, context->Edi, context->Esp, context->Ebp, context->EFlags);

            BYTE bytes[32] = {};
            if (ReadDumpMemory(dumpBase, context->Eip, bytes, sizeof(bytes)))
            {
                std::printf("bytes@EIP=");
                for (BYTE value : bytes)
                    std::printf("%02X ", value);
                std::printf("\n");
            }
            else
            {
                std::printf("bytes@EIP=<not in dump memory list>\n");
            }

            PrintStackCandidates(dumpBase, modules, context->Esp);
        }
    }
    else
    {
        std::printf("no exception stream\n");
    }

    std::printf("modules=%zu\n", modules.size());
    for (ModuleInfo const& module : modules)
    {
        if (module.name.find(L"wow.exe") != std::wstring::npos ||
            module.name.find(L"hermes") != std::wstring::npos ||
            module.name.find(L"v2_") != std::wstring::npos ||
            module.name.find(L"ntdll.dll") != std::wstring::npos ||
            module.name.find(L"KERNEL") != std::wstring::npos)
        {
            std::printf("module base=0x%08llX size=0x%08lX end=0x%08llX name=%ls\n",
                module.base, module.size, module.base + module.size, module.name.c_str());
        }
    }

    UnmapViewOfFile(dumpBase);
    CloseHandle(mapping);
    CloseHandle(file);
    return 0;
}
