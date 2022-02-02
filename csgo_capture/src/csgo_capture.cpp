#include <reversed_structs.h>

static bool g_bLoadedCSGO_CAPTURE = false;

using useDetourProcessPacket = CDetourHandler<void, ptr_t, netpacket_t*, bool>;
useDetourProcessPacket* detourProcessPacket = nullptr;

int g_captureCountMax = 100000;

void new_ProcessPacket(ptr_t thisptr, netpacket_t* netpacket, bool bHasHeader)
{
    static int g_countPackets = 0;

    detourProcessPacket->setSafeDelete(false);

    CBitRead_reversed* bitreadmsg = &netpacket->message;

    std::ofstream file;

    if (g_countPackets == 0)
    {
        file.open("/tmp/savedpackets_hook.txt",
                  std::ios::trunc | std::ios::out);
        file.close();
    }

    file.open("/tmp/savedpackets_hook.txt", std::ios::app | std::ios::binary);
    auto size = (int)bitreadmsg->m_nDataBytes;

    netadr_t netadr;
    memcpy(&netadr, &netpacket->from, sizeof(netadr_t));
    netadr.port = ntohs(netadr.port);

    file.write(reinterpret_cast<const char*>(&netadr), sizeof(netadr_t));
    file.write(reinterpret_cast<const char*>(&size), sizeof(size));
    file.write(reinterpret_cast<const char*>(bitreadmsg->m_pData), size);
    file.close();

    g_countPackets++;

    detourProcessPacket->callOriginal(thisptr, netpacket, bHasHeader);

    if (g_countPackets == g_captureCountMax)
    {
        std::cout << "Finished capture" << std::endl;
        detourProcessPacket->restoreOpCodes();
    }

    detourProcessPacket->setSafeDelete();
}

using useDetourReadPacketEntities = CDetourHandler<void, void*, entityread_t*>;
useDetourReadPacketEntities* detourReadPacketEntities = nullptr;

// Hook only once.
void new_ReadPacketEntities(void* thisptr, entityread_t* u)
{
    auto bitreadmsg = u->msg;
    static int g_countPackets = 0;
    // std::cout << bitreadmsg->m_pDebugName << ":" << std::endl;
    // std::cout << "  "
    //           << "m_bOverflow = " << std::hex << bitreadmsg->m_bOverflow
    //           << std::endl;
    // std::cout << "  "
    //           << "m_nBitsAvail = " << std::hex << bitreadmsg->m_nBitsAvail
    //           << std::endl;
    // std::cout << "  "
    //           << "m_nDataBits = " << std::hex << bitreadmsg->m_nDataBits
    //           << std::endl;
    // std::cout << "  "
    //           << "m_nDataBytes = " << std::hex << bitreadmsg->m_nDataBytes
    //           << std::endl;
    // std::cout << "  "
    //           << "m_nInBufWord = " << std::hex << bitreadmsg->m_nInBufWord
    //           << std::endl;
    // std::cout << "  "
    //           << "m_pData = " << std::hex << bitreadmsg->m_pData <<
    //           std::endl;
    // std::cout << "  "
    //           << "m_pDataIn = " << std::hex << bitreadmsg->m_pDataIn
    //           << std::endl;
    // std::cout << "  "
    //           << "m_pBufferEnd = " << std::hex << bitreadmsg->m_pBufferEnd
    //           << std::endl;

    {
        std::ofstream file;

        if (g_countPackets == 0)
        {
            file.open("/tmp/savedpackets_ents.txt",
                      std::ios::trunc | std::ios::out);
            file.close();
        }

        file.open("/tmp/savedpackets_ents.txt",
                  std::ios::app | std::ios::binary);

        auto size = (int)bitreadmsg->m_nDataBytes;

        file.write(reinterpret_cast<const char*>(&u->m_bAsDelta), sizeof(bool));
        file.write(reinterpret_cast<const char*>(&u->m_nHeaderCount),
                   sizeof(int));
        file.write(reinterpret_cast<const char*>(&size), sizeof(size));
        file.write(reinterpret_cast<const char*>(bitreadmsg->m_pData), size);
        file.close();
        g_countPackets++;
    }

    detourReadPacketEntities->setSafeDelete(false);
    detourReadPacketEntities->callOriginal(thisptr, u);
    if (g_captureCountMax == g_countPackets)
    {
        detourReadPacketEntities->restoreOpCodes();
    }
    detourReadPacketEntities->setSafeDelete();
}

void __attribute__((constructor)) init_module(void)
{
    g_bLoadedCSGO_CAPTURE = true;

    std::cout << "Injected" << std::endl;

    auto lm = reinterpret_cast<link_map_t*>(
        dlopen("bin/linux64/engine_client.so", RTLD_LAZY));

    auto ptrProcessPacket = reinterpret_cast<ptr_t>(lm->l_addr + 0x4C11F0);

    if (!isValidPtr(ptrProcessPacket))
        assert("Not valid ProcessPacket address");

    detourProcessPacket = new useDetourProcessPacket(ptrProcessPacket);
    detourProcessPacket->setFunction(new_ProcessPacket);

    if (!detourProcessPacket->placeStubJmp())
    {
        assert("Couldn't place stubjmp for ProcessPacket");
    }

    auto ptrReadPacketEntities = reinterpret_cast<ptr_t>(lm->l_addr + 0x2FEE50);

    if (!isValidPtr(ptrReadPacketEntities))
        assert("Not valid ReadPacketEntities address");

    detourReadPacketEntities = new useDetourReadPacketEntities(
        ptrReadPacketEntities);
    detourReadPacketEntities->setFunction(new_ReadPacketEntities);

    if (!detourReadPacketEntities->placeStubJmp())
    {
        assert("Couldn't place the jmp on original ReadPacketEntities "
               "function.");
    }
}

void __attribute__((destructor)) cleanup_module(void)
{
    delete detourProcessPacket;
    delete detourReadPacketEntities;
}
