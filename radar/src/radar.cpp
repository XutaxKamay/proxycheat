#include <radar.h>
#include <radar_pch.h>

std::vector<pRadar_t> Radar::m_vecRadars;
int Radar::m_countRadars = 0;

Radar::Radar(std::string strMapName,
             sf::Vector2u windowSize,
             sf::Vector2i windowPos) :
    m_strMapName(strMapName),
    m_pWindow(nullptr), m_pRenderer(nullptr), m_pMapTexture(nullptr),
    m_pMarkerTexture(nullptr), m_frameCount(0), m_id(-1),
    m_thread(&Radar::mainFrame, this), m_bTerminate(false),
    m_renderRatioX(0.5f), m_renderRatioY(0.5f), m_initWindowSize(windowSize),
    m_initWindowPos(windowPos)
{
    m_countRadars++;
    m_id = m_countRadars;
    m_thread.launch();
}

Radar::~Radar()
{
    // Wait for thread terminating
    m_bTerminate = true;
    m_thread.wait();

    delete m_pMapTexture;
    delete m_pMarkerTexture;
    delete m_pRenderer;
    delete m_pWindow;

    m_countRadars--;
}

auto Radar::updateEntity(Entity2D& entityUpdate) -> bool
{
    bool shouldAdd = false;

    for (auto&& entity : m_vecEntities)
    {
        if (entityUpdate.m_iIndex == entity.m_iIndex)
        {
            memcpy(&entity, &entityUpdate, sizeof(Entity2D));
            shouldAdd = true;
            break;
        }
    }

    if (!shouldAdd)
    {
        m_vecEntities.push_back(entityUpdate);
    }

    return true;
}

auto Radar::loadMap() -> bool
{
    if (m_pMapTexture)
        delete m_pMapTexture;

    auto mapOverviewPath = "radar/overviews/" + m_strMapName + "_radar.png";

    m_pMapTexture = new sf::Texture();
    try
    {
        if (m_pMapTexture == nullptr ||
            !m_pMapTexture->loadFromFile(mapOverviewPath))
        {
            std::ostringstream strstream;
            strstream << sf::err().rdbuf();
            throw("The image extension " + mapOverviewPath +
                  " is not valid. ( " + strstream.str() + " )");
        }
    }
    catch (const std::string& e)
    {
        std::cerr << e << '\n';
        return false;
    }

    m_pMapTexture->setSmooth(true);

    // Load map origin & scale
    auto mapInfoPath = "radar/overviews/" + m_strMapName + ".txt";

    std::vector<char> keyValues;
    std::ifstream mapFileInfo(mapInfoPath);

    mapFileInfo.seekg(0, std::ios::end);

    keyValues.resize(static_cast<size_t>(mapFileInfo.tellg()));

    mapFileInfo.seekg(0);
    mapFileInfo.read(keyValues.data(),
                     static_cast<std::streamsize>(keyValues.size()));
    mapFileInfo.close();

    std::string strKeyValues(keyValues.data(), keyValues.size());

    auto getValueFromKey = [&strKeyValues](std::string key) {
        std::string strValue = "";

        key = "\"" + key + "\"";

        auto keyPos = strKeyValues.find(key);

        if (keyPos != std::string::npos)
        {
            auto value = strKeyValues.find('"', keyPos + 1 + key.length());

            if (value != std::string::npos)
            {
                auto valueEnd = strKeyValues.find('"', value + 1);
                // ignore all "
                strValue = strKeyValues.substr(value + 1,
                                               (valueEnd - value) - 1);
            }
        }

        return strValue;
    };

    m_fMapScale = static_cast<float>(
        std::atof(getValueFromKey("scale").c_str()));
    m_vecMapOrigin.x = static_cast<float>(
        std::atof(getValueFromKey("pos_x").c_str()));
    m_vecMapOrigin.y = static_cast<float>(
        std::atof(getValueFromKey("pos_y").c_str()));
    m_bMapRotate = static_cast<bool>(
        std::atoi(getValueFromKey("rotate").c_str()));

    if (m_fMapScale == 0.0f)
    {
        assert("Couldn't find map scale.");
    }

    return true;
}

