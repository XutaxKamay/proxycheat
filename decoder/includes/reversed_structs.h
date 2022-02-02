#pragma once
#include <dlfcn.h>
#include <link.h>
#include <proxycheat.h>
#include <netinet/in.h>
#include <algorithm>
#include <list>
#include <detour.h>

typedef void* (*CreateInterfaceFn)(const char* pName, int* pReturnCode);
typedef struct link_map link_map_t;

template <typename T = ptr_t*>
constexpr T vtable(ptr_t pAddress)
{
    return *(T*)(pAddress);
}

template <typename RetType = void, typename P = ptr_t, typename... vArgs>
constexpr RetType callvf(P pAddress, int iIndex, vArgs... pArgs)
{
    return ((RetType(*)(P, vArgs...))vtable(pAddress)[iIndex])(pAddress,
                                                               pArgs...);
}

class SendTable;
class CSendProxyRecipients;
class DVariant;
class SendProp_t;
class RecvProp;

typedef int (*ArrayLengthSendProxyFn)(const void* pStruct, int objectID);
typedef void (*SendVarProxyFn)(const SendProp_t* pProp,
                               const void* pStructBase,
                               const void* pData,
                               DVariant* pOut,
                               int iElement,
                               int objectID);

typedef void* (*SendTableProxyFn)(const SendProp_t* pProp,
                                  const void* pStructBase,
                                  const void* pData,
                                  CSendProxyRecipients* pRecipients,
                                  int objectID);

struct DVariant
{
    union
    {
        float m_Float;
        long m_Int;
        char* m_pString;
        void* m_pData; // For DataTables.
#if 0 // We can't ship this since it changes the size of DTVariant to be 20
      // bytes instead of 16 and that breaks MODs!!!
		float	m_Vector[4];
#else
        float m_Vector[3];
#endif
        int64_t m_Int64;
    };
    SendPropType_t m_Type;
};

struct SendProp_t
{
    ptr_t destructor;

    RecvProp* m_pMatchingRecvProp; // This is temporary and only used while

    SendPropType_t m_Type;
    int m_nBits;
    float m_fLowValue;
    float m_fHighValue;

    SendProp_t* m_pArrayProp; // If this is an array, this is the property that
                              // defines each array element.

    ArrayLengthSendProxyFn m_ArrayLengthProxy; // This callback returns the
                                               // array length.

    int m_nElements;     // Number of elements in the array (or 1 if it's not an
                         // array).
                         // ff ff ff ff 56
    int m_ElementStride; // ptr_t distance between array elements.

    char* m_pExcludeDTName; // If this is an exclude prop, then this is the name
                            // of the datatable to exclude a prop from.

    char* m_pParentArrayPropName;

    char* m_pVarName;
    float m_fHighLowMul;

    byte m_priority;

    int m_Flags; // SPROP_ flags.

    SendVarProxyFn m_ProxyFn;            // NULL for DPT_DataTable.
    SendTableProxyFn m_DataTableProxyFn; // Valid for DPT_DataTable.

    SendTable* m_pDataTable;

    // This also contains the NETWORKVAR_ flags shifted into the
    // SENDPROP_NETWORKVAR_FLAGS_SHIFT range. (Use GetNetworkVarFlags to access
    // them).
    int m_Offset;

    // Extra data bound to this property.
    const void* m_pExtraData;
};

struct new_SendTable
{
    std::vector<SendProp_t*> m_vecProps;
    char* m_pNetTableName;
};

struct SendTable
{
    SendProp_t* m_pProps;
    int m_nProps;

    char* m_pNetTableName; // The name matched between client and server.

    // The engine hooks the SendTable here.
    void* m_pPrecalc;

    bool m_bInitialized : 1;
    bool m_bHasBeenWritten : 1;
    bool m_bHasPropsEncodedAgainstCurrentTickCount : 1; // m_flSimulationTime
                                                        // and m_flAnimTime,
                                                        // e.g.
};

class ServerClass
{
 public:
    char* m_pNetworkName;
    SendTable* m_pTable;
    ServerClass* m_pNext;
    int m_ClassID; // Managed by the engine.

    // This is an index into the network string table
    // (sv.GetInstanceBaselineTable()).
    int m_InstanceBaselineIndex; // INVALID_STRING_INDEX if not initialized yet.
};

class IServerGameDLL
{
 public:
    ServerClass* GetAllClasses()
    {
        return callvf<ServerClass*>(this, 10);
    }
};

struct CBitRead_reversed
{
    char const* m_pDebugName;     // 0x38
    bool m_bOverflow;             // 0x40
    int m_nDataBits;              // 0x44
    size_t m_nDataBytes;          // 0x48
    uint32_t m_nInBufWord;        // 0x50
    int m_nBitsAvail;             // 0x54
    uint32_t const* m_pDataIn;    // 0x58
    uint32_t const* m_pBufferEnd; // 0x60
    uint32_t const* m_pData;      // 0x68
};

struct CBitWrite_reversed
{
    char const* m_pDebugName; // 0x38
    bool m_bOverflow;         // 0x40
    int m_nDataBits;          // 0x44
    size_t m_nDataBytes;      // 0x48
    uint32 m_nOutBufWord;
    int m_nOutBitsAvail;
    uint32* m_pDataOut;
    uint32* m_pBufferEnd;
    uint32* m_pData;
    bool m_bFlushed;
    int pad[0x100];

    CBitWrite_reversed(const char* pDebugName,
                       void* pData,
                       int nBytes,
                       int nBits = -1)
    {
        auto lm = reinterpret_cast<link_map_t*>(
            dlopen("bin/engine_client.so", RTLD_LAZY));

        // 644650
        ((void (*)(void*, const char*, void*, int, int))(
            lm->l_addr +
            0x644650))((void*)this, pDebugName, pData, nBytes, nBits);
    }
};

typedef struct netpacket_s
{
    netadr_t from; // sender IP
    uint8_t pad[0x2C];
    CBitRead_reversed message; // easy bitbuf data access 0x50
} netpacket_t;

typedef struct entityread_s
{
    uint8_t pad[0x24];
    int m_nHeaderCount; // 0x24
    uint8_t pad_2[8];   // 0x28
    bool m_bAsDelta;    // 0x2C
    uint8_t pad_3[7];
    CBitRead_reversed* msg;
} entityread_t;

constexpr auto size = sizeof(entityread_s);
