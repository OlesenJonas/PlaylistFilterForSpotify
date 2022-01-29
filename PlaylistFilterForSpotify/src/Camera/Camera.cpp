#include "Camera.h"

Camera::Camera(std::string name, float _aspect)
{
    aspect = _aspect;
    this->name = name;
    init();
}

Camera::Camera(float _aspect)
{
    aspect = _aspect;
    name = "CameraDefaultName";
    init();
}

void Camera::init()
{
    viewVec = POS_FROM_2_POLAR(theta, phi);
    center = glm::vec3(0.0f);
    glm::mat4& view = matrices[0];
    view = glm::lookAt(center + radius * viewVec, center, glm::vec3(0.f, 1.f, 0.f));
    position = center + radius * viewVec;
    camx = glm::vec3(view[0][0], view[1][0], view[2][0]);
    camy = glm::vec3(view[0][1], view[1][2], view[2][3]);
    camz = glm::vec3(view[0][2], view[1][2], view[2][2]);
    matrices[1] = glm::perspective(fov, aspect, cam_near, cam_far);
    matrices[2] = glm::inverse(matrices[1]);
}

void Camera::setPosition(glm::vec3& newPosition)
{
    glm::vec3 offset = newPosition - position;
    position += offset;
    center += offset;
}

void Camera::setCenter(glm::vec3 newCenter)
{
    glm::vec3 offset = position - center;
    center = newCenter;
    position = center + offset;
}

void Camera::updateView()
{
    viewVec = POS_FROM_2_POLAR(theta, phi);
    glm::mat4& view = matrices[0];
    if(mode == CAMERA_ORBIT)
    {
        glm::vec3 eye = center + radius * viewVec;
        view = glm::lookAt(center + radius * viewVec, center, glm::vec3(0.f, 1.f, 0.f));
        position = center + radius * viewVec;
    }
    else if(mode == CAMERA_FLY)
    {
        view = glm::lookAt(center, center + viewVec, glm::vec3(0.f, 1.f, 0.f));
        position = center;
    }
    camx = glm::vec3(view[0][0], view[1][0], view[2][0]);
    camy = glm::vec3(view[0][1], view[1][1], view[2][1]);
    camz = glm::vec3(view[0][2], view[1][2], view[2][2]);
}

// move the camera along its local axis
void Camera::move(glm::vec3 offset)
{
    glm::vec3 offs = offset.x * camx + offset.y * camy + offset.z * camz;
    center += (mode == CAMERA_FLY ? flySpeed : 1.0f) * offs;
}

void Camera::rotate(float dx, float dy)
{
    theta = std::min<float>(std::max<float>(theta - dy * 0.01f, 0.1f), M_PI - 0.1f);
    phi = phi - dx * 0.01f;
}

void Camera::changeRadius(bool increase)
{
    if(increase)
        radius /= 0.95f;
    else
        radius *= 0.95f;
}

void Camera::setFov(float _fov)
{
    fov = _fov;
    matrices[1] = glm::perspective(fov, aspect, cam_near, cam_far);
    matrices[2] = glm::inverse(matrices[1]);
}

void Camera::setAspect(float _aspect)
{
    aspect = _aspect;
    matrices[1] = glm::perspective(fov, aspect, cam_near, cam_far);
    matrices[2] = glm::inverse(matrices[1]);
}

void Camera::setMode(uint8_t _mode)
{
    if(mode == _mode)
        return;
    mode = _mode;

    // flip viewvecter and sawp center and target
    theta = M_PI - theta;
    phi = phi + M_PI;
    center = center + radius * viewVec;
    viewVec = -viewVec;
}

glm::mat4* Camera::getView()
{
    return &matrices[0];
}

glm::mat4* Camera::getProj()
{
    return &matrices[1];
}

const float* Camera::getMatricesPointer()
{
    return glm::value_ptr(matrices[0]);
}

glm::vec3 Camera::getPosition()
{
    return position;
}

CameraPreset Camera::savePreset()
{
    CameraPreset preset;
    preset.fov = fov;
    preset.cam_near = cam_near;
    preset.cam_far = cam_far;
    preset.flySpeed = flySpeed;
    preset.phi = phi;
    preset.theta = theta;
    preset.radius = radius;
    preset.aspect = aspect;
    std::memcpy(&preset.position[0], glm::value_ptr(position), 3 * sizeof(float));
    std::memcpy(&preset.center[0], glm::value_ptr(center), 3 * sizeof(float));
    std::memcpy(&preset.viewVec[0], glm::value_ptr(viewVec), 3 * sizeof(float));
    std::memcpy(&preset.up[0], glm::value_ptr(up), 3 * sizeof(float));
    std::memcpy(&preset.camx[0], glm::value_ptr(camx), 3 * sizeof(float));
    std::memcpy(&preset.camy[0], glm::value_ptr(camy), 3 * sizeof(float));
    std::memcpy(&preset.camz[0], glm::value_ptr(camz), 3 * sizeof(float));
    std::memcpy(&preset.matrices[0], glm::value_ptr(matrices[0]), 4 * 4 * 3 * sizeof(float));

    return preset;
}

void Camera::loadPreset(const CameraPreset& preset)
{
    fov = preset.fov;
    cam_near = preset.cam_near;
    cam_far = preset.cam_far;
    flySpeed = preset.flySpeed;
    phi = preset.phi;
    theta = preset.theta;
    radius = preset.radius;
    aspect = preset.aspect;
    std::memcpy(glm::value_ptr(position), &preset.position[0], 3 * sizeof(float));
    std::memcpy(glm::value_ptr(center), &preset.center[0], 3 * sizeof(float));
    std::memcpy(glm::value_ptr(viewVec), &preset.viewVec[0], 3 * sizeof(float));
    std::memcpy(glm::value_ptr(up), &preset.up[0], 3 * sizeof(float));
    std::memcpy(glm::value_ptr(camx), &preset.camx[0], 3 * sizeof(float));
    std::memcpy(glm::value_ptr(camy), &preset.camy[0], 3 * sizeof(float));
    std::memcpy(glm::value_ptr(camz), &preset.camz[0], 3 * sizeof(float));
    std::memcpy(glm::value_ptr(matrices[0]), &preset.matrices[0], 4 * 4 * 3 * sizeof(float));
}