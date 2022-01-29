#pragma once

struct CameraPreset
{
    float fov;
    float cam_near;
    float cam_far;
    float flySpeed;
    float phi;
    float theta;
    float radius;
    float aspect;

    float position[3];
    float viewVec[3];
    float center[3];
    float up[3];
    float camx[3];
    float camy[3];
    float camz[3];
    float matrices[4 * 4 * 3];
};