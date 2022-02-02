#include <sniffer.h>
#include <proxycheat.h>
#include <fstream>

static std::vector<uint8_t> readFile(const char* filename)
{
    std::ifstream ifs(filename, std::ios::binary | std::ios::ate);
    std::ifstream::pos_type pos = ifs.tellg();

    std::vector<uint8_t> result(pos);

    ifs.seekg(0, std::ios::beg);
    ifs.read(reinterpret_cast<char*>(result.data()), pos);
    ifs.close();

    return result;
}

int main()
{
    static bool bDoOnce = false;

    if (!bDoOnce)
    {
        // Create ServerClasses and DataTables.
        if (!parseDumpDataFile("dump.txt"))
        {
            assert("Couldn't parse dump file to have "
                   "serverclasses/datatables\n");
        }

        bDoOnce = true;
    }

    auto savedpackets = readFile("savedpackets.txt");
    struct packet_s
    {
        std::vector<uint8_t> packet_data;
        netadr_t netadr;
    };
    std::vector<packet_s> packets;

    uint8_t* ptrReadPackets = savedpackets.data();
    size_t read = 0;

    while (read < savedpackets.size())
    {
        packet_s packet;

        auto netadr = reinterpret_cast<netadr_t*>(ptrReadPackets);
        memcpy(&packet.netadr, netadr, sizeof(netadr_t));

        auto offsetPtr = [&ptrReadPackets, &read](uintptr_t size) {
            read += size;
            ptrReadPackets += size;
        };

        offsetPtr(sizeof(netadr_t));

        auto packetSize = *reinterpret_cast<int*>(ptrReadPackets);
        offsetPtr(sizeof(int));

        std::vector<uint8_t> vecPacket(packetSize);
        memcpy(vecPacket.data(), ptrReadPackets, packetSize);

        packet.packet_data = vecPacket;
        packets.push_back(packet);

        offsetPtr(packetSize);
    }

    int i = 0;
    read = 0;
    for (auto&& packet : packets)
    {
        i++;
        printf("packet %i -- %02i.%02i.%02i.%02i:%i\n",
               i,
               packet.netadr.ip[0],
               packet.netadr.ip[1],
               packet.netadr.ip[2],
               packet.netadr.ip[3],
               packet.netadr.port);
        auto netchan = netchan_t::findOrCreateChannel(
            *(uint32_t*)packet.netadr.ip, packet.netadr.port);

        netchan->parsePayload(packet.packet_data.data(),
                              packet.packet_data.size());
        read += packet.packet_data.size() + sizeof(netadr_t) + sizeof(int);
    }

    /*auto savedpackets = readFile("savedpackets_ents.txt");
    struct packet_s
    {
        std::vector<uint8_t> packet_data;
        bool m_bAsDelta;
        int m_nHeaderCount;
    };
    std::vector<packet_s> packets;

    uint8_t* ptrReadPackets = savedpackets.data();
    size_t read = 0;

    while (read < savedpackets.size())
    {
        packet_s packet;

        packet.m_bAsDelta = reinterpret_cast<bool*>(ptrReadPackets);

        auto offsetPtr = [&ptrReadPackets, &read](uintptr_t size) {
            read += size;
            ptrReadPackets += size;
        };

        offsetPtr(sizeof(bool));

        packet.m_nHeaderCount = *reinterpret_cast<int*>(ptrReadPackets);
        offsetPtr(sizeof(int));

        auto packetSize = *reinterpret_cast<int*>(ptrReadPackets);
        offsetPtr(sizeof(int));

        std::vector<uint8_t> vecPacket(packetSize);
        memcpy(vecPacket.data(), ptrReadPackets, packetSize);

        packet.packet_data = vecPacket;
        packets.push_back(packet);

        offsetPtr(packetSize);
    }

    int i = 0;
    read = 0;
    for (auto&& packet : packets)
    {
        i++;
        printf("packet: %i\n", i);
        auto netchan = netchan_t::findOrCreateChannel(0, 0);
        CSVCMsg_PacketEntities packetEntities;
        packetEntities.set_is_delta(packet.m_bAsDelta);
        packetEntities.set_updated_entries(packet.m_nHeaderCount);
        packetEntities.set_entity_data(packet.packet_data.data(),
    packet.packet_data.size());

        netchan->parseEnts(packetEntities);
        read += packet.packet_data.size() + sizeof(netadr_t) + sizeof(int);
    }*/

    return 0;
}