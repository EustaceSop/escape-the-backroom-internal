#pragma once
#include <Windows.h>
#include <cmath>
#include <d3d9types.h>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

class Vector3
{
public:
    float x, y, z;

    Vector3() : x(0.f), y(0.f), z(0.f) {}
    Vector3(float _x, float _y, float _z) : x(_x), y(_y), z(_z) {}

    inline float Dot(const Vector3& v) const { return x * v.x + y * v.y + z * v.z; }
    inline float Distance(const Vector3& v) const
    {
        return sqrtf((v.x - x) * (v.x - x) + (v.y - y) * (v.y - y) + (v.z - z) * (v.z - z));
    }
    inline Vector3 operator+(const Vector3& v) const { return Vector3(x + v.x, y + v.y, z + v.z); }
    inline Vector3 operator-(const Vector3& v) const { return Vector3(x - v.x, y - v.y, z - v.z); }
    inline Vector3 operator*(float s) const { return Vector3(x * s, y * s, z * s); }
};

struct FQuat
{
    float x, y, z, w;
};

struct FTransform
{
    FQuat rot;
    Vector3 translation;
    char pad[4];
    Vector3 scale;
    char pad1[4];

    D3DMATRIX ToMatrixWithScale()
    {
        D3DMATRIX m{};
        m._41 = translation.x;
        m._42 = translation.y;
        m._43 = translation.z;

        float x2 = rot.x + rot.x;
        float y2 = rot.y + rot.y;
        float z2 = rot.z + rot.z;

        float xx2 = rot.x * x2;
        float yy2 = rot.y * y2;
        float zz2 = rot.z * z2;
        m._11 = (1.0f - (yy2 + zz2)) * scale.x;
        m._22 = (1.0f - (xx2 + zz2)) * scale.y;
        m._33 = (1.0f - (xx2 + yy2)) * scale.z;

        float yz2 = rot.y * z2;
        float wx2 = rot.w * x2;
        m._32 = (yz2 - wx2) * scale.z;
        m._23 = (yz2 + wx2) * scale.y;

        float xy2 = rot.x * y2;
        float wz2 = rot.w * z2;
        m._21 = (xy2 - wz2) * scale.y;
        m._12 = (xy2 + wz2) * scale.x;

        float xz2 = rot.x * z2;
        float wy2 = rot.w * y2;
        m._31 = (xz2 + wy2) * scale.z;
        m._13 = (xz2 - wy2) * scale.x;

        m._14 = 0.0f;
        m._24 = 0.0f;
        m._34 = 0.0f;
        m._44 = 1.0f;

        return m;
    }
};

inline D3DMATRIX Matrix(Vector3 rot, Vector3 origin = Vector3(0, 0, 0))
{
    float radPitch = rot.x * float(M_PI) / 180.f;
    float radYaw = rot.y * float(M_PI) / 180.f;
    float radRoll = rot.z * float(M_PI) / 180.f;

    float SP = sinf(radPitch), CP = cosf(radPitch);
    float SY = sinf(radYaw), CY = cosf(radYaw);
    float SR = sinf(radRoll), CR = cosf(radRoll);

    D3DMATRIX matrix{};
    matrix.m[0][0] = CP * CY;
    matrix.m[0][1] = CP * SY;
    matrix.m[0][2] = SP;
    matrix.m[0][3] = 0.f;

    matrix.m[1][0] = SR * SP * CY - CR * SY;
    matrix.m[1][1] = SR * SP * SY + CR * CY;
    matrix.m[1][2] = -SR * CP;
    matrix.m[1][3] = 0.f;

    matrix.m[2][0] = -(CR * SP * CY + SR * SY);
    matrix.m[2][1] = CY * SR - CR * SP * SY;
    matrix.m[2][2] = CR * CP;
    matrix.m[2][3] = 0.f;

    matrix.m[3][0] = origin.x;
    matrix.m[3][1] = origin.y;
    matrix.m[3][2] = origin.z;
    matrix.m[3][3] = 1.f;

    return matrix;
}

inline D3DMATRIX MatrixMultiplication(D3DMATRIX pM1, D3DMATRIX pM2)
{
    D3DMATRIX pOut{};
    for (int i = 0; i < 4; i++)
    {
        for (int j = 0; j < 4; j++)
        {
            float sum = 0.f;
            for (int k = 0; k < 4; k++)
                sum += pM1.m[i][k] * pM2.m[k][j];
            pOut.m[i][j] = sum;
        }
    }
    return pOut;
}

struct CameraInfo
{
    Vector3 Location;
    Vector3 Rotation;
    float FOV = 90.f;
};

inline Vector3 ProjectWorldToScreenEx(Vector3 WorldLocation, const CameraInfo& cam, float screenW, float screenH)
{
    Vector3 Screenlocation(0, 0, 0);

    D3DMATRIX tempMatrix = Matrix(cam.Rotation);
    Vector3 vAxisX(tempMatrix.m[0][0], tempMatrix.m[0][1], tempMatrix.m[0][2]);
    Vector3 vAxisY(tempMatrix.m[1][0], tempMatrix.m[1][1], tempMatrix.m[1][2]);
    Vector3 vAxisZ(tempMatrix.m[2][0], tempMatrix.m[2][1], tempMatrix.m[2][2]);

    Vector3 vDelta = WorldLocation - cam.Location;
    Vector3 vTransformed(vDelta.Dot(vAxisY), vDelta.Dot(vAxisZ), vDelta.Dot(vAxisX));

    if (vTransformed.z < 1.f)
        vTransformed.z = 1.f;

    float ScreenCenterX = screenW / 2.0f;
    float ScreenCenterY = screenH / 2.0f;

    Screenlocation.x = ScreenCenterX + vTransformed.x * (ScreenCenterX / tanf(cam.FOV * (float)M_PI / 360.f)) / vTransformed.z;
    Screenlocation.y = ScreenCenterY - vTransformed.y * (ScreenCenterX / tanf(cam.FOV * (float)M_PI / 360.f)) / vTransformed.z;

    return Screenlocation;
}
