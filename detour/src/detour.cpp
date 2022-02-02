#include <pch.h>
#include <detour.h>

// This shouldn't change until next load of the elf program.
uintptr_t g_PageSize = static_cast<uintptr_t>(sysconf(_SC_PAGESIZE));

namespace stubs
{
    namespace x64
    {
        std::vector<uint8_t> makeJmp(uptr_t src)
        {
            std::vector<uint8_t> veclongJmp;
            veclongJmp.insert(veclongJmp.end(), mov_reg.begin(), mov_reg.end());

            memcpy(reinterpret_cast<ptr_t>(
                       reinterpret_cast<uintptr_t>(veclongJmp.data()) +
                       mov_reg_offset),
                   &src.p,
                   sizeof(ptr_t));

            veclongJmp.insert(veclongJmp.end(), jmp_reg.begin(), jmp_reg.end());

            return veclongJmp;
        };

        std::vector<uint8_t> makeCall(uptr_t src)
        {
            std::vector<uint8_t> veclongCall;
            veclongCall.insert(veclongCall.end(),
                               mov_reg.begin(),
                               mov_reg.end());
            memcpy(reinterpret_cast<ptr_t>(
                       reinterpret_cast<uintptr_t>(veclongCall.data()) +
                       mov_reg_offset),
                   &src.p,
                   sizeof(ptr_t));

            veclongCall.insert(veclongCall.end(),
                               call_reg.begin(),
                               call_reg.end());

            return veclongCall;
        };
    };

    namespace x86
    {
        std::vector<uint8_t> makeJmp(uptr_t src)
        {
            std::vector<uint8_t> veclongJmp;
            veclongJmp.insert(veclongJmp.end(), mov_reg.begin(), mov_reg.end());

            memcpy(reinterpret_cast<ptr_t>(
                       reinterpret_cast<uintptr_t>(veclongJmp.data()) +
                       mov_reg_offset),
                   &src.p,
                   sizeof(ptr_t));

            veclongJmp.insert(veclongJmp.end(), jmp_reg.begin(), jmp_reg.end());

            return veclongJmp;
        };

        std::vector<uint8_t> makeCall(uptr_t src)
        {
            std::vector<uint8_t> veclongCall;
            veclongCall.insert(veclongCall.end(),
                               mov_reg.begin(),
                               mov_reg.end());
            memcpy(reinterpret_cast<ptr_t>(
                       reinterpret_cast<uintptr_t>(veclongCall.data()) +
                       mov_reg_offset),
                   &src.p,
                   sizeof(ptr_t));

            veclongCall.insert(veclongCall.end(),
                               call_reg.begin(),
                               call_reg.end());

            return veclongCall;
        };

    };
};

auto mread() -> std::string
{
    constexpr const char* strFileMaps = "/proc/self/maps";

    FILE* fileMaps = fopen(strFileMaps, "r");

    if (fileMaps == nullptr)
    {
        std::cout << "Couldn't open " << strFileMaps << std::endl;
        return 0;
    }

    std::string mappedMemory;

    while (true)
    {
        auto curChar = fgetc(fileMaps);
        if (curChar <= 0)
            break;

        mappedMemory += std::string(1, curChar);
    }

    fclose(fileMaps);

    return mappedMemory;
}

auto minfo(const uptr_t& address, mapinfo_t* mapinfo) -> bool
{
    if (mapinfo == nullptr)
    {
        mapinfo_t mapinfo_tmp;

        mapinfo = &mapinfo_tmp;
    }

    std::string mappedMemory = mread();

    size_t nPosEndLine = 0;
    size_t nPosStartLine = 0;

    while (true)
    {
        // 561d78663000-561d78665000 r--p 00000000 103:05 11284980 /usr/bin/cat
        nPosEndLine = mappedMemory.find('\n', nPosEndLine);

        if (nPosEndLine == mappedMemory.npos)
        {
            break;
        }

        auto nPosPageStart = mappedMemory.find('-', nPosStartLine);

        if (nPosPageStart == mappedMemory.npos)
        {
            break;
        }

        auto nPosPageEnd = mappedMemory.find(' ', nPosPageStart + 1);

        if (nPosPageEnd == mappedMemory.npos)
        {
            break;
        }

        auto nPosProt = mappedMemory.find(' ', nPosPageEnd + 1);
        if (nPosProt == mappedMemory.npos)
        {
            break;
        }

        std::stringstream strStreamPageStart;
        strStreamPageStart << std::hex
                           << mappedMemory.substr(nPosStartLine,
                                                  (nPosPageStart -
                                                   nPosStartLine));

        strStreamPageStart >> mapinfo->start.ui;

        std::stringstream strStreamPageEnd;
        strStreamPageEnd << std::hex
                         << mappedMemory.substr(nPosPageStart + 1,
                                                nPosPageEnd -
                                                    (nPosPageStart + 1));
        strStreamPageEnd >> mapinfo->size.ui;

        if (address.ui >= mapinfo->start.ui && address.ui < mapinfo->size.ui)
        {
            mapinfo->size.ui -= mapinfo->start.ui;
            auto strProt = mappedMemory.substr(nPosPageEnd + 1,
                                               nPosProt - (nPosPageEnd + 1));
            mapinfo->prot = PROT_NONE;

            for (auto&& i : strProt)
            {
                if (i == 'r')
                {
                    mapinfo->prot |= PROT_READ;
                }
                else if (i == 'w')
                {
                    mapinfo->prot |= PROT_WRITE;
                }
                else if (i == 'x')
                {
                    mapinfo->prot |= PROT_EXEC;
                }
            }

            return true;
        }

        nPosEndLine++;
        nPosStartLine = nPosEndLine;
    }

    return false;
}

// So we don't change how memory is structured.
auto mprotect(const uptr_t& address, int newProt, mapinfo_t* mapinfo) -> int
{
    if (address.p == nullptr)
        return -1;

    if (mapinfo == nullptr)
    {
        mapinfo_t mapinfo_tmp;

        mapinfo = &mapinfo_tmp;
    }

    if (!minfo(address, mapinfo))
        return -1;

    return mprotect(mapinfo->start.p, mapinfo->size.ui, newProt);
}

auto mprotect(const uptr_t& address,
              uintptr_t size,
              int newProt,
              mapinfo_t* mapinfo) -> int
{
    if (address.p == nullptr)
        return -1;

    if (mapinfo == nullptr)
    {
        mapinfo_t mapinfo_tmp;

        mapinfo = &mapinfo_tmp;
    }

    if (!minfo(address, mapinfo))
        return -1;

    uintptr_t pagesSize = ((size - 1) / g_PageSize) + g_PageSize;
    uptr_t realAddress(address.ui & -g_PageSize);

    return mprotect(realAddress.p, pagesSize, newProt);
}