auto Radar::loadMarker(const std::string& pathImage) -> bool
{
    if (m_pMarkerTexture)
        delete m_pMarkerTexture;

    m_pMarkerTexture = new sf::Texture();

    try
    {
        if (m_pMarkerTexture == nullptr ||
            !m_pMarkerTexture->loadFromFile(pathImage))
        {
            std::ostringstream strstream;
            strstream << sf::err().rdbuf();
            throw("The image extension or path " + pathImage +
                  " is not valid. ( " + strstream.str() + " )");
        }
    }
    catch (const std::string& e)
    {
        std::cerr << e << '\n';
        return false;
    }

    m_pMarkerTexture->setSmooth(true);
    m_pMarkerTexture->setRepeated(true);

    return true;
}

auto Radar::createWindow() -> bool
{
    if (m_pMapTexture == nullptr)
    {
        assert("No map texture");
        return false;
    }

    auto size = m_pMapTexture->getSize();

    if (m_pWindow == nullptr)
        m_pWindow = new sf::Window(sf::VideoMode(size.x, size.y),
                                   m_strMapName,
                                   sf::Style::Default);

    try
    {
        if (m_pWindow == nullptr)
        {
            std::ostringstream strstream;
            strstream << sf::err().rdbuf();
            throw("Couldn't create window " + m_strMapName + "( " +
                  strstream.str() + " )");
        }
    }
    catch (const std::string& e)
    {
        std::cerr << e << '\n';
        return false;
    }

    m_pWindow->setVerticalSyncEnabled(true);

    if (m_pRenderer == nullptr)
        m_pRenderer = new sf::RenderWindow(m_pWindow->getSystemHandle());

    try
    {
        if (m_pRenderer == nullptr)
        {
            std::ostringstream strstream;
            strstream << sf::err().rdbuf();
            throw("Couldn't create renderer " + m_strMapName + "( " +
                  strstream.str() + " )");
        }
    }
    catch (const std::string& e)
    {
        std::cerr << e << '\n';
        return false;
    }

    m_pRenderer->setVerticalSyncEnabled(true);

    return true;
}

auto Radar::renderMarkers(float scaleX, float scaleY) -> bool
{
    // Resize accordingly the window size
    constexpr auto defaultSize = 0.05f;
    // scaleX /= m_fMapScale;
    // scaleY /= m_fMapScale;
    scaleX *= defaultSize;
    scaleY *= defaultSize;

    if (scaleX < 0.025f)
        scaleX = 0.025f;
    if (scaleY < 0.025f)
        scaleY = 0.025f;

    sf::Sprite sprite;
    sprite.setTexture(*m_pMarkerTexture);
    sprite.scale(scaleX, scaleY);
    auto centerX = sprite.getLocalBounds().width / 2;
    auto centerY = sprite.getLocalBounds().height / 2;
    sprite.setOrigin(sf::Vector2f(centerX, centerY));

    for (auto&& entity : m_vecEntities)
    {
        auto entityInWhitelist = [&entity]() {
            auto tableName = std::string(entity.m_tableName);

            for (auto&& tableNameWhiteList : tableList)
            {
                if (tableName.find(tableNameWhiteList) != std::string::npos)
                {
                    return true;
                }
            }

            return false;
        };

        if (!entityInWhitelist() || !entity.m_bDraw ||
            entity.m_angleOrientation == __FLT_MAX__)
        {
            continue;
        }

        // Others Entities
        if (std::string(entity.m_tableName) != "DT_CSPlayer")
        {
            auto color = sf::Color(0x00000033);

            if (entity.m_iHealth > 0)
            {
                if (entity.m_iTeamNum == 2)
                    color = sf::Color::Red;
                else if (entity.m_iTeamNum == 3)
                    color = sf::Color::Blue;
            }
            else
            {
                if (entity.m_iTeamNum == 2)
                    color = sf::Color(0xFFD00033);
                else if (entity.m_iTeamNum == 3)
                    color = sf::Color(0x0000FF33);
            }

            sprite.setColor(color);
        }
        // Players
        else
        {
            auto color = sf::Color::Green;

            if (entity.m_iHealth > 0)
            {
                if (entity.m_iTeamNum == 2)
                    color = sf::Color(0xFFD011FF);
                else if (entity.m_iTeamNum == 3)
                    color = sf::Color(0x00AEFFFF);
                else if (entity.m_iTeamNum == -1)
                    color = sf::Color::White;
            }
            else
            {
                if (entity.m_iTeamNum == 2)
                    color = sf::Color(0xFFD01133);
                else if (entity.m_iTeamNum == 3)
                    color = sf::Color(0x00AEFF33);
                else if (entity.m_iTeamNum == -1)
                    color = sf::Color(0xFFFFFF33);
            }

            sprite.setColor(color);
        }

        sprite.setPosition(sf::Vector2((entity.m_vecPosition[coordinates::X]),
                                       (entity.m_vecPosition[coordinates::Y])));

        sprite.setRotation(entity.m_angleOrientation);

        // Draw on screen.
        m_pRenderer->draw(sprite);
    }

    return true;
}

