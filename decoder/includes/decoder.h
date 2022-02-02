#pragma once
#include <google/protobuf/descriptor.h>
#include <google/protobuf/reflection_ops.h>
#include <google/protobuf/descriptor.pb.h>
#include <cstrike15_usermessages.pb.h>
#include <netmessages.pb.h>
#include <iostream>
#include <vector>
#include <fstream>
#include <platform.h>
#include <bitread.h>

#ifndef DECODER_H
    #define DECODER_H

struct ServerClass_t;

extern int s_nServerClassBits;
extern bool g_bDumpPacketEntities;

enum SendPropType_t
{
    DPT_Int = 0,
    DPT_Float,
    DPT_Vector,
    DPT_VectorXY, // Only encodes the XY of a vector, ignores Z
    DPT_String,
    DPT_Array, // An array of the base types (can't be of datatables).
    DPT_DataTable,
    DPT_Int64,
    DPT_NUMSendPropTypes
};

enum UpdateType
{
    EnterPVS = 0, // Entity came back into pvs, create new entity if one doesn't
                  // exist
    LeavePVS,     // Entity left pvs
    DeltaEnt,     // There is a delta for this entity.
    PreserveEnt,  // Entity stays alive but no delta ( could be LOD, or just
                  // unchanged )
    Finished,     // finished parsing entities successfully
    Failed,       // parsing error occured while reading entities
};

// Flags for delta encoding header
enum HeaderFlags
{
    FHDR_ZERO = 0x0000,
    FHDR_LEAVEPVS = 0x0001,
    FHDR_DELETE = 0x0002,
    FHDR_ENTERPVS = 0x0004,
};

    // How many bits to use to encode an edict.
    #define MAX_EDICT_BITS 11 // # of bits needed to represent max edicts
    // Max # of edicts in a level
    #define MAX_EDICTS (1 << MAX_EDICT_BITS)

    #define MAX_USERDATA_BITS 14
    #define MAX_USERDATA_SIZE (1 << MAX_USERDATA_BITS)
    #define SUBSTRING_BITS 5

    #define NUM_NETWORKED_EHANDLE_SERIAL_NUMBER_BITS 10

    #define MAX_PLAYER_NAME_LENGTH 128
    #define MAX_CUSTOM_FILES 4 // max 4 files
    #define SIGNED_GUID_LEN \
        32 // Hashed CD Key (32 hex alphabetic chars + 0 terminator )

    #define ENTITY_SENTINEL 9999

    #define MAX_STRING_TABLES 64

    #define SPROP_UNSIGNED (1 << 0) // Unsigned integer data.
    #define SPROP_COORD \
        (1              \
         << 1) // If this is set, the float/vector is treated like a world
               // coordinate. Note that the bit count is ignored in this case.
    #define SPROP_NOSCALE \
        (1 << 2) // For floating point, don't scale into range, just take value
                 // as is.
    #define SPROP_ROUNDDOWN \
        (1 << 3) // For floating point, limit high value to range minus one bit
                 // unit
    #define SPROP_ROUNDUP \
        (1 << 4) // For floating point, limit low value to range minus one bit
                 // unit
    #define SPROP_NORMAL \
        (1 << 5) // If this is set, the vector is treated like a normal (only
                 // valid for vectors)
    #define SPROP_EXCLUDE \
        (1 << 6) // This is an exclude prop (not excludED, but it points at
                 // another prop to be excluded).
    #define SPROP_XYZE (1 << 7) // Use XYZ/Exponent encoding for vectors.
    #define SPROP_INSIDEARRAY \
        (1 << 8) // This tells us that the property is inside an array, so it
                 // shouldn't be put into the flattened property list. Its array
                 // will point at it when it needs to.
    #define SPROP_PROXY_ALWAYS_YES \
        (1 << 9) // Set for datatable props using one of the default datatable
                 // proxies like SendProxy_DataTableToDataTable that always send
                 // the data to all clients.
    #define SPROP_IS_A_VECTOR_ELEM \
        (1 << 10) // Set automatically if SPROP_VECTORELEM is used.
    #define SPROP_COLLAPSIBLE \
        (1 << 11) // Set automatically if it's a datatable with an offset of 0
                  // that doesn't change the pointer (ie: for all
                  // automatically-chained base classes).
    #define SPROP_COORD_MP \
        (1 << 12) // Like SPROP_COORD, but special handling for multiplayer
                  // games
    #define SPROP_COORD_MP_LOWPRECISION \
        (1 << 13) // Like SPROP_COORD, but special handling for multiplayer
                  // games where the fractional component only gets a 3 bits
                  // instead of 5
    #define SPROP_COORD_MP_INTEGRAL \
        (1 << 14) // SPROP_COORD_MP, but coordinates are rounded to integral
                  // boundaries
    #define SPROP_CELL_COORD \
        (1 << 15) // Like SPROP_COORD, but special encoding for cell coordinates
                  // that can't be negative, bit count indicate maximum value
    #define SPROP_CELL_COORD_LOWPRECISION \
        (1 << 16) // Like SPROP_CELL_COORD, but special handling where the
                  // fractional component only gets a 3 bits instead of 5
    #define SPROP_CELL_COORD_INTEGRAL \
        (1 << 17) // SPROP_CELL_COORD, but coordinates are rounded to integral
                  // boundaries
    #define SPROP_CHANGES_OFTEN \
        (1 << 18) // this is an often changed field, moved to head of sendtable
                  // so it gets a small index
    #define SPROP_VARINT \
        (1 << 19) // use var int encoded (google protobuf style), note you want
                  // to include SPROP_UNSIGNED if needed, its more efficient

    #define DT_MAX_STRING_BITS 9
    #define DT_MAX_STRING_BUFFERSIZE \
        (1 << DT_MAX_STRING_BITS) // Maximum length of a string that can be
                                  // sent.

