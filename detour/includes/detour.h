#ifndef DETOUR
#define DETOUR

#include <pch.h>

typedef void* ptr_t;
typedef struct mapinfo_s mapinfo_t;
union uptr_t;

extern uintptr_t g_PageSize;
extern auto mprotect(const uptr_t& address,
                     uintptr_t size,
                     int newProt,
                     mapinfo_t* mapinfo = nullptr) -> int;
extern auto mprotect(const uptr_t& address,
                     int newProt,
                     mapinfo_t* mapinfo = nullptr) -> int;
extern auto minfo(const uptr_t& address, mapinfo_t* mapinfo = nullptr) -> bool;
extern auto mread() -> std::string;

union uptr_t
{
    uptr_t()
    {
        memset(this, 0, sizeof(uptr_t));
    }

    template <typename T>
    uptr_t(T value)
    {
        if constexpr (sizeof(T) != sizeof(ptr_t))
            static_assert("Wrong variable type.");

        ui = (uintptr_t)(value);
    }

    template <typename T>
    constexpr auto operator=(T value)
    {
        if constexpr (std::is_same<T, std::nullptr_t>::value)
            return nullptr;

        else
        {
            memcpy(&p, &value, sizeof(T));
            return p;
        }
    }

    template <typename T>
    auto operator=(const uptr_t& pointer)
    {
        memcpy(&p, &pointer.p, sizeof(uptr_t));
        return reinterpret_cast<T>(ui);
    }

    template <typename T>
    auto operator!=(T value)
    {
        if (minfo(*this))
            return reinterpret_cast<T>(p) != value;

        return false;
    }

    ptr_t p;
    uintptr_t ui;
    uint8_t b[sizeof(ptr_t)];
};

typedef struct mapinfo_s
{
    uptr_t start;
    uptr_t size;
    int prot;
} mapinfo_t;

namespace stubs
{
    namespace x64
    {
        // r11 is used here as temp register. Should be safe to modify as it's
        // not preserved across functions.
        constexpr uintptr_t mov_reg_offset = 2;
        constexpr std::array<uint8_t, 10> mov_reg =
            {0x49, 0xBB, 0x88, 0x77, 0x66, 0x55, 0x44, 0x33, 0x22, 0x11};

        constexpr std::array<uint8_t, 3> jmp_reg = {0x41, 0xFF, 0xE3};
        constexpr std::array<uint8_t, 3> call_reg = {0x41, 0xFF, 0xD3};

        constexpr std::array<uint8_t, 2> push_reg = {0x41, 0x53};
        constexpr std::array<uint8_t, 2> pop_reg = {0x41, 0x5B};
        std::vector<uint8_t> makeJmp(uptr_t src);
        std::vector<uint8_t> makeCall(uptr_t src);

    };

    namespace x86
    {
        // eax is used here as temp register. Should be safe to use even if it's
        // used for return for the call cause we return something
        // at the end of the function.
        constexpr uintptr_t mov_reg_offset = 1;
        constexpr std::array<uint8_t, 5> mov_reg = {0xB8,
                                                    0x44,
                                                    0x33,
                                                    0x22,
                                                    0x11};

        constexpr std::array<uint8_t, 2> jmp_reg = {0xFF, 0xE0};
        constexpr std::array<uint8_t, 2> call_reg = {0xFF, 0xD0};

        constexpr std::array<uint8_t, 1> push_reg = {0x50};
        constexpr std::array<uint8_t, 1> pop_reg = {0x58};
        std::vector<uint8_t> makeJmp(uptr_t src);
        std::vector<uint8_t> makeCall(uptr_t src);

    };

    namespace xCUR
    {
#ifdef MX64
        using namespace x64;
#else
        using namespace x86;
#endif
    };

    // Instructions wich are same for x64 & x86.
    constexpr uint8_t ret = 0xC3;

    constexpr uintptr_t rel_call_offset = 1;
    constexpr uintptr_t rel_jmp_offset = 1;

    constexpr std::array<uint8_t, 5> rel_call = {0xE8, 0x44, 0x33, 0x22, 0x11};
    constexpr std::array<uint8_t, 5> rel_jmp = {0xE9, 0x44, 0x33, 0x22, 0x11};

};

template <typename T>
auto isValidPtr(T addr)
{
    if (addr == 0)
        return false;

    if (minfo(uptr_t(addr)))
        return true;

    return false;
}

template <typename RET = void, typename... vArgs>
class CDetourHandler
{
 public:
    typedef RET (*typefunc_t)(vArgs...);

    CDetourHandler() :
        m_pOriginalFunction(nullptr), m_pFunction(nullptr), m_bSafeDelete(true),
        m_bAskDelete(false), m_bCallingOriginal(false)
    {}

