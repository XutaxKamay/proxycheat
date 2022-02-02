#include <chrono>
#include <proxycheat.h>
#include <sys/types.h>
#include <unistd.h>

std::vector<netchan_t*> netchan_t::g_vecNetChannels;
double netchan_t::m_netTime = 0.0;

CSplitPacketEntry* netchan_t::NET_FindOrCreateSplitPacketEntry(netadr_t* from)
{
    vecSplitPacketEntries_t& splitPacketEntries = m_netSplitPacket;
    int i, count = splitPacketEntries.size();
    CSplitPacketEntry* entry = NULL;
    for (i = 0; i < count; i++)
    {
        entry = &splitPacketEntries[i];
        assert(entry);

        if (from->CompareAdr(&entry->from))
            break;
    }

    if (i >= count)
    {
        CSplitPacketEntry newentry;
        memcpy(&newentry.from, from, sizeof(netadr_t));

        splitPacketEntries.push_back(newentry);

        entry = &splitPacketEntries[splitPacketEntries.size() - 1];
    }

    assert(entry);
    return entry;
}

void netchan_t::NET_DiscardStaleSplitpackets()
{
    vecSplitPacketEntries_t& splitPacketEntries = m_netSplitPacket;
    int i;
    for (i = splitPacketEntries.size() - 1; i >= 0; i--)
    {
        CSplitPacketEntry* entry = &splitPacketEntries[i];

        if (m_netTime < (entry->lastactivetime + SPLIT_PACKET_STALE_TIME))
            continue;

        splitPacketEntries.erase(splitPacketEntries.begin() + i);
    }
}

bool netchan_t::NET_GetLong(unsigned char*& packet,
                            uint32_t& packet_size,
                            netadr_t* netadr)
{
    int packetNumber, packetCount, sequenceNumber, offset;
    short packetID;
    SPLITPACKET* pHeader;

    if (packet_size < (int)sizeof(SPLITPACKET))
    {
        printf("Invalid split packet length %i\n", packet_size);
        return false;
    }

    CSplitPacketEntry* entry = NET_FindOrCreateSplitPacketEntry(netadr);
    assert(entry);
    if (!entry)
        return false;

    entry->lastactivetime = m_netTime;
    assert(netadr->CompareAdr(&entry->from));

    pHeader = (SPLITPACKET*)packet;
    // pHeader is network endian correct
    sequenceNumber = LittleLong(pHeader->sequenceNumber);
    packetID = LittleShort(pHeader->packetID);
    // High byte is packet number
    packetNumber = (packetID >> 8);
    // Low byte is number of total packets
    packetCount = (packetID & 0xff);

    int nSplitSizeMinusHeader = (int)LittleShort(pHeader->nSplitSize);
    if (nSplitSizeMinusHeader < (int)MIN_SPLIT_SIZE ||
        nSplitSizeMinusHeader > (int)MAX_SPLIT_SIZE)
    {
        return false;
    }

    if (packetNumber >= (int)MAX_SPLITPACKET_SPLITS ||
        packetCount > (int)MAX_SPLITPACKET_SPLITS)
    {
        return false;
    }

    // First packet in split series?
    if (entry->netsplit.currentSequence == -1 ||
        sequenceNumber != entry->netsplit.currentSequence)
    {
        entry->netsplit.currentSequence = sequenceNumber;
        entry->netsplit.splitCount = packetCount;
        entry->netsplit.nExpectedSplitSize = nSplitSizeMinusHeader;
    }

    if (entry->netsplit.nExpectedSplitSize != nSplitSizeMinusHeader)
    {
        printf("NET_GetLong:  Split packet with inconsistent split size "
               "(number %i/ count %i) where size %i not equal to initial size "
               "of "
               "%i\n",
               packetNumber,
               packetCount,
               nSplitSizeMinusHeader,
               entry->netsplit.nExpectedSplitSize);
        return false;
    }

    int size = packet_size - sizeof(SPLITPACKET);

    if (entry->splitflags[packetNumber] != sequenceNumber)
    {
        // Last packet in sequence? set size
        if (packetNumber == (packetCount - 1))
        {
            entry->netsplit.totalSize = (packetCount - 1) *
                                            nSplitSizeMinusHeader +
                                        size;
        }

        entry->netsplit.splitCount--; // Count packet
        entry->splitflags[packetNumber] = sequenceNumber;
    }
    else
    {
        printf("NET_GetLong:  Ignoring duplicated split packet %i of %i ( %i "
               "bytes )\n",
               packetNumber + 1,
               packetCount,
               size);
    }

    // Copy the incoming data to the appropriate place in the buffer
    offset = (packetNumber * nSplitSizeMinusHeader);
    memcpy(entry->netsplit.buffer + offset, packet + sizeof(SPLITPACKET), size);

    // Have we received all of the pieces to the packet?
    if (entry->netsplit.splitCount <= 0)
    {
        entry->netsplit.currentSequence = -1; // Clear packet
        if (entry->netsplit.totalSize > (int)sizeof(entry->netsplit.buffer))
        {
            printf("Split packet too large! %d bytes\n",
                   entry->netsplit.totalSize);
            return false;
        }

        packet_size = (uint32_t)entry->netsplit.totalSize;
        free(packet);
        packet = (unsigned char*)malloc(packet_size);
        memcpy(packet, entry->netsplit.buffer, packet_size);
        return true;
    }

    return false;
}