auto Radar::renderMap(float scaleX, float scaleY) -> bool
{
    // Resize accordingly the window size
    sf::Sprite sprite;
    sprite.setTexture(*m_pMapTexture);
    sprite.scale(scaleX, scaleY);

    // Draw on screen.
    m_pRenderer->draw(sprite);

    return true;
}

auto Radar::init() -> bool
{
    if (loadMarker("radar/marker.png") && loadMap() && createWindow())
    {
        if (m_initWindowPos.x == 0 && m_initWindowPos.y == 0 &&
            m_initWindowSize.x == 0 && m_initWindowSize.y == 0)
        {
            auto sizeInitMap = m_pMapTexture->getSize();

            sizeInitMap.x /= 2.0f;
            sizeInitMap.y /= 2.0f;

            m_pWindow->setSize(sizeInitMap);
            m_pRenderer->setSize(sizeInitMap);

            auto desktop = sf::VideoMode::getDesktopMode();
            auto posCenter = sf::Vector2i(desktop.width / 2 - sizeInitMap.x / 2,
                                          desktop.height / 2 -
                                              sizeInitMap.y / 2);

            m_pWindow->setPosition(posCenter);
            m_pRenderer->setPosition(posCenter);

            m_initWindowPos = posCenter;
            m_initWindowSize = sizeInitMap;

            // Wait all events being done.
            {
                sf::Event event;
                m_pRenderer->waitEvent(event);
                m_pWindow->waitEvent(event);
            }

            // Init ratio.
            m_renderRatioX = static_cast<float>(sizeInitMap.x) /
                             static_cast<float>(m_pMapTexture->getSize().x);
            m_renderRatioY = static_cast<float>(sizeInitMap.y) /
                             static_cast<float>(m_pMapTexture->getSize().y);
        }
        else
        {
            m_pWindow->setSize(m_initWindowSize);
            m_pRenderer->setSize(m_initWindowSize);

            m_pWindow->setPosition(m_initWindowPos);
            m_pRenderer->setPosition(m_initWindowPos);

            // Wait all events being done.
            {
                sf::Event event;
                m_pRenderer->waitEvent(event);
                m_pWindow->waitEvent(event);
            }

            // Init ratio.
            m_renderRatioX = static_cast<float>(m_initWindowSize.x) /
                             static_cast<float>(m_pMapTexture->getSize().x);
            m_renderRatioY = static_cast<float>(m_initWindowSize.y) /
                             static_cast<float>(m_pMapTexture->getSize().y);
        }

        m_pWindow->setTitle(m_strMapName);

        return true;
    }

    return false;
}

