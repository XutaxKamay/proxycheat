#pragma once

#include <basetypes.h>
#include <platform.h>
#include <assert.h>
#include <stdio.h>
#include <string>
#include <stdint.h>

#if !defined(MAX_OSPATH)
    #define MAX_OSPATH 260 // max length of a filesystem pathname
#endif

struct QAngle
{
    float x, y, z;
    void Init(void)
    {
        x = y = z = 0.0f;
    }
    void Init(float _x, float _y, float _z)
    {
        x = _x;
        y = _y;
        z = _z;
    }
};

struct Vector
{
    float x, y, z;
    // Vector()
    //{
    //	x = 0;
    //	y = 0;
    //	z = 0;
    //}
    // Vector(float _x, float _y, float _z)
    //{
    //	x = _x;
    //	y = _y;
    //	z = _z;
    //}
    void Init(void)
    {
        x = y = z = 0.0f;
    }
    void Init(float _x, float _y, float _z)
    {
        x = _x;
        y = _y;
        z = _z;
    }
    void Set(float _x, float _y, float _z)
    {
        x = _x;
        y = _y;
        z = _z;
    }
    void Set(Vector v)
    {
        x = v.x;
        y = v.y;
        z = v.z;
    }
};