void netchan_t::NET_UpdateTime()
{
    static double s_last_realtime = 0;
    using namespace std::chrono;
    auto realtime = std::chrono::duration<double>(
                        system_clock::now().time_since_epoch())
                        .count();

    double frametime = realtime - s_last_realtime;
    s_last_realtime = realtime;

    if (frametime > 1.0)
    {
        // if we have very long frame times because of loading stuff
        // don't apply that to net time to avoid unwanted timeouts
        frametime = 1.0;
    }
    else if (frametime < 0.0)
    {
        frametime = 0.0;
    }

    // TODO: adjust network time so fakelag works with host_timescale
    m_netTime += frametime;
}

EntityEntry* netchan_t::FindEntity(int nEntity)
{
    for (std::vector<EntityEntry*>::iterator i = m_vecEntities.begin();
         i != m_vecEntities.end();
         i++)
    {
        if ((*i)->m_nEntity == nEntity)
        {
            return *i;
        }
    }

    return nullptr;
}

int netchan_t::FindHighestEntity()
{
    int index = -1;

    for (std::vector<EntityEntry*>::iterator i = m_vecEntities.begin();
         i != m_vecEntities.end();
         i++)
    {
        if ((*i)->m_nEntity > index)
        {
            index = (*i)->m_nEntity;
        }
    }

    return index;
}

EntityEntry* netchan_t::AddEntity(int nEntity, uint32 uClass, uint32 uSerialNum)
{
    // if entity already exists, then replace it, else add it
    EntityEntry* pEntity = FindEntity(nEntity);
    if (pEntity)
    {
        pEntity->m_uClass = uClass;
        pEntity->m_uSerialNum = uSerialNum;
    }
    else
    {
        pEntity = new EntityEntry(nEntity, uClass, uSerialNum);
        m_vecEntities.push_back(pEntity);
    }

    return pEntity;
}

void netchan_t::RemoveEntity(int nEntity)
{
    for (std::vector<EntityEntry*>::iterator i = m_vecEntities.begin();
         i != m_vecEntities.end();
         i++)
    {
        EntityEntry* pEntity = *i;
        if (pEntity->m_nEntity == nEntity)
        {
            m_vecEntities.erase(i);
            delete pEntity;
            break;
        }
    }
}

void netchan_t::ParseDeltaHeader(EntityHeaderInfo& u, CBitRead* pBuf)
{
    u.m_UpdateFlags = FHDR_ZERO;

    u.m_nNewEntity = u.m_nHeaderBase + 1 + pBuf->ReadUBitVar();

    u.m_nHeaderBase = u.m_nNewEntity;

    // leave pvs flag
    if (pBuf->ReadOneBit() == 0)
    {
        // enter pvs flag
        if (pBuf->ReadOneBit() != 0)
        {
            u.m_UpdateFlags |= FHDR_ENTERPVS;
        }
    }
    else
    {
        u.m_UpdateFlags |= FHDR_LEAVEPVS;

        // Force delete flag
        if (pBuf->ReadOneBit() != 0)
        {
            u.m_UpdateFlags |= FHDR_DELETE;
        }
    }
}

