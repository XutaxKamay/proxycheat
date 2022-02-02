
#ifndef ENTITY_H
#define ENTITY_H

namespace coordinates
{
    enum
    {
        X,
        Y,
        Z
    };
};


#pragma pack(1)
enum
{
    packet_serverinfos,
    packet_entities
};
#pragma pack()

#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wunused-variable"
constexpr static const char* tableList[] = {
    "Player",
    "Weapon",
    "Grenade",
    "Smoke",
    "Flashbang",
    //"Ragdoll",
    "Hostage",
    "Chicken",
    "Drone",
    "Decoy",
    //"Projectile"
};
#pragma GCC diagnostic pop

class Entity2D
{
 public:
    Entity2D();

    short m_iIndex;
    char m_iHealth;
    char m_iTeamNum;
    float m_vecPosition[2];
    float m_angleOrientation;
    char m_tableName[32];
    bool m_bDraw;
};

class EntityNetwork
{
 public:
    EntityNetwork();

    short m_iIndex;
    char m_iHealth;
    char m_iTeamNum;
    float m_vecPosition[2];
    float m_angleOrientation;
    char m_tableName[32];
    bool m_bDraw;
};

#endif
