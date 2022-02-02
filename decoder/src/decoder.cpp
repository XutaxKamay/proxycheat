#include <decoder.h>
#include <algorithm>

std::vector<ServerClass_t> s_ServerClasses;
std::vector<CSVCMsg_SendTable> s_DataTables;
std::vector<ExcludeEntry> s_currentExcludes;

bool g_bDumpPacketEntities = false;
int s_nServerClassBits = 0;

int Int_Decode(CBitRead& entityBitBuffer,
               const CSVCMsg_SendTable::sendprop_t* pSendProp)
{
    int flags = pSendProp->flags();

    if (flags & SPROP_VARINT)
    {
        if (flags & SPROP_UNSIGNED)
        {
            return (int)entityBitBuffer.ReadVarInt32();
        }
        else
        {
            return entityBitBuffer.ReadSignedVarInt32();
        }
    }
    else
    {
        if (flags & SPROP_UNSIGNED)
        {
            return entityBitBuffer.ReadUBitLong(pSendProp->num_bits());
        }
        else
        {
            return entityBitBuffer.ReadSBitLong(pSendProp->num_bits());
        }
    }
}

// Look for special flags like SPROP_COORD, SPROP_NOSCALE, and SPROP_NORMAL and
// decode if they're there. Fills in fVal and returns true if it decodes
// anything.
static inline bool DecodeSpecialFloat(
    CBitRead& entityBitBuffer,
    const CSVCMsg_SendTable::sendprop_t* pSendProp,
    float& fVal)
{
    int flags = pSendProp->flags();

    if (flags & SPROP_COORD)
    {
        fVal = entityBitBuffer.ReadBitCoord();
        return true;
    }
    else if (flags & SPROP_COORD_MP)
    {
        fVal = entityBitBuffer.ReadBitCoordMP(kCW_None);
        return true;
    }
    else if (flags & SPROP_COORD_MP_LOWPRECISION)
    {
        fVal = entityBitBuffer.ReadBitCoordMP(kCW_LowPrecision);
        return true;
    }
    else if (flags & SPROP_COORD_MP_INTEGRAL)
    {
        fVal = entityBitBuffer.ReadBitCoordMP(kCW_Integral);
        return true;
    }
    else if (flags & SPROP_NOSCALE)
    {
        fVal = entityBitBuffer.ReadBitFloat();
        return true;
    }
    else if (flags & SPROP_NORMAL)
    {
        fVal = entityBitBuffer.ReadBitNormal();
        return true;
    }
    else if (flags & SPROP_CELL_COORD)
    {
        fVal = entityBitBuffer.ReadBitCellCoord(pSendProp->num_bits(),
                                                kCW_None);
        return true;
    }
    else if (flags & SPROP_CELL_COORD_LOWPRECISION)
    {
        fVal = entityBitBuffer.ReadBitCellCoord(pSendProp->num_bits(),
                                                kCW_LowPrecision);
        return true;
    }
    else if (flags & SPROP_CELL_COORD_INTEGRAL)
    {
        fVal = entityBitBuffer.ReadBitCellCoord(pSendProp->num_bits(),
                                                kCW_Integral);
        return true;
    }

    return false;
}

float Float_Decode(CBitRead& entityBitBuffer,
                   const CSVCMsg_SendTable::sendprop_t* pSendProp)
{
    float fVal = 0.0f;
    unsigned long dwInterp;

    // Check for special flags..
    if (DecodeSpecialFloat(entityBitBuffer, pSendProp, fVal))
    {
        return fVal;
    }

    dwInterp = entityBitBuffer.ReadUBitLong(pSendProp->num_bits());
    fVal = (float)dwInterp / ((1 << pSendProp->num_bits()) - 1);
    fVal = pSendProp->low_value() +
           (pSendProp->high_value() - pSendProp->low_value()) * fVal;
    return fVal;
}

void Vector_Decode(CBitRead& entityBitBuffer,
                   const CSVCMsg_SendTable::sendprop_t* pSendProp,
                   Vector& v)
{
    v.x = Float_Decode(entityBitBuffer, pSendProp);
    v.y = Float_Decode(entityBitBuffer, pSendProp);

    // Don't read in the third component for normals
    if ((pSendProp->flags() & SPROP_NORMAL) == 0)
    {
        v.z = Float_Decode(entityBitBuffer, pSendProp);
    }
    else
    {
        int signbit = entityBitBuffer.ReadOneBit();

        float v0v0v1v1 = v.x * v.x + v.y * v.y;
        if (v0v0v1v1 < 1.0f)
        {
            v.z = sqrtf(1.0f - v0v0v1v1);
        }
        else
        {
            v.z = 0.0f;
        }

        if (signbit)
        {
            v.z *= -1.0f;
        }
    }
}