void netchan_t::parseEnts(CSVCMsg_PacketEntities& msg)
{
    CBitRead entityBitBuffer(msg.entity_data().data(),
                             msg.entity_data().size());
    bool bAsDelta = msg.is_delta();
    int nHeaderCount = msg.updated_entries();
    // int nMaxEntries = msg.max_entries();
    // int nDeltaFrom = msg.delta_from();
    // int nBaseline = msg.baseline();
    // bool bUpdateBaselines = msg.update_baseline();

    // (void)bUpdateBaselines;
    // (void)nBaseline;
    // (void)nDeltaFrom;
    // (void)nMaxEntries;

    EntityHeaderInfo info;
    info.m_nHighestEntity = FindHighestEntity();

    // printf("Reading entries\n");
    UpdateType updateType = PreserveEnt;

    while (updateType < Finished)
    {
        nHeaderCount--;

        bool bIsEntity = (nHeaderCount >= 0) ? true : false;

        if (bIsEntity)
        {
            ParseDeltaHeader(info, &entityBitBuffer);
        }

        for (updateType = PreserveEnt; updateType == PreserveEnt;)
        {
            // Figure out what kind of an update this is.
            if (!bIsEntity || info.m_nNewEntity > ENTITY_SENTINEL)
            {
                updateType = Finished;
            }
            else
            {
                m_bUpdatedEntities = true;

                if (info.m_UpdateFlags & FHDR_ENTERPVS)
                {
                    updateType = EnterPVS;
                }
                else if (info.m_UpdateFlags & FHDR_LEAVEPVS)
                {
                    updateType = LeavePVS;
                }
                else
                {
                    updateType = DeltaEnt;
                }
            }

            switch (updateType)
            {
                case EnterPVS:
                {
                    uint32 uClass = entityBitBuffer.ReadUBitLong(
                        s_nServerClassBits); // 9 is gud
                    uint32 uSerialNum = entityBitBuffer.ReadUBitLong(
                        NUM_NETWORKED_EHANDLE_SERIAL_NUMBER_BITS); // 10 is gud.

                    if (g_bDumpPacketEntities)
                    {
                        printf("Entity Enters PVS: id:%d, class:%d, "
                               "serial:%d\n",
                               info.m_nNewEntity,
                               uClass,
                               uSerialNum);
                    }

                    EntityEntry* pEntity = AddEntity(info.m_nNewEntity,
                                                     uClass,
                                                     uSerialNum);

                    if (!ReadNewEntity(entityBitBuffer, pEntity))
                    {
                        printf(
                            "*****Entry: Error reading entity! Bailing on this "
                            "PacketEntities!\n");
                        return;
                    }

                    pEntity->m_bDraw = true;
                    pEntity->m_bUpdated = true;
                }
                break;

                case LeavePVS:
                {
                    if (!bAsDelta) // Should never happen on a full update.
                    {
                        printf("WARNING: LeavePVS on full update");
                        updateType = Failed; // break out
                        assert(0);
                    }
                    else
                    {
                        EntityEntry* pEntity = FindEntity(info.m_nNewEntity);

                        if (pEntity != nullptr)
                        {
                            pEntity->m_bDraw = false;
                            pEntity->m_bUpdated = true;
                        }

                        if (info.m_UpdateFlags & FHDR_DELETE)
                        {
                            if (g_bDumpPacketEntities)
                                printf("Entity leaves PVS and is deleted: "
                                       "id:%d\n",
                                       info.m_nNewEntity);
                            // Remove here entity.
                            RemoveEntity(info.m_nNewEntity);
                        }
                        else
                        {
                            if (g_bDumpPacketEntities)
                                printf("Entity leaves PVS: id:%d\n",
                                       info.m_nNewEntity);
                        }
                    }
                }
                break;

                case DeltaEnt:
                {
                    EntityEntry* pEntity = FindEntity(info.m_nNewEntity);
                    if (pEntity)
                    {
                        if (g_bDumpPacketEntities)
                        {
                            printf("Entity Delta update: id:%d, class:%d, "
                                   "serial:%d\n",
                                   pEntity->m_nEntity,
                                   pEntity->m_uClass,
                                   pEntity->m_uSerialNum);
                        }

                        if (!ReadNewEntity(entityBitBuffer, pEntity))
                        {
                            printf("*****Delta: Error reading entity! Bailing "
                                   "on this "
                                   "PacketEntities!\n");
                            return;
                        }

                        pEntity->m_bUpdated = true;
                    }
                    else
                    {
                        assert(0);
                    }
                }
                break;

                case PreserveEnt:
                {
                    if (!bAsDelta) // Should never happen on a full update.
                    {
                        printf("WARNING: PreserveEnt on full update");
                        updateType = Failed; // break out
                        assert(0);
                    }
                    else
                    {
                        if (info.m_nNewEntity >= MAX_EDICTS)
                        {
                            printf(
                                "PreserveEnt: info.m_nNewEntity == MAX_EDICTS");
                            assert(0);
                        }
                        else
                        {
                            if (g_bDumpPacketEntities)
                            {
                                printf("PreserveEnt: id:%d\n",
                                       info.m_nNewEntity);
                            }

                            EntityEntry* pEntity = FindEntity(
                                info.m_nNewEntity);

                            if (pEntity)
                            {
                                pEntity->m_bUpdated = false;
                            }
                        }
                    }
                }
                break;

                default:
                    break;
            }
        }
    }

    if (g_bDumpPacketEntities)
        printf("Updated entities: %zu\n", m_vecEntities.size());

    // Find highest entity so we only update the one we know..

    // Ignore deletion.
    // if (bAsDelta && updateType == Finished)
    // {
    //     while (entityBitBuffer.ReadOneBit() != 0)
    //     {
    //         int index = entityBitBuffer.ReadUBitLong(MAX_EDICT_BITS);
    //         // delete Entity
    //         RemoveEntity(index);
    //     }
    // }
}

