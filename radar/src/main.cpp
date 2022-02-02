#include <radar_pch.h>
#include <radar.h>
#include <socket_buffer.h>
#include <decoder.h>

auto recvPacket(boost::asio::ip::udp::socket& socket,
                std::vector<EntityNetwork>& vecEntities,
                std::string& strMapName) -> void
{
    // Static buffer for faster updates
    constexpr auto worstSenarioSize = MAX_EDICTS * sizeof(EntityNetwork) +
                                      MAX_OSPATH;

    static char buffer[worstSenarioSize];
    static socket_buffer::ReadBuffer<worstSenarioSize> decompressedBuffer;

    decompressedBuffer.reset();

    boost::asio::ip::udp::endpoint endpoint;
    auto recvSize = socket.receive_from(boost::asio::buffer(buffer,
                                                            sizeof(buffer)),
                                        endpoint);

    auto newSize = LZ4_decompress_safe(buffer,
                                       decompressedBuffer.shift<char*>(),
                                       static_cast<int>(recvSize),
                                       static_cast<int>(
                                           decompressedBuffer.m_maxSize));

    socket_buffer::safesize_t lenMapName = 0;
    auto ptrStrMapName = decompressedBuffer.readVar<socket_buffer::type_array>(
        &lenMapName);

    strMapName = std::string(reinterpret_cast<char*>(ptrStrMapName),
                             static_cast<size_t>(lenMapName));

    socket_buffer::safesize_t entitiesSize = 0;
    auto ptrEntities = decompressedBuffer.readVar<socket_buffer::type_array>(
        &entitiesSize);

    vecEntities.resize(static_cast<size_t>(entitiesSize) /
                       sizeof(EntityNetwork));

    memcpy(vecEntities.data(), ptrEntities, static_cast<size_t>(entitiesSize));

    assert(decompressedBuffer.m_readSize == newSize);
}

auto main(int argc, char* argv[]) -> int
{
    std::string ip;
    unsigned short port;

    if (argc < 3)
    {
        std::cout << "Please enter the ip of the proxy: ";
        std::cin >> ip;
        std::cout << std::endl;
        std::cout << "Please enter the port of the proxy: ";
        std::cin >> port;
        std::cout << std::endl;
    }
    else
    {
        ip = argv[1];
        port = static_cast<unsigned short>(std::atoi(argv[2]));
    }

    std::cout << "Listenning on " << ip << ":" << port << std::endl;

    boost::asio::io_service io_service;
    boost::asio::ip::udp::endpoint receiver_endpoint(
        boost::asio::ip::address::from_string(ip), port);

    boost::asio::ip::udp::socket socket(
        io_service,
        boost::asio::ip::udp::endpoint(boost::asio::ip::udp::v4(), port));

    pRadar_t radar = nullptr;

    while (true)
    {
        std::string strMapName = "";
        std::vector<EntityNetwork> vecEntities;

        recvPacket(socket, vecEntities, strMapName);

        if (!strMapName.empty())
        {
            // Recreate radar.
            if (radar && (radar->m_strMapName != strMapName))
            {
                std::cout << "Creating radar " << strMapName << std::endl;
                auto oldWindowSize = radar->m_pWindow->getSize();
                auto oldWindowPos = radar->m_pWindow->getPosition();
                deleteRadar(radar->m_id);
                radar = createRadar(strMapName, oldWindowSize, oldWindowPos);
            }
            else
            {
                if (radar == nullptr)
                {
                    std::cout << "Creating radar " << strMapName << std::endl;
                    radar = createRadar(strMapName);
                }
            }
        }

        for (auto&& entity : vecEntities)
        {
            sf::Vector2f pos2d(entity.m_vecPosition[coordinates::X],
                               entity.m_vecPosition[coordinates::Y]);

            auto coords = radar->worldToMap(pos2d, entity.m_angleOrientation);

            Entity2D entity2D;
            entity2D.m_bDraw = entity.m_bDraw;
            entity2D.m_angleOrientation = std::get<1>(coords);

            auto pos = std::get<0>(coords);

            memcpy(entity2D.m_vecPosition, &pos.x, sizeof(sf::Vector2f));

            entity2D.m_iHealth = entity.m_iHealth;
            entity2D.m_iIndex = entity.m_iIndex;
            entity2D.m_iTeamNum = entity.m_iTeamNum;
            strcpy(entity2D.m_tableName, entity.m_tableName);

            radar->updateEntity(entity2D);
        }

        // Let's see the ones we do not need to draw.
        // So it's synchronized the main radar thread.

        //        for (auto it = radar->m_vecEntities.begin();
        //             it != radar->m_vecEntities.end();)
        //        {
        //            for (auto&& updatedEntity : vecUpdatedEntities)
        //            {
        //                if (it->m_iIndex == updatedEntity)
        //                {
        //                    goto end;
        //                }
        //            }

        //            it->m_bUpdated = false;
        //        end:
        //            it++;
        //        }
    }
}