struct Prop_t
{
    Prop_t()
    {}

    Prop_t(SendPropType_t type) : m_type(type), m_nNumElements(0)
    {
        // this makes all possible types init to 0's
        m_value.m_vector.Init();
    }

    void Print(int nMaxElements = 0)
    {
        if (m_nNumElements > 0)
        {
            printf(" Element: %d  ",
                   (nMaxElements ? nMaxElements : m_nNumElements) -
                       m_nNumElements);
        }

        switch (m_type)
        {
            case DPT_Int:
            {
                printf("%d\n", m_value.m_int);
            }
            break;
            case DPT_Float:
            {
                printf("%f\n", m_value.m_float);
            }
            break;
            case DPT_Vector:
            {
                printf("%f, %f, %f\n",
                       m_value.m_vector.x,
                       m_value.m_vector.y,
                       m_value.m_vector.z);
            }
            break;
            case DPT_VectorXY:
            {
                printf("%f, %f\n", m_value.m_vector.x, m_value.m_vector.y);
            }
            break;
            case DPT_String:
            {
                printf("%s\n", m_value.m_pString);
            }
            break;
            case DPT_Array:
                break;
            case DPT_DataTable:
                break;
            case DPT_Int64:
            {
                printf("%ld\n", m_value.m_int64);
            }
            break;
            default:
                break;
        }

        if (m_nNumElements > 1)
        {
            Prop_t* pProp = this;
            pProp[1].Print(nMaxElements ? nMaxElements : m_nNumElements);
        }
    }

    SendPropType_t m_type;
    union
    {
        int m_int;
        float m_float;
        const char* m_pString;
        int64 m_int64;
        Vector m_vector;
    } m_value;
    int m_nNumElements;
};

struct FlattenedPropEntry
{
    FlattenedPropEntry(const CSVCMsg_SendTable::sendprop_t* prop,
                       const CSVCMsg_SendTable::sendprop_t* arrayElementProp) :
        m_prop(prop),
        m_arrayElementProp(arrayElementProp)
    {}
    const CSVCMsg_SendTable::sendprop_t* m_prop;
    const CSVCMsg_SendTable::sendprop_t* m_arrayElementProp;
};

struct ServerClass_t
{
    int nClassID;
    char strName[256];
    char strDTName[256];
    int nDataTable;

    std::vector<FlattenedPropEntry> flattenedProps;
};