bool netchan_t::ReadSubDataChannel(CBitRead& buf, int stream)
{
    dataFragments_t* data = &m_ReceiveList[stream]; // get
                                                    // list
    int startFragment = 0;
    int numFragments = 0;
    unsigned int offset = 0;
    unsigned int length = 0;

    bool bSingleBlock = buf.ReadOneBit() == 0; // is single block ?

    if (!bSingleBlock)
    {
        startFragment = buf.ReadUBitLong(MAX_FILE_SIZE_BITS -
                                         FRAGMENT_BITS); // 16 MB max
        numFragments = buf.ReadUBitLong(3); // 8 fragments per packet max
        offset = startFragment * FRAGMENT_SIZE;
        length = numFragments * FRAGMENT_SIZE;
    }

    if (offset == 0) // first fragment, read header info
    {
        data->filename[0] = 0;
        data->isCompressed = false;
        data->transferID = 0;

        if (bSingleBlock)
        {
            // data compressed ?
            if (buf.ReadOneBit())
            {
                data->isCompressed = true;
                data->nUncompressedSize = buf.ReadUBitLong(MAX_FILE_SIZE_BITS);
            }
            else
            {
                data->isCompressed = false;
            }

            data->bytes = buf.ReadUBitLong(NET_MAX_PALYLOAD_BITS);
        }
        else
        {
            if (buf.ReadOneBit()) // is it a file ?
            {
                data->transferID = buf.ReadUBitLong(32);
                buf.ReadString(data->filename, MAX_OSPATH);
            }

            // data compressed ?
            if (buf.ReadOneBit())
            {
                data->isCompressed = true;
                data->nUncompressedSize = buf.ReadUBitLong(MAX_FILE_SIZE_BITS);
            }
            else
            {
                data->isCompressed = false;
            }

            data->bytes = buf.ReadUBitLong(MAX_FILE_SIZE_BITS);
        }

        if (data->buffer)
        {
            // last transmission was aborted, free data
            free(data->buffer);
        }

        data->bits = data->bytes * 8;
        data->buffer = (char*)malloc(PAD_NUMBER(data->bytes, 4));
        data->asTCP = false;
        data->numFragments = BYTES2FRAGMENTS(data->bytes);
        data->ackedFragments = 0;
        data->file = FILESYSTEM_INVALID_HANDLE;

        if (bSingleBlock)
        {
            numFragments = data->numFragments;
            length = numFragments * FRAGMENT_SIZE;
        }
    }
    else
    {
        if (data->buffer == NULL)
        {
            // This can occur if the packet containing the "header" (offset ==
            // 0) is dropped.  Since we need the header to arrive we'll just
            // wait
            //  for a retry
            // ConDMsg("Received fragment out of order: %i/%i\n", startFragment,
            // numFragments );
            return false;
        }
    }

    if ((startFragment + numFragments) == data->numFragments)
    {
        //   if ( startFragment + numFragments == *(_DWORD *)(v7 + 304) )
        //   {
        //     v50 = 256 - *(unsigned __int8 *)(v7 + 280);
        //     if ( v50 != 256 )
        //       numFragments_1 -= v50;
        //   }

        // we are receiving the last fragment, adjust length
        int rest = FRAGMENT_SIZE - (unsigned int)(*(uint8_t*)&data->bytes);
        if ((unsigned int)rest != FRAGMENT_SIZE)
            length -= rest;
    }
    else if ((startFragment + numFragments) > data->numFragments)
    {
        printf("Received fragment chunk out of bounds\n");
        return false;
    }

    assert((offset + length) <= data->bytes);

    buf.ReadBytes(data->buffer + offset, length); // read data

    data->ackedFragments += numFragments;

    return true;
}