void VectorXY_Decode(CBitRead& entityBitBuffer,
                     const CSVCMsg_SendTable::sendprop_t* pSendProp,
                     Vector& v)
{
    v.x = Float_Decode(entityBitBuffer, pSendProp);
    v.y = Float_Decode(entityBitBuffer, pSendProp);
}

const char* String_Decode(CBitRead& entityBitBuffer,
                          const CSVCMsg_SendTable::sendprop_t* pSendProp)
{
    // Read it in.
    int len = entityBitBuffer.ReadUBitLong(DT_MAX_STRING_BITS);

    char* tempStr = new char[len + 1];

    if (len >= DT_MAX_STRING_BUFFERSIZE)
    {
        printf("String_Decode( %s ) invalid length (%d)\n",
               pSendProp->var_name().c_str(),
               len);
        len = DT_MAX_STRING_BUFFERSIZE - 1;
    }

    entityBitBuffer.ReadBits(tempStr, len * 8);
    tempStr[len] = 0;

    return tempStr;
}

int64 Int64_Decode(CBitRead& entityBitBuffer,
                   const CSVCMsg_SendTable::sendprop_t* pSendProp)
{
    if (pSendProp->flags() & SPROP_VARINT)
    {
        if (pSendProp->flags() & SPROP_UNSIGNED)
        {
            return (int64)entityBitBuffer.ReadVarInt64();
        }
        else
        {
            return entityBitBuffer.ReadSignedVarInt64();
        }
    }
    else
    {
        uint32 highInt = 0;
        uint32 lowInt = 0;
        bool bNeg = false;
        if (!(pSendProp->flags() & SPROP_UNSIGNED))
        {
            bNeg = entityBitBuffer.ReadOneBit() != 0;
            lowInt = entityBitBuffer.ReadUBitLong(32);
            highInt = entityBitBuffer.ReadUBitLong(pSendProp->num_bits() - 32 -
                                                   1);
        }
        else
        {
            lowInt = entityBitBuffer.ReadUBitLong(32);
            highInt = entityBitBuffer.ReadUBitLong(pSendProp->num_bits() - 32);
        }

        int64 temp;

        uint32* pInt = (uint32*)&temp;
        *pInt++ = lowInt;
        *pInt = highInt;

        if (bNeg)
        {
            temp = -temp;
        }

        return temp;
    }
}

Prop_t* Array_Decode(CBitRead& entityBitBuffer,
                     FlattenedPropEntry* pFlattenedProp,
                     int nNumElements,
                     uint32 uClass,
                     int nFieldIndex,
                     bool bQuiet)
{
    int maxElements = nNumElements;
    int numBits = 1;
    while ((maxElements >>= 1) != 0)
    {
        numBits++;
    }

    int nElements = entityBitBuffer.ReadUBitLong(numBits);

    Prop_t* pResult = NULL;
    pResult = new Prop_t[nElements];

    if (!bQuiet)
    {
        printf("array with %d elements of %d max\n", nElements, nNumElements);
    }

    for (int i = 0; i < nElements; i++)
    {
        FlattenedPropEntry temp(pFlattenedProp->m_arrayElementProp, NULL);
        Prop_t* pElementResult =
            DecodeProp(entityBitBuffer, &temp, uClass, nFieldIndex, bQuiet);
        pResult[i] = *pElementResult;
        delete pElementResult;
        pResult[i].m_nNumElements = nElements - i;
    }

    return pResult;
}

