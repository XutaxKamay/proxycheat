#ifndef RADAR_H
#define RADAR_H

#include <entity.h>
#include <radar_pch.h>

class Radar;
typedef Radar* pRadar_t;

class Radar
{
 public:
    Radar(std::string strMapName = "",
          sf::Vector2u windowSize = sf::Vector2u(0, 0),
          sf::Vector2i windowPos = sf::Vector2i(0, 0));
    ~Radar();

    // Update entity.
    auto updateEntity(Entity2D& entity) -> bool;

    // Loads the overview image of the map and get its surface.
    auto loadMap() -> bool;
    // Loads the image of the marker and get its surface.
    auto loadMarker(const std::string& pathImage) -> bool;
    // Render the entities
    auto renderMarkers(float scaleX = 1.0f, float scaleY = 1.0f) -> bool;
    // Render the map
    auto renderMap(float scaleX = 1.0f, float scaleY = 1.0f) -> bool;
    // Creates the window for rendering
    auto createWindow() -> bool;
    // Main thread function of the radar
    auto mainFrame() -> void;
    // Init radar
    auto init() -> bool;
    // Handle events
    auto handleEvents() -> void;
    // Convert 3D position to map position.
    auto worldToMap(sf::Vector2f vecPosition,
                    float angleOrientation = __FLT_MAX__)
        -> std::tuple<sf::Vector2f, float>;

 public:
    // Bounds fields.
    sf::Vector2f m_vecMapOrigin;
    // Map scaling.
    float m_fMapScale;
    // Map rotation?
    bool m_bMapRotate;
    // Entities list.
    std::vector<Entity2D> m_vecEntities;
    // Map name.
    std::string m_strMapName;

    /// SFML Stuffs

    // SFML Window
    sf::Window* m_pWindow;
    // SFML Renderer
    sf::RenderWindow* m_pRenderer;
    // Texture to the image of the overview of the map.
    sf::Texture* m_pMapTexture;
    // Texture to the entity marker.
    sf::Texture* m_pMarkerTexture;

    // Radar frame count.
    uint64_t m_frameCount;
    // Radar ID
    int m_id;
    // Radar thread
    sf::Thread m_thread;
    // Should we terminate radar?
    bool m_bTerminate;
    // Radar ratio for scaling the map texture.
    float m_renderRatioX;
    float m_renderRatioY;

 public:
    // Count radars for setting up their IDs.
    static int m_countRadars;
    // Vectors of radars
    static std::vector<pRadar_t> m_vecRadars;

 private:
    sf::Vector2u m_initWindowSize;
    sf::Vector2i m_initWindowPos;
};

auto createRadar(std::string strMapName,
                 sf::Vector2u windowSize = sf::Vector2u(0, 0),
                 sf::Vector2i windowPos = sf::Vector2i(0, 0)) -> pRadar_t;

auto deleteRadar(int id) -> bool;

#endif