bool netchan_t::NET_BufferToBufferDecompress(char* dest,
                                             unsigned int* destLen,
                                             char* source,
                                             unsigned int sourceLen)
{
    CLZSS s;
    if (s.IsCompressed((byte*)source))
    {
        unsigned int uDecompressedLen = s.GetActualSize((byte*)source);
        if (uDecompressedLen > *destLen)
        {
            printf("NET_BufferToBufferDecompress with improperly sized dest "
                   "buffer (%u in, %u needed)\n",
                   *destLen,
                   uDecompressedLen);
            return false;
        }
        else
        {
            *destLen = s.Uncompress((byte*)source, (byte*)dest);
        }
    }
    else
    {
        memcpy(dest, source, sourceLen);
        *destLen = sourceLen;
    }

    return true;
}

void netchan_t::UncompressFragments(dataFragments_t* data)
{
    if (!data->isCompressed)
        return;

    // allocate buffer for uncompressed data, align to 4 bytes boundary
    char* newbuffer = (char*)malloc(PAD_NUMBER(data->nUncompressedSize, 4));
    unsigned int uncompressedSize = data->nUncompressedSize;

    // uncompress data
    NET_BufferToBufferDecompress(
        newbuffer, &uncompressedSize, data->buffer, data->bytes);

    assert(uncompressedSize == data->nUncompressedSize);

    // free old buffer and set new buffer
    free(data->buffer);
    data->buffer = newbuffer;
    data->bytes = uncompressedSize;
    data->isCompressed = false;
}

bool netchan_t::CheckReceivingList(int nList)
{
    dataFragments_t* data = &m_ReceiveList[nList]; // get
                                                   // list

    if (data->buffer == NULL)
        return true;

    if (data->ackedFragments < data->numFragments)
        return true;

    if (data->ackedFragments > data->numFragments)
    {
        return false;
    }

    UncompressFragments(data);

    if (!data->filename[0])
    {
        CBitRead buf(data->buffer, data->bytes);

        if (!processCmd(buf)) // parse net message
        {
            return false; // stop reading any further
        }
    }
    else
    {
        printf("Got file %s\n", data->filename);
    }

    // clear receiveList
    if (data->buffer)
    {
        free(data->buffer);
        data->buffer = NULL;
    }

    return true;
}