Prop_t* DecodeProp(CBitRead& entityBitBuffer,
                   FlattenedPropEntry* pFlattenedProp,
                   uint32 uClass,
                   int nFieldIndex,
                   bool bQuiet)
{
    const CSVCMsg_SendTable::sendprop_t* pSendProp = pFlattenedProp->m_prop;

    Prop_t* pResult = NULL;
    if (pSendProp->type() != DPT_Array && pSendProp->type() != DPT_DataTable)
    {
        pResult = new Prop_t((SendPropType_t)(pSendProp->type()));
    }

    if (!bQuiet)
    {
        printf("Field: %d, %s = ", nFieldIndex, pSendProp->var_name().c_str());
    }
    switch (pSendProp->type())
    {
        case DPT_Int:
            pResult->m_value.m_int = Int_Decode(entityBitBuffer, pSendProp);
            break;
        case DPT_Float:
            pResult->m_value.m_float = Float_Decode(entityBitBuffer, pSendProp);
            break;
        case DPT_Vector:
            Vector_Decode(entityBitBuffer, pSendProp, pResult->m_value.m_vector);
            break;
        case DPT_VectorXY:
            VectorXY_Decode(entityBitBuffer,
                            pSendProp,
                            pResult->m_value.m_vector);
            break;
        case DPT_String:
            pResult->m_value.m_pString = String_Decode(entityBitBuffer,
                                                       pSendProp);
            break;
        case DPT_Array:
            pResult = Array_Decode(entityBitBuffer,
                                   pFlattenedProp,
                                   pSendProp->num_elements(),
                                   uClass,
                                   nFieldIndex,
                                   bQuiet);
            break;
        case DPT_DataTable:
            break;
        case DPT_Int64:
            pResult->m_value.m_int64 = Int64_Decode(entityBitBuffer, pSendProp);
            break;
    }
    if (!bQuiet)
    {
        pResult->Print();
    }

    return pResult;
}

CSVCMsg_SendTable* GetTableByClassID(uint32 nClassID)
{
    for (uint32 i = 0; i < s_ServerClasses.size(); i++)
    {
        if (s_ServerClasses[i].nClassID == (int32)nClassID)
        {
            return &(s_DataTables[s_ServerClasses[i].nDataTable]);
        }
    }
    return NULL;
}

CSVCMsg_SendTable* GetTableByName(const char* pName)
{
    for (unsigned int i = 0; i < s_DataTables.size(); i++)
    {
        if (s_DataTables[i].net_table_name().compare(pName) == 0)
        {
            return &(s_DataTables[i]);
        }
    }
    return NULL;
}

FlattenedPropEntry* GetSendPropByIndex(uint32 uClass, uint32 uIndex)
{
    if (uIndex < s_ServerClasses[uClass].flattenedProps.size())
    {
        return &s_ServerClasses[uClass].flattenedProps[uIndex];
    }
    return NULL;
}

bool IsPropExcluded(CSVCMsg_SendTable* pTable,
                    const CSVCMsg_SendTable::sendprop_t& checkSendProp)
{
    for (unsigned int i = 0; i < s_currentExcludes.size(); i++)
    {
        if (pTable->net_table_name().compare(s_currentExcludes[i].m_pDTName) ==
                0 &&
            checkSendProp.var_name().compare(s_currentExcludes[i].m_pVarName) ==
                0)
        {
            return true;
        }
    }
    return false;
}

void GatherExcludes(CSVCMsg_SendTable* pTable)
{
    for (int iProp = 0; iProp < pTable->props_size(); iProp++)
    {
        const CSVCMsg_SendTable::sendprop_t& sendProp = pTable->props(iProp);
        if (sendProp.flags() & SPROP_EXCLUDE)
        {
            s_currentExcludes.push_back(
                ExcludeEntry(sendProp.var_name().c_str(),
                             sendProp.dt_name().c_str(),
                             pTable->net_table_name().c_str()));
        }

        if (sendProp.type() == DPT_DataTable)
        {
            CSVCMsg_SendTable* pSubTable = GetTableByName(
                sendProp.dt_name().c_str());
            if (pSubTable != NULL)
            {
                GatherExcludes(pSubTable);
            }
        }
    }
}

void GatherProps(CSVCMsg_SendTable* pTable, int nServerClass);

void GatherProps_IterateProps(CSVCMsg_SendTable* pTable,
                              int nServerClass,
                              std::vector<FlattenedPropEntry>& flattenedProps)
{
    for (int iProp = 0; iProp < pTable->props_size(); iProp++)
    {
        const CSVCMsg_SendTable::sendprop_t& sendProp = pTable->props(iProp);

        if ((sendProp.flags() & SPROP_INSIDEARRAY) ||
            (sendProp.flags() & SPROP_EXCLUDE) ||
            IsPropExcluded(pTable, sendProp))
        {
            continue;
        }

        if (sendProp.type() == DPT_DataTable)
        {
            CSVCMsg_SendTable* pSubTable = GetTableByName(
                sendProp.dt_name().c_str());
            if (pSubTable != NULL)
            {
                if (sendProp.flags() & SPROP_COLLAPSIBLE)
                {
                    GatherProps_IterateProps(pSubTable,
                                             nServerClass,
                                             flattenedProps);
                }
                else
                {
                    GatherProps(pSubTable, nServerClass);
                }
            }
        }
        else
        {
            if (sendProp.type() == DPT_Array)
            {
                flattenedProps.push_back(
                    FlattenedPropEntry(&sendProp, &(pTable->props(iProp - 1))));
            }
            else
            {
                flattenedProps.push_back(FlattenedPropEntry(&sendProp, NULL));
            }
        }
    }
}