    template <typename T = typefunc_t>
    CDetourHandler(T pOriginal) :
        m_pOriginalFunction(reinterpret_cast<typefunc_t>(pOriginal)),
        m_pFunction(nullptr), m_bSafeDelete(true), m_bAskDelete(false),
        m_bCallingOriginal(false)
    {
        if (!minfo(m_pOriginalFunction, &mapinfo))
        {
            assert("Couldn't get mapping informations.");
        }
    }

    ~CDetourHandler()
    {
        m_bAskDelete = true;

        while (true)
        {
            // Let's wait for it.
            // A ret instruction is really fast enough to just check it this
            // way.
            if (m_bSafeDelete)
                break;
        }

        restoreOpCodes();
    }

    auto setFunction(typefunc_t pNewFunction) -> void
    {
        m_pFunction = pNewFunction;
    }

    auto setOriginalFunction(typefunc_t pOriginalFunction) -> void
    {
        m_pOriginalFunction = pOriginalFunction;
    }

    auto restoreOpCodes() -> bool
    {
        if (mprotect(mapinfo.start.p,
                     mapinfo.size.ui,
                     PROT_EXEC | PROT_READ | PROT_WRITE) != -1)
        {
            memcpy(reinterpret_cast<ptr_t>(m_pOriginalFunction),
                   m_vecOriginalFunctionBytes.data(),
                   m_vecOriginalFunctionBytes.size());

            if (mprotect(mapinfo.start.p, mapinfo.size.ui, mapinfo.prot) != -1)
            {
                return true;
            }
        }

        return false;
    }

    auto placeStubJmp() -> bool
    {
        if (mprotect(mapinfo.start.p,
                     mapinfo.size.ui,
                     PROT_EXEC | PROT_READ | PROT_WRITE) != -1)
        {
            /*
            startoforiginalfunction:
                    mov temp_reg, function
                    jmp temp_reg
            */

            std::vector<uint8_t> vecOpCodes;

            auto stubJmp = stubs::xCUR::makeJmp(m_pFunction);
            vecOpCodes.insert(vecOpCodes.end(), stubJmp.begin(), stubJmp.end());

            m_vecOriginalFunctionBytes.resize(vecOpCodes.size());

            memcpy(m_vecOriginalFunctionBytes.data(),
                   reinterpret_cast<ptr_t>(m_pOriginalFunction),
                   vecOpCodes.size());
            memcpy(reinterpret_cast<ptr_t>(m_pOriginalFunction),
                   vecOpCodes.data(),
                   vecOpCodes.size());

            if (mprotect(mapinfo.start.p, mapinfo.size.ui, mapinfo.prot) != -1)
            {
                return true;
            }
        }

        return false;
    }

    constexpr inline auto callOriginal(vArgs... pArgs) -> RET
    {
        if constexpr (std::is_same<RET, void>::value)
        {
            m_bCallingOriginal = true;

            if (!restoreOpCodes())
            {
                assert("Couldn't restore opcodes.");
            }

            // The fail is starting there cause there is not our jmp code yet
            // Since we're calling the original function
            m_pOriginalFunction(pArgs...);

            // Place as soon as possible our new jmp so we can hope to catch all
            // threads. Don't place again the stub jmp if we were asked to
            // remove this detour.
            if (!m_bAskDelete)
            {
                if (!placeStubJmp())
                {
                    assert("Couldn't place stub jmp.");
                }
            }

            m_bCallingOriginal = false;
        }
        else
        {
            m_bCallingOriginal = true;

            if (!restoreOpCodes())
            {
                assert("Couldn't restore opcodes.");
            }

            // The fail is starting there cause there is not our jmp code yet
            // Since we're calling the original function
            RET ret = m_pOriginalFunction(pArgs...);

            // Place as soon as possible our new jmp so we can hope to catch all
            // threads. Don't place again the stub jmp if we were asked to
            // remove this detour.
            if (!m_bAskDelete)
            {
                if (!placeStubJmp())
                {
                    assert("Couldn't place stub jmp.");
                }
            }

            m_bCallingOriginal = false;

            return ret;
        }
    }

    template <bool bValue = true>
    auto setSafeDelete() -> void
    {
        m_bSafeDelete = bValue;
    }

    auto setSafeDelete(bool bValue) -> void
    {
        m_bSafeDelete = bValue;
    }

    auto& getSafeDelete()
    {
        return m_bSafeDelete;
    }

    auto& getCallingOriginal()
    {
        return m_bCallingOriginal;
    }

    auto& getOriginalFunction()
    {
        return m_pOriginalFunction;
    }

    auto& getFunction()
    {
        return m_pFunction;
    }

 private:
    // The original function that will be detoured.
    typefunc_t m_pOriginalFunction;
    // Our new function.
    typefunc_t m_pFunction;
    std::vector<uint8_t> m_vecOriginalFunctionBytes;
    bool m_bSafeDelete, m_bAskDelete, m_bCallingOriginal;
    mapinfo_t mapinfo;
};

#endif
