#include <windows.h>
#include <stdio.h>
#include <stdint.h>
#include <stdlib.h>

typedef struct Candidate
{
    DWORD address;
    DWORD socketValue;
    DWORD state;
    DWORD ptr14;
    DWORD flags;
} Candidate;

static int ReadMem(HANDLE process, DWORD address, void* buffer, SIZE_T size)
{
    SIZE_T bytesRead = 0;
    return ReadProcessMemory(process, (LPCVOID)(uintptr_t)address, buffer, size, &bytesRead) && bytesRead == size;
}

static int IsReadableProtect(DWORD protect)
{
    if (protect & (PAGE_NOACCESS | PAGE_GUARD))
        return 0;
    return protect == PAGE_READONLY || protect == PAGE_READWRITE || protect == PAGE_WRITECOPY ||
        protect == PAGE_EXECUTE_READ || protect == PAGE_EXECUTE_READWRITE || protect == PAGE_EXECUTE_WRITECOPY;
}

static int LooksLikeConnection(BYTE* data, SIZE_T offset, DWORD base, Candidate* out)
{
    DWORD first = *(DWORD*)(data + offset + 0x00);
    DWORD socketValue = *(DWORD*)(data + offset + 0x04);
    DWORD minusOneA = *(DWORD*)(data + offset + 0x08);
    DWORD zeroC = *(DWORD*)(data + offset + 0x0C);
    DWORD state = *(DWORD*)(data + offset + 0x10);
    DWORD ptr14 = *(DWORD*)(data + offset + 0x14);
    DWORD zero18 = *(DWORD*)(data + offset + 0x18);
    DWORD ptr1C = *(DWORD*)(data + offset + 0x1C);
    DWORD size24 = *(DWORD*)(data + offset + 0x24);
    DWORD minusOne28 = *(DWORD*)(data + offset + 0x28);
    DWORD minusOne2C = *(DWORD*)(data + offset + 0x2C);
    DWORD marker3C = *(DWORD*)(data + offset + 0x3C);

    if (first != 2 || minusOneA != 0xFFFFFFFFu || zeroC != 0 || state != 5 || zero18 != 0)
        return 0;
    if (socketValue == 0 || socketValue > 0xFFFF)
        return 0;
    if (ptr14 < 0x10000000 || ptr14 > 0x7FFFFFFF || ptr1C < 0x10000000 || ptr1C > 0x7FFFFFFF)
        return 0;
    if (size24 != 0x200000 || minusOne28 != 0xFFFFFFFFu || minusOne2C != 0xFFFFFFFFu)
        return 0;
    if ((marker3C & 0xFFFF) != 0x07D0)
        return 0;

    out->address = base + (DWORD)offset;
    out->socketValue = socketValue;
    out->state = state;
    out->ptr14 = ptr14;
    out->flags = *(DWORD*)(data + offset + 0xE8);
    return 1;
}

int main(int argc, char** argv)
{
    if (argc < 2)
    {
        fprintf(stderr, "usage: hermes_process_inspect.exe <pid>\n");
        return 1;
    }

    DWORD pid = (DWORD)strtoul(argv[1], NULL, 10);
    HANDLE process = OpenProcess(PROCESS_QUERY_INFORMATION | PROCESS_VM_READ, FALSE, pid);
    if (!process)
    {
        fprintf(stderr, "OpenProcess failed: %lu\n", GetLastError());
        return 2;
    }

    DWORD addresses[] = { 0x004675F0, 0x00632001, 0x00500560 };
    for (int i = 0; i < 3; ++i)
    {
        BYTE bytes[8] = {0};
        if (ReadMem(process, addresses[i], bytes, sizeof(bytes)))
        {
            printf("addr 0x%08lX: %02X %02X %02X %02X %02X %02X %02X %02X\n",
                addresses[i], bytes[0], bytes[1], bytes[2], bytes[3], bytes[4], bytes[5], bytes[6], bytes[7]);
        }
        else
        {
            printf("addr 0x%08lX: unreadable\n", addresses[i]);
        }
    }

    SYSTEM_INFO info;
    GetSystemInfo(&info);
    DWORD address = 0x10000000;
    Candidate candidates[32];
    int candidateCount = 0;

    while (address < 0x70000000 && candidateCount < 32)
    {
        MEMORY_BASIC_INFORMATION mbi;
        if (!VirtualQueryEx(process, (LPCVOID)(uintptr_t)address, &mbi, sizeof(mbi)))
        {
            address += 0x10000;
            continue;
        }

        DWORD base = (DWORD)(uintptr_t)mbi.BaseAddress;
        DWORD regionSize = (DWORD)mbi.RegionSize;
        if (mbi.State == MEM_COMMIT && IsReadableProtect(mbi.Protect) && regionSize >= 0x1000 && regionSize <= 0x4000000)
        {
            BYTE* buffer = (BYTE*)malloc(regionSize);
            SIZE_T bytesRead = 0;
            if (buffer && ReadProcessMemory(process, mbi.BaseAddress, buffer, regionSize, &bytesRead) && bytesRead >= 0xF0)
            {
                for (DWORD offset = 0; offset + 0xF0 < bytesRead && candidateCount < 32; offset += 4)
                {
                    Candidate candidate;
                    if (LooksLikeConnection(buffer, offset, base, &candidate))
                        candidates[candidateCount++] = candidate;
                }
            }
            if (buffer)
                free(buffer);
        }

        address = base + regionSize;
        if (address <= base)
            break;
    }

    printf("connection candidates=%d\n", candidateCount);
    for (int i = 0; i < candidateCount; ++i)
    {
        printf("candidate[%d] self=0x%08lX socket=0x%08lX state=%lu ptr14=0x%08lX flags=0x%08lX\n",
            i, candidates[i].address, candidates[i].socketValue, candidates[i].state, candidates[i].ptr14, candidates[i].flags);
    }

    CloseHandle(process);
    return 0;
}