void GatherProps(CSVCMsg_SendTable* pTable, int nServerClass)
{
    std::vector<FlattenedPropEntry> tempFlattenedProps;
    GatherProps_IterateProps(pTable, nServerClass, tempFlattenedProps);

    std::vector<FlattenedPropEntry>& flattenedProps =
        s_ServerClasses[nServerClass].flattenedProps;
    for (uint32 i = 0; i < tempFlattenedProps.size(); i++)
    {
        flattenedProps.push_back(tempFlattenedProps[i]);
    }
}

void FlattenDataTable(int nServerClass)
{
    CSVCMsg_SendTable* pTable =
        &s_DataTables[s_ServerClasses[nServerClass].nDataTable];

    s_currentExcludes.clear();
    GatherExcludes(pTable);

    GatherProps(pTable, nServerClass);

    std::vector<FlattenedPropEntry>& flattenedProps =
        s_ServerClasses[nServerClass].flattenedProps;

    // get priorities
    std::vector<uint32> priorities;
    priorities.push_back(64);
    for (unsigned int i = 0; i < flattenedProps.size(); i++)
    {
        uint32 priority = flattenedProps[i].m_prop->priority();

        bool bFound = false;
        for (uint32 j = 0; j < priorities.size(); j++)
        {
            if (priorities[j] == priority)
            {
                bFound = true;
                break;
            }
        }

        if (!bFound)
        {
            priorities.push_back(priority);
        }
    }

    std::sort(priorities.begin(), priorities.end());

    // sort flattenedProps by priority
    uint32 start = 0;
    for (uint32 priority_index = 0; priority_index < priorities.size();
         ++priority_index)
    {
        uint32 priority = priorities[priority_index];

        while (true)
        {
            uint32 currentProp = start;
            while (currentProp < flattenedProps.size())
            {
                const CSVCMsg_SendTable::sendprop_t* prop =
                    flattenedProps[currentProp].m_prop;

                if (prop->priority() == (int32)priority ||
                    (priority == 64 && (SPROP_CHANGES_OFTEN & prop->flags())))
                {
                    if (start != currentProp)
                    {
                        FlattenedPropEntry temp = flattenedProps[start];
                        flattenedProps[start] = flattenedProps[currentProp];
                        flattenedProps[currentProp] = temp;
                    }
                    start++;
                    break;
                }
                currentProp++;
            }

            if (currentProp == flattenedProps.size())
                break;
        }
    }
}

int ReadFieldIndex(CBitRead& entityBitBuffer, int lastIndex, bool bNewWay)
{
    if (bNewWay)
    {
        if (entityBitBuffer.ReadOneBit())
        {
            return lastIndex + 1;
        }
    }

    int ret = 0;
    if (bNewWay && entityBitBuffer.ReadOneBit())
    {
        ret = entityBitBuffer.ReadUBitLong(3); // read 3 bits
    }
    else
    {
        ret = entityBitBuffer.ReadUBitLong(7); // read 7 bits
        switch (ret & (32 | 64))
        {
            case 32:
                ret = (ret & ~96) | (entityBitBuffer.ReadUBitLong(2) << 5);
                assert(ret >= 32);
                break;
            case 64:
                ret = (ret & ~96) | (entityBitBuffer.ReadUBitLong(4) << 5);
                assert(ret >= 128);
                break;
            case 96:
                ret = (ret & ~96) | (entityBitBuffer.ReadUBitLong(7) << 5);
                assert(ret >= 512);
                break;
        }
    }

    if (ret == 0xFFF) // end marker is 4095 for cs:go
    {
        return -1;
    }

    return lastIndex + 1 + ret;
}

bool ReadNewEntity(CBitRead& entityBitBuffer, EntityEntry* pEntity)
{
    bool bNewWay = (entityBitBuffer.ReadOneBit() ==
                    1); // 0 = old way, 1 = new way

    std::vector<int> fieldIndices;

    int index = -1;
    do
    {
        index = ReadFieldIndex(entityBitBuffer, index, bNewWay);
        if (index != -1)
        {
            fieldIndices.push_back(index);
        }
    } while (index != -1);

    for (unsigned int i = 0; i < fieldIndices.size(); i++)
    {
        FlattenedPropEntry* pSendProp = GetSendPropByIndex(pEntity->m_uClass,
                                                           fieldIndices[i]);
        if (pSendProp)
        {
            Prop_t* pProp = DecodeProp(entityBitBuffer,
                                       pSendProp,
                                       pEntity->m_uClass,
                                       fieldIndices[i],
                                       !g_bDumpPacketEntities);
            pEntity->AddOrUpdateProp(pSendProp, pProp);
        }
        else
        {
            return false;
        }
    }

    return true;
}