auto Radar::handleEvents() -> void
{
    sf::Event event;
    while (m_pWindow->pollEvent(event))
    {
        switch (event.type)
        {
            case sf::Event::Closed:
            {
                exit(0);
                break;
            }

            case sf::Event::Resized:
            {
                sf::Vector2 newSize(event.size.width, event.size.height);

                m_renderRatioX = static_cast<float>(newSize.x) /
                                 static_cast<float>(m_pMapTexture->getSize().x);
                m_renderRatioY = static_cast<float>(newSize.y) /
                                 static_cast<float>(m_pMapTexture->getSize().y);

                // Limit scaling
                if (m_renderRatioX > 1.0f)
                {
                    m_renderRatioX = 1.0f;
                }

                if (m_renderRatioY > 1.0f)
                {
                    m_renderRatioY = 1.0f;
                }

                // Do same scale for both X & Y
                if (m_renderRatioX > m_renderRatioY)
                {
                    m_renderRatioX = m_renderRatioY;
                }
                else if (m_renderRatioX < m_renderRatioY)
                {
                    m_renderRatioY = m_renderRatioX;
                }
                break;
            }

            default:
                break;
        }
    }
}

auto Radar::mainFrame() -> void
{
    if (!init())
    {
        std::cout << "Couldn't init radar " << m_id << std::endl;
        return;
    }

    while (!m_bTerminate)
    {
        handleEvents();

        auto renderEverything = [this]() {
            m_pRenderer->setSize(sf::Vector2u(
                static_cast<unsigned int>(
                    m_renderRatioX * m_pMapTexture->getSize().x + 0.5f),
                static_cast<unsigned int>(
                    m_renderRatioY * m_pMapTexture->getSize().y + 0.5f)));

            m_pRenderer->clear();

            renderMap();

            renderMarkers();

            m_pRenderer->display();

            m_pWindow->setSize(m_pRenderer->getSize());
        };

        renderEverything();

        // Update frame count.
        m_frameCount++;
    }

    if (!m_bTerminate)
        m_bTerminate = true;
}

auto Radar::worldToMap(sf::Vector2f vecPosition, float angleOrientation)
    -> std::tuple<sf::Vector2f, float>
{
    sf::Vector2f convertedPosition(vecPosition.x - m_vecMapOrigin.x,
                                   vecPosition.y - m_vecMapOrigin.y);

    convertedPosition.x /= m_fMapScale;
    convertedPosition.y /= -m_fMapScale;

    float convertedAngle = 0.0f;

    auto checkAngle = [](float& convertedAngle) {
        if (convertedAngle > 180.0f)
        {
            convertedAngle -= 360.0f;
        }
        if (convertedAngle < -180.0f)
        {
            convertedAngle += 360.0f;
        }
    };

    // std::cout << angleOrientation << std::endl;

    convertedAngle = angleOrientation;

    if (convertedAngle != __FLT_MAX__)
    {
        convertedAngle = convertedAngle - 90.0f;
        checkAngle(convertedAngle);

        convertedAngle = 360.0f - convertedAngle;
        checkAngle(convertedAngle);
    }

    return std::tuple(convertedPosition, convertedAngle);
}

auto createRadar(std::string strMapName,
                 sf::Vector2u windowSize,
                 sf::Vector2i windowPos) -> pRadar_t
{
    auto radar = new Radar(strMapName, windowSize, windowPos);
    Radar::m_vecRadars.push_back(radar);
    return radar;
}

auto deleteRadar(int id) -> bool
{
    bool ret = false;

    for (auto it = Radar::m_vecRadars.begin(); it != Radar::m_vecRadars.end();
         it++)
    {
        if ((*it)->m_id == id)
        {
            Radar::m_vecRadars.erase(it);
            delete (*it);
            ret = true;
            break;
        }
    }

    return ret;
}