bool netchan_t::processCmd(CBitRead& buf)
{
    //  https://github.com/VSES/SourceEngine2007/blob/43a5c90a5ada1e69ca044595383be67f40b33c61/se2007/engine/net_chan.cpp#L1845
    while (true)
    {
        if (buf.IsOverflowed())
        {
            return false;
        }

        //
        // https://github.com/VSES/SourceEngine2007/blob/43a5c90a5ada1e69ca044595383be67f40b33c61/src_main/engine/net_chan.cpp#L1856
        if (buf.GetNumBitsLeft() < NETMSG_TYPE_BITS)
        {
            break;
        }

        int cmd = buf.ReadUBitLong(NETMSG_TYPE_BITS);

        // https://github.com/VSES/SourceEngine2007/blob/43a5c90a5ada1e69ca044595383be67f40b33c61/src_main/engine/net_chan.cpp#L1874
        // https://github.com/VSES/SourceEngine2007/blob/43a5c90a5ada1e69ca044595383be67f40b33c61/src_main/engine/net_chan.cpp#L1876
        int bufSize = buf.ReadVarInt32();
        auto parseBuffer = [&bufSize, &buf](void** pBuffer) {
            if (bufSize < 0 || bufSize > NET_MAX_PAYLOAD)
            {
                return false;
            }

            // Check its valid
            if (bufSize > buf.GetNumBytesLeft())
            {
                return false;
            }

            *pBuffer = malloc(bufSize);

            // If the read buffer is byte aligned, we can parse right out of it
            if ((buf.GetNumBitsRead() % 8) == 0)
            {
                memcpy(*pBuffer,
                       buf.GetBasePointer() + buf.GetNumBytesRead(),
                       bufSize);
                buf.SeekRelative(bufSize * 8);
                return true;
            }

            // otherwise we have to ReadBytes() it out
            if (!buf.ReadBytes(*pBuffer, bufSize))
            {
                return false;
            }

            return true;
        };

        void* pBuf = nullptr;
        if (!parseBuffer(&pBuf))
        {
            if (pBuf != nullptr)
            {
                free(pBuf);
            }

            break;
        }

        switch (cmd)
        {
            case svc_PacketEntities:
            {
                CSVCMsg_PacketEntities msg;
                if (!msg.ParseFromArray(pBuf, bufSize))
                {
                    break;
                }

                parseEnts(msg);
                break;
            }
            case svc_ClassInfo:
            {
                CSVCMsg_ClassInfo msg;
                if (!msg.ParseFromArray(pBuf, bufSize))
                {
                    break;
                }

                assert("Can't do this yet");

                break;
            }
            case net_Disconnect:
            {
                auto netadr = &m_netAdr;
                printf("Disconnecting %i.%i.%i.%i:%i\n",
                       netadr->ip[0],
                       netadr->ip[1],
                       netadr->ip[2],
                       netadr->ip[3],
                       netadr->port);
                clear();
                break;
            }
            case net_SignonState:
            {
                CNETMsg_SignonState msg;
                if (msg.ParseFromArray(pBuf, bufSize))
                    printf("%s\n", msg.DebugString().c_str());
                break;
            }
            case svc_ServerInfo:
            {
                CSVCMsg_ServerInfo msg;
                if (msg.ParseFromArray(pBuf, bufSize))
                {
                    printf("%s\n", msg.DebugString().c_str());
                    m_strMapName = msg.map_name();
                }
            }
            case net_Tick:
            {
                // CNETMsg_Tick msg;
                // if (msg.ParseFromArray(pBuf, bufSize))
                //     printf("%s\n", msg.DebugString().c_str());
                break;
            }
            case svc_SendTable:
            {
                CSVCMsg_SendTable msg;
                if (!msg.ParseFromArray(pBuf, bufSize))
                {
                    break;
                }

                assert("Can't do this yet");

                for (int iProp = 0; iProp < msg.props_size(); iProp++)
                {
                    const CSVCMsg_SendTable::sendprop_t& sendProp = msg.props(
                        iProp);

                    if ((sendProp.type() == DPT_DataTable) ||
                        (sendProp.flags() & SPROP_EXCLUDE))
                    {
                        printf("%d:%06X:%s:%s%s\n",
                               sendProp.type(),
                               sendProp.flags(),
                               sendProp.var_name().c_str(),
                               sendProp.dt_name().c_str(),
                               (sendProp.flags() & SPROP_EXCLUDE) ? "exclude" :
                                                                    "");
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
                               (sendProp.flags() & SPROP_INSIDEARRAY) ?
                                   " inside array" :
                                   "");
                    }
                }
            }

            default:
                break;
        }

        free(pBuf);
    }

    return true;
}