void RecvTable_ReadInfos(const CSVCMsg_SendTable& msg)
{
    printf("%s:%d\n", msg.net_table_name().c_str(), msg.props_size());

    for (int iProp = 0; iProp < msg.props_size(); iProp++)
    {
        const CSVCMsg_SendTable::sendprop_t& sendProp = msg.props(iProp);

        if ((sendProp.type() == DPT_DataTable) ||
            (sendProp.flags() & SPROP_EXCLUDE))
        {
            printf("%d:%06X:%s:%s%s\n",
                   sendProp.type(),
                   sendProp.flags(),
                   sendProp.var_name().c_str(),
                   sendProp.dt_name().c_str(),
                   (sendProp.flags() & SPROP_EXCLUDE) ? " exclude" : "");
        }
        else if (sendProp.type() == DPT_Array)
        {
            printf("%d:%06X:%s[%d]\n",
                   sendProp.type(),
                   sendProp.flags(),
                   sendProp.var_name().c_str(),
                   sendProp.num_elements());
        }
        else
        {
            printf("%d:%06X:%s:%f,%f,%08X%s\n",
                   sendProp.type(),
                   sendProp.flags(),
                   sendProp.var_name().c_str(),
                   sendProp.low_value(),
                   sendProp.high_value(),
                   sendProp.num_bits(),
                   (sendProp.flags() & SPROP_INSIDEARRAY) ? " inside array" :
                                                            "");
        }
    }
}

bool parseDumpDataFile(const char* filename)
{
    s_DataTables.clear();
    s_ServerClasses.clear();

    std::ifstream streamFile(filename, std::ifstream::binary);

    if (!streamFile.is_open())
    {
        std::cout << "Couldn't open file " << filename << std::endl;
        return false;
    }

    streamFile.seekg(0, streamFile.end);

    auto fileSize = streamFile.tellg();

    streamFile.seekg(0, streamFile.beg);

    unsigned char* pBuf = reinterpret_cast<unsigned char*>(malloc(fileSize));
    void* pBufBackup = pBuf;

    streamFile.read(reinterpret_cast<char*>(pBuf), fileSize);

    streamFile.close();

    int size = *reinterpret_cast<int*>(pBuf);
    pBuf += sizeof(size);

    std::cout << "datatables count: " << size << '(' << sizeof(size) << ')'
              << std::endl;

    for (int i = 0; i < size; i++)
    {
        auto bytesize = *reinterpret_cast<int*>(pBuf);
        pBuf += sizeof(bytesize);

        CSVCMsg_SendTable msg;
        msg.ParseFromArray(pBuf, bytesize);
        s_DataTables.push_back(msg);

        printf("%i %s\n", bytesize, msg.net_table_name().c_str());
        RecvTable_ReadInfos(msg);

        pBuf += bytesize;
    }

    size = *reinterpret_cast<int*>(pBuf);
    pBuf += sizeof(size);

    std::cout << "serverclasses count: " << size << std::endl;

    for (int i = 0; i < size; i++)
    {
        ServerClass_t serverclass;
        serverclass.nClassID = *reinterpret_cast<int*>(pBuf);
        pBuf += sizeof(int);

        serverclass.nDataTable = *reinterpret_cast<int*>(pBuf);
        pBuf += sizeof(int);

        memcpy(serverclass.strDTName, pBuf, 256);
        pBuf += (uintptr_t)256;

        memcpy(serverclass.strName, pBuf, 256);

        printf("classid %i, datatable: %i dtname: %s name: %s\n",
               serverclass.nClassID,
               serverclass.nDataTable,
               serverclass.strDTName,
               serverclass.strName);

        s_ServerClasses.push_back(serverclass);

        if (i == size - 1)
        {
            break;
        }

        pBuf += (uintptr_t)256;
    }

    free(pBufBackup);

    std::cout << "parsed file " << filename << std::endl;

    for (size_t i = 0; i < s_ServerClasses.size(); i++)
    {
        FlattenDataTable(i);
    }

    int nTemp = s_ServerClasses.size();
    s_nServerClassBits = 0;
    while (nTemp >>= 1)
        ++s_nServerClassBits;

    s_nServerClassBits++;

    printf("serversclassbits: %i\n", s_nServerClassBits);

    return true;
}
