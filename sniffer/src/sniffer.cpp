#include <utility>
#include <proxycheat.h>
#include <sniffer.h>

struct payload_t
{
    payload_t()
    {}

    payload_t(uint32_t ip, unsigned short port, Tins::byte_array packet) :
        ip(ip), port(port), packet(std::move(packet))
    {}

    ~payload_t()
    {}

    uint32_t ip;
    unsigned short port;
    Tins::byte_array packet;
};

// int g_captureCountMax = 100000;
// int g_countPackets = 0;
// if (g_countPackets < g_captureCountMax)
// {
//     std::ofstream file;

//     if (g_countPackets == 0)
//     {
//         file.open("/tmp/savedpackets.txt",
//                   std::ios::trunc | std::ios::out);
//         file.close();
//     }

//     file.open("/tmp/savedpackets.txt",
//               std::ios::app | std::ios::binary);
//     auto size = (int)payload_size;

//     netadr_t netadr;

//     *(uint32_t*)netadr.ip =
//         pdu.find_pdu<IP>()->src_addr().operator uint32_t();
//     netadr.port = g_usPort;

//     file.write(reinterpret_cast<const char*>(&netadr),
//                sizeof(netadr_t));
//     file.write(reinterpret_cast<const char*>(&size), sizeof(size));
//     file.write(reinterpret_cast<const char*>(payload.data()),
//                payload_size);
//     file.close();

//     g_countPackets++;
// }

using namespace Tins;
static unsigned short g_usPort;
using boost::asio::ip::udp;
static boost::asio::io_service io_service;
static udp::socket socket_udp(io_service, udp::endpoint(udp::v4(), 0));
// std::condition_variable g_notifyUpdatePayLoad;
// std::vector<payload_t> g_vecPayloads;

auto radarThreadSend(netchan_t* channel) -> void
{
    if (channel->m_vecEntities.size() == 0 || channel->m_strMapName.size() == 0)
        return;

    // The worst case senario. MAXIMUM EDICTS * size of an entity + server
    // infos.
    constexpr auto worstSenarioSize = LZ4_COMPRESSBOUND(
        MAX_EDICTS * sizeof(EntityNetwork) + MAX_OSPATH);

    static char compressed[worstSenarioSize];

    static socket_buffer::WriteBuffer<worstSenarioSize> buffer;
    buffer.reset();

    // Server infos.
    {
        std::vector<socket_buffer::byte_t> mapName;
        mapName.resize(channel->m_strMapName.size());
        memcpy(mapName.data(), channel->m_strMapName.data(), mapName.size());

        buffer.addVar<socket_buffer::type_array>(
            mapName.data(),
            static_cast<socket_buffer::safesize_t>(mapName.size()));
    }

    // Entities
    {
        std::vector<EntityNetwork> vecNetEntities;

        for (auto&& entityEntry : channel->m_vecEntities)
        {
            if (!entityEntry->m_bUpdated)
            {
                continue;
            }

            // Say we updated the entity.
            entityEntry->m_bUpdated = false;

            auto table = GetTableByClassID(entityEntry->m_uClass);

            EntityNetwork entityNetwork;
            entityNetwork.m_bDraw = entityEntry->m_bDraw;

            auto prop = entityEntry->FindProp("m_vecOrigin");

            if (prop)
            {
                entityNetwork.m_vecPosition[coordinates::X] =
                    prop->m_pPropValue->m_value.m_vector.x;
                entityNetwork.m_vecPosition[coordinates::Y] =
                    prop->m_pPropValue->m_value.m_vector.y;
            }
            else
            {
                prop = entityEntry->FindProp("m_vecOrigin[0]");

                if (prop)
                {
                    entityNetwork.m_vecPosition[coordinates::X] =
                        prop->m_pPropValue->m_value.m_float;
                }

                prop = entityEntry->FindProp("m_vecOrigin[1]");

                if (prop)
                {
                    entityNetwork.m_vecPosition[coordinates::Y] =
                        prop->m_pPropValue->m_value.m_float;
                }
            }

            // m_angEyeAngles or m_angRotation.

            prop = entityEntry->FindProp("m_angEyeAngles");

            if (prop)
            {
                entityNetwork.m_angleOrientation =
                    prop->m_pPropValue->m_value.m_vector.y;
            }
            else
            {
                prop = entityEntry->FindProp("m_angEyeAngles[1]");

                if (prop)
                {
                    entityNetwork.m_angleOrientation =
                        prop->m_pPropValue->m_value.m_float;
                }
                else
                {
                    prop = entityEntry->FindProp("m_angRotation");

                    if (prop)
                    {
                        entityNetwork.m_angleOrientation =
                            prop->m_pPropValue->m_value.m_vector.y;
                    }
                    else
                    {
                        prop = entityEntry->FindProp("m_angRotation[1]");

                        if (prop)
                        {
                            entityNetwork.m_angleOrientation =
                                prop->m_pPropValue->m_value.m_float;
                        }
                    }
                }
            }

            prop = entityEntry->FindProp("m_iHealth");

            if (prop)
            {
                entityNetwork.m_iHealth = static_cast<char>(
                    prop->m_pPropValue->m_value.m_int);
            }

            prop = entityEntry->FindProp("m_iTeamNum");

            if (prop)
            {
                entityNetwork.m_iTeamNum = static_cast<char>(
                    prop->m_pPropValue->m_value.m_int);
            }

            entityNetwork.m_iIndex = static_cast<short>(entityEntry->m_nEntity);
            strcpy(entityNetwork.m_tableName, table->net_table_name().c_str());
            vecNetEntities.push_back(entityNetwork);
        }

        buffer.addVar<socket_buffer::type_array>(
            reinterpret_cast<socket_buffer::gvt<socket_buffer::type_array>>(
                vecNetEntities.data()),
            static_cast<socket_buffer::safesize_t>(vecNetEntities.size() *
                                                   sizeof(EntityNetwork)));
    }

    if (buffer.m_writeSize == 0)
        return;

    auto dstCompressedSize = LZ4_compressBound(buffer.m_writeSize);
    dstCompressedSize = LZ4_compress_fast(buffer.shift<char*>(),
                                          compressed,
                                          buffer.m_writeSize,
                                          dstCompressedSize,
                                          1);

    socket_udp.send_to(
        boost::asio::buffer(compressed, static_cast<size_t>(dstCompressedSize)),
        udp::endpoint(boost::asio::ip::address_v4::broadcast(), 2407));
}