int netchan_t::ReadHeader(CBitRead& buf)
{
    // Process header.
    int sequence = buf.ReadUBitLong(32);          // seq
    /*int sequence_ack = */ buf.ReadUBitLong(32); // seqack
    int nFlags = buf.ReadByte();                  // nFlags

    unsigned short checkSum = (unsigned short)buf.ReadUBitLong(16); // checksum

    int nOffset = buf.GetNumBitsRead() >> 3;
    int nCheckSumBytes = buf.TotalBytesAvailable() - nOffset;

    const void* pvData = buf.GetBasePointer() + nOffset;
    unsigned short usDataCheckSum = BufferToShortChecksum(pvData,
                                                          nCheckSumBytes);

    // Incorrect checksum
    if (usDataCheckSum != checkSum)
    {
        printf("Invalid packet checksum\n");
        return -1;
    }

    /*int realState = */ buf.ReadByte(); // realState
    /*int nChoked = 0;*/

    if (nFlags & PACKET_FLAG_CHOKED)
    {
        /*nChoked = */ buf.ReadByte();
    }

    // printf("seq=%i ack=%i rel=%1i choke=%i flags=%i checksum=%i "
    //        "realstate=%i "
    //        "oldseq=%i\n",
    //        sequence,
    //        sequence_ack,
    //        nFlags & PACKET_FLAG_RELIABLE,
    //        nChoked,
    //        nFlags,
    //        checkSum,
    //        realState,
    //        m_nInSequenceNr);

    // discard stale or duplicated packets
    if (sequence <= m_nInSequenceNr)
    {
        printf("Discarding packet because sequence is not newer\n");
        return -1;
    }

    m_nInSequenceNr = sequence;

    if (sequence == 0x36)
        nFlags |= PACKET_FLAG_TABLES;

    return nFlags;
}

// https://github.com/VSES/SourceEngine2007/blob/43a5c90a5ada1e69ca044595383be67f40b33c61/se2007/engine/net_chan.cpp#L2095new
// char
// https://github.com/VSES/SourceEngine2007/blob/43a5c90a5ada1e69ca044595383be67f40b33c61/se2007/engine/net_ws.cpp#L1702
// https://github.com/VSES/SourceEngine2007/blob/43a5c90a5ada1e69ca044595383be67f40b33c61/se2007/engine/net_chan.cpp#L1587
bool netchan_t::ReadPacketServer(unsigned char* packetData, int size)
{
    CBitRead buf(packetData, size);

    auto nFlags = ReadHeader(buf);

    // Invalid packet.
    if (nFlags == -1)
    {
        printf("Malformed packet\n");
        return false;
    }

    // From there it seems to work good.
    // printf("Chocked: %i In: %i Out: %i\n", nChocked, nSeqNrIn & 63,
    // nSeqNrOut & 63);
    // https://github.com/VSES/SourceEngine2007/blob/43a5c90a5ada1e69ca044595383be67f40b33c61/se2007/engine/net_chan.cpp#L2304

    if (nFlags & PACKET_FLAG_RELIABLE)
    {
        // bit
        buf.ReadUBitLong(3);

        for (int i = 0; i < MAX_STREAMS; i++)
        {
            if (buf.ReadOneBit() != 0)
            {
                if (!ReadSubDataChannel(buf, i))
                    return false; // error while
                                  // reading fragments,
                                  // drop whole packet
            }
        }

        for (int i = 0; i < MAX_STREAMS; i++)
        {
            if (!CheckReceivingList(i))
                return false; // error while processing
        }
    }

    if (buf.GetNumBitsLeft() > 0)
    {
        if (!processCmd(buf))
        {
            return false;
        }
    }

    return true;
    // printf("Getting out\n")
}

void checkCompression(unsigned char*& pDataOut, uint32_t& finalSize)
{
    // Next check for compressed message
    if (LittleLong(*(int*)pDataOut) == NET_HEADER_FLAG_COMPRESSEDPACKET)
    {
        byte* pCompressedData = pDataOut + sizeof(int);

        CLZSS lzss;
        // Decompress
        int actualSize = lzss.GetActualSize(pCompressedData);
        if (actualSize <= 0)
            return;

        auto memDecompressed = reinterpret_cast<uint8_t*>(alloca(actualSize));

        unsigned int uDecompressedSize = lzss.Uncompress(pCompressedData,
                                                         memDecompressed);

        // packet->wiresize is already set
        auto buffer = reinterpret_cast<unsigned char*>(
            malloc(uDecompressedSize));

        memcpy(buffer, memDecompressed, uDecompressedSize);
        finalSize = uDecompressedSize;

        free(pDataOut);
        pDataOut = buffer;
        printf("Decompressed packet\n");
    }
}

