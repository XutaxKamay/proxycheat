#include <radar_pch.h>
#include <entity.h>

Entity2D::Entity2D()
{
    memset(m_tableName, 0, sizeof(m_tableName));
    m_vecPosition[coordinates::X] = __FLT_MAX__;
    m_vecPosition[coordinates::Y] = __FLT_MAX__;
    m_angleOrientation = __FLT_MAX__;
    m_iHealth = -1;
    m_iIndex = -1;
    m_iTeamNum = -1;
    m_bDraw = false;
}

EntityNetwork::EntityNetwork()
{
    memset(m_tableName, 0, sizeof(m_tableName));
    m_vecPosition[coordinates::X] = __FLT_MAX__;
    m_vecPosition[coordinates::Y] = __FLT_MAX__;
    m_angleOrientation = __FLT_MAX__;
    m_iHealth = -1;
    m_iIndex = -1;
    m_iTeamNum = -1;
    m_bDraw = false;
}