// auto packetsThread() -> void
// {
//     while (true)
//     {
//         // // This is way too low.
//         // std::mutex m;
//         // std::unique_lock<std::mutex> lm(m);
//         // g_notifyUpdatePayLoad.wait(lm,
//         //                            [] { return g_vecPayloads.size() != 0;
//         });

//         if (g_vecPayloads.size() > 0)
//         {
//             auto payload = g_vecPayloads.begin();

//             auto channel = processPayload(payload->ip,
//                                           payload->port,
//                                           payload->packet.data(),
//                                           payload->packet.size());

//             transformToRadar(channel);

//             g_vecPayloads.erase(payload);
//         }
//     }
// }

bool sniff_loop(const PDU& pdu)
{
    auto udp = pdu.find_pdu<UDP>();
    auto packet = pdu.find_pdu<RawPDU>();
    auto payload = packet->payload();
    auto ip = pdu.find_pdu<IP>()->src_addr().operator uint32_t();
    auto port = udp->sport();

    // Only received packets
    if (udp->sport() == g_usPort)
    {
        auto channel = processPayload(
            ip, port, payload.data(), static_cast<int>(payload.size()));

        // This should be thread safe.
        if (channel->m_bUpdatedEntities)
        {
            radarThreadSend(channel);
            channel->m_bUpdatedEntities = false;
        }

        // g_notifyUpdatePayLoad.notify_one();
        // g_vecPayloads.push_back(payload_t(ip, port, payload));
    }

    return true;
}

int main()
{
    socket_udp.set_option(boost::asio::ip::udp::socket::reuse_address(true));
    socket_udp.set_option(boost::asio::socket_base::broadcast(true));

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

    auto nis = NetworkInterface::all();
    std::cout << "Please enter interface number" << std::endl;

    int number = 0;
    for (auto&& ni : nis)
    {
        std::cout << "[" << number << "] " << ni.name() << std::endl;
        number++;
    }

    size_t interchosen;
    std::cin >> interchosen;

    std::cout << "Port?" << std::endl;
    std::cin >> g_usPort;

    SnifferConfiguration config;
    config.set_immediate_mode(true);
    config.set_filter("port " + std::to_string(g_usPort));
    // config.set_buffer_size(NET_MAX_MESSAGE);
    config.set_promisc_mode(true);
    Sniffer sniffer(nis[interchosen].name(), config);

    // std::thread parsePacket(packetsThread);
    // parsePacket.detach();

    sniffer.sniff_loop(sniff_loop);

    // parsePacket.join();
    socket_udp.close();

    return 0;
}