bool netchan_t::parsePayload(const unsigned char* payload, const int size)
{
    NET_UpdateTime();

    // We can't do this yet, we need proper update time.
    // NET_DiscardStaleSplitpackets();
    // If we weren't connected, clear everything.

    unsigned char* copied_payload = reinterpret_cast<unsigned char*>(
        malloc(size));

    memcpy(copied_payload, payload, size);

    uint32_t payload_size = (uint32_t)size;

    if (payload_size >= NET_MAX_MESSAGE)
    {
        free(copied_payload);
        printf("Ignoring packet too large\n");
        return false;
    }

    if (LittleLong(*(int*)copied_payload) == NET_HEADER_FLAG_SPLITPACKET)
    {
        if (!NET_GetLong(copied_payload, payload_size, &m_netAdr))
        {
            free(copied_payload);
            return false;
        }
    }

    // Shouldn't be needed but in case.
    // checkCompression(copied_payload, payload_size);

    // Init server informations, steam stuff.. Done at creating channels.
    if (LittleLong(*(int*)copied_payload) == NET_HEADER_FLAG_QUERY)
    {
        CBitRead msg((unsigned char*)((uintptr_t)payload + sizeof(int)), size);
        auto connectionType = msg.ReadChar();

        printf("connectionType: %c\n", connectionType);

        free(copied_payload);

        if (!m_connected)
        {
            m_connected = true;
        }

        return false;
    }

    if (!m_connected)
    {
        return false;
    }

    unsigned char* pData = copied_payload;
    IceKey ice(2);
    ice.set(g_iceKey);

    auto pDataOut = (uint8_t*)malloc(payload_size);

    int32 blockSize = ice.blockSize();

    uint8* p1 = pData;
    uint8* p2 = pDataOut;

    // decrypt data in 8 byte blocks
    int32 bytesLeft = payload_size;

    while (bytesLeft >= blockSize)
    {
        ice.decrypt(p1, p2);

        bytesLeft -= blockSize;
        p1 += blockSize;
        p2 += blockSize;
    }

    // The end chunk doesn't get an encryption. it sux.
    memcpy(p2, p1, bytesLeft);

    unsigned char deltaOffset = *(unsigned char*)pDataOut;
    uint32_t realSize = 0;

    if (deltaOffset)
    {
        LODWORD(realSize) = (unsigned char)deltaOffset + 5;

        if (realSize < (uint32_t)payload_size)
        {
            auto finalSize = bswap_32(
                *reinterpret_cast<uint32_t*>(&pDataOut[deltaOffset + 1]));

            LODWORD(realSize) = realSize + finalSize;

            // printf("%i = %i\n", realSize, size);

            if (realSize == (uint32_t)payload_size)
            {
                auto tempBuffer = (unsigned char*)malloc(finalSize);
                memcpy(tempBuffer, &pDataOut[deltaOffset + 5], finalSize);

                free(pDataOut);
                pDataOut = tempBuffer;

                checkCompression(pDataOut, finalSize);

                ReadPacketServer(pDataOut, finalSize);
            }
        }
    }

    free(pDataOut);
    free(copied_payload);

    return true;
}

netchan_t* netchan_t::findOrCreateChannel(uint32_t ip,
                                          unsigned short port,
                                          std::vector<byte> iceKey)
{
    netchan_t* netChannel = nullptr;

    for (auto&& channel : g_vecNetChannels)
    {
        uint32_t copiedIp;
        memcpy(&copiedIp, channel->m_netAdr.ip, sizeof(copiedIp));

        if (copiedIp == ip && channel->m_netAdr.port == port)
        {
            netChannel = channel;
            break;
        }
    }

    if (netChannel == nullptr)
    {
        netChannel = new netchan_t(iceKey);

        netadr_t* netadr = &netChannel->m_netAdr;
        netadr->port = port;
        netadr->type = netadrtype_t::NA_IP;
        memcpy(netadr->ip, &ip, sizeof(netadr_t::ip));

        g_vecNetChannels.push_back(netChannel);
    }

    return netChannel;
}

auto processPayload(uint32_t ip,
                    unsigned short port,
                    const unsigned char* payload,
                    const int size,
                    std::vector<byte> iceKey) -> netchan_t*
{
    auto netChannel = netchan_t::findOrCreateChannel(ip, port, iceKey);
    netChannel->parsePayload(payload, size);
    return netChannel;
}
