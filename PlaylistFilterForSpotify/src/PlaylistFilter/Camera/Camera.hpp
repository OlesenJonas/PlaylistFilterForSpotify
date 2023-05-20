#ifndef CAMERA_H
#define CAMERA_H

#define _USE_MATH_DEFINES
#include <array>
#include <math.h>
#include <string>

#include <glm/ext.hpp>
#include <glm/glm.hpp>

#include "CameraPreset.hpp"

#define CAMERA_ORBIT 0
#define CAMERA_FLY 1

#define POS_FROM_2_POLAR(theta, phi) glm::vec3(sin(theta) * sin(phi), cos(theta), sin(theta) * cos(phi))

/*
    TODO:
    look sensitivity, scroll sensitivity, pan sensitivity
*/

class Camera
{
  public:
    Camera(std::string name, float _aspect);
    Camera(float _aspect);
    // only using it in one case, may need to remove later
    Camera(Camera const&) = default;

    void move(glm::vec3 offset);
    void setPosition(glm::vec3& newPosition);
    void setCenter(glm::vec3 newCenter);
    void rotate(float dx, float dy);
    void changeRadius(bool increase);
    // Set vertical! Fov in radians.
    void setFov(float _fov);
    void setAspect(float _aspect);
    void setMode(uint8_t _mode);
    void updateView();

    glm::vec3 getPosition();
    glm::mat4* getView();
    glm::mat4* getProj();
    const float* getMatricesPointer();

    CameraPreset savePreset();
    void loadPreset(const CameraPreset& preset);

    uint8_t mode = 0;
    float fov = glm::radians(60.0f);
    float cam_near = 0.1f;
    float cam_far = 100.0f;
    float flySpeed = 2.0f;

    std::string name;

  private:
    void init();
    float phi = 0.0f;
    float theta = glm::pi<float>() * 0.5;
    float radius = 1.0f, aspect = 1.77f;
    // orbit uses center+viewVec as eye and center         as target
    // fly   uses center         as eye and center+viewVec as target
    glm::vec3 position;
    glm::vec3 viewVec, center;
    glm::vec3 up, camx, camy, camz;
    //[0] is view, [1] is projection, [2] inverse Projection
    std::array<glm::mat4, 3> matrices;
};

#endif // CAMERA_H