struct ExcludeEntry
{
    ExcludeEntry(const char* pVarName,
                 const char* pDTName,
                 const char* pDTExcluding) :
        m_pVarName(pVarName),
        m_pDTName(pDTName), m_pDTExcluding(pDTExcluding)
    {}

    const char* m_pVarName;
    const char* m_pDTName;
    const char* m_pDTExcluding;
};

struct PropEntry
{
    PropEntry(FlattenedPropEntry* pFlattenedProp, Prop_t* pPropValue) :
        m_pFlattenedProp(pFlattenedProp), m_pPropValue(pPropValue)
    {}
    ~PropEntry()
    {
        delete m_pPropValue;
    }

    FlattenedPropEntry* m_pFlattenedProp;
    Prop_t* m_pPropValue;
};

struct EntityEntry
{
    EntityEntry(int nEntity, uint32 uClass, uint32 uSerialNum) :
        m_nEntity(nEntity), m_uClass(uClass), m_uSerialNum(uSerialNum)
    {}

    ~EntityEntry()
    {
        for (std::vector<PropEntry*>::iterator i = m_props.begin();
             i != m_props.end();
             i++)
        {
            delete *i;
        }
    }

    PropEntry* FindProp(const char* pName)
    {
        for (std::vector<PropEntry*>::iterator i = m_props.begin();
             i != m_props.end();
             i++)
        {
            PropEntry* pProp = *i;
            if (pProp->m_pFlattenedProp->m_prop->var_name().compare(pName) == 0)
            {
                return pProp;
            }
        }
        return NULL;
    }
    void AddOrUpdateProp(FlattenedPropEntry* pFlattenedProp, Prop_t* pPropValue)
    {
        // if ( m_uClass == 34 && pFlattenedProp->m_prop->var_name().compare(
        // "m_vecOrigin" ) == 0 )
        //{
        //	printf("got vec origin!\n" );
        //}
        PropEntry* pProp = FindProp(pFlattenedProp->m_prop->var_name().c_str());
        if (pProp)
        {
            delete pProp->m_pPropValue;
            pProp->m_pPropValue = pPropValue;
        }
        else
        {
            pProp = new PropEntry(pFlattenedProp, pPropValue);
            m_props.push_back(pProp);
        }
    }

    int m_nEntity;
    uint32 m_uClass;
    uint32 m_uSerialNum;
    bool m_bDraw;
    bool m_bUpdated;

    std::vector<PropEntry*> m_props;
};

struct EntityHeaderInfo
{
    EntityHeaderInfo()
    {
        m_nHighestEntity = -1;
        m_nNewEntity = -1;
        m_nHeaderBase = -1;
    }

    inline void NextOldEntity()
    {}

    int m_UpdateFlags;
    int m_nHeaderBase;
    int m_nNewEntity;
    int m_nHighestEntity;
};

CSVCMsg_SendTable* GetTableByClassID(uint32 nClassID);
CSVCMsg_SendTable* GetTableByName(const char* pName);
FlattenedPropEntry* GetSendPropByIndex(uint32 uClass, uint32 uIndex);
bool IsPropExcluded(CSVCMsg_SendTable* pTable,
                    const CSVCMsg_SendTable::sendprop_t& checkSendProp);
void GatherExcludes(CSVCMsg_SendTable* pTable);
void GatherProps(CSVCMsg_SendTable* pTable, int nServerClass);
void GatherProps_IterateProps(CSVCMsg_SendTable* pTable,
                              int nServerClass,
                              std::vector<FlattenedPropEntry>& flattenedProps);
void GatherProps(CSVCMsg_SendTable* pTable, int nServerClass);
void FlattenDataTable(int nServerClass);
Prop_t* DecodeProp(CBitRead& entityBitBuffer,
                   FlattenedPropEntry* pFlattenedProp,
                   uint32 uClass,
                   int nFieldIndex,
                   bool bQuiet);
bool parseDumpDataFile(const char* filename);
int ReadFieldIndex(CBitRead& entityBitBuffer, int lastIndex, bool bNewWay);
bool ReadNewEntity(CBitRead& entityBitBuffer, EntityEntry* pEntity);
bool ReadFromBuffer(CBitRead& buffer, void** pBuffer, int& size);
#endif
