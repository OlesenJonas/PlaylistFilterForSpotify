#include "Input.hpp"
#include "App.hpp"

#include <Renderer/Renderer.hpp>

void mouseButtonCallback(GLFWwindow* window, int button, int action, int mods)
{
    auto* renderer = reinterpret_cast<Renderer*>(glfwGetWindowUserPointer(window));
    App& app = renderer->app;
    if((button == GLFW_MOUSE_BUTTON_MIDDLE || button == GLFW_MOUSE_BUTTON_RIGHT) && action == GLFW_PRESS)
    {
        glfwGetCursorPos(window, &renderer->mouse_x, &renderer->mouse_y);
    }
    if(button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_PRESS)
    {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_DISABLED);
        renderer->cam.setMode(CAMERA_FLY);
    }
    if(button == GLFW_MOUSE_BUTTON_RIGHT && action == GLFW_RELEASE)
    {
        glfwSetInputMode(window, GLFW_CURSOR, GLFW_CURSOR_NORMAL);
        renderer->cam.setMode(CAMERA_ORBIT);
    }
    if(button == GLFW_MOUSE_BUTTON_LEFT && action == GLFW_PRESS)
    {
        // todo: this all should be function of app ?

        if(ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow | ImGuiHoveredFlags_AllowWhenBlockedByPopup))
        {
            return;
        }
        double mx = 0;
        double my = 0;
        glfwGetCursorPos(window, &mx, &my);

        glm::mat4 invProj = glm::inverse(*(renderer->cam.getProj()));
        glm::mat4 invView = glm::inverse(*(renderer->cam.getView()));
        float screenX = 2.f * (static_cast<float>(mx) / renderer->width) - 1.f;
        float screenY = -2.f * (static_cast<float>(my) / renderer->height) + 1.f;
        glm::vec4 clipN{screenX, screenY, 0.0f, 1.0f};
        glm::vec4 viewN = invProj * clipN;
        viewN /= viewN.w;
        glm::vec4 worldN = invView * viewN;
        glm::vec4 clipF{screenX, screenY, 1.0f, 1.0f};
        glm::vec4 viewF = invProj * clipF;
        viewF /= viewF.w;
        glm::vec4 worldF = invView * viewF;

        // everything gets remapped [0,1] -> [-1,1] in VS
        // coverBuffer contains values in [0,1] but we want to test against visible tris
        worldF = 0.5f * worldF + 0.5f;
        worldN = 0.5f * worldN + 0.5f;

        glm::vec3 rayStart = worldN;
        glm::vec3 rayDir = (worldF - worldN);
        rayDir = glm::normalize(rayDir);

        // glm::vec3 worldCamX = glm::vec3(invView * glm::vec4(coverSize3D, 0.f, 0.f, 0.f));
        // glm::vec3 worldCamY = glm::vec3(invView * glm::vec4(0.f, coverSize3D, 0.f, 0.f));
        glm::vec3 worldCamX = glm::vec3(invView * glm::vec4(1.0f, 0.f, 0.f, 0.f));
        glm::vec3 worldCamY = glm::vec3(invView * glm::vec4(0.f, 1.0f, 0.f, 0.f));
        // need worldCamX/Y later, cant just do invView*(0,0,-1,0) here
        glm::vec3 n = glm::normalize(glm::cross(worldCamX, worldCamY));

        glm::vec3 axisMins{
            app.featureMinMaxValues[app.graphingFeatureX].x,
            app.featureMinMaxValues[app.graphingFeatureY].x,
            app.featureMinMaxValues[app.graphingFeatureZ].x};
        glm::vec3 axisMaxs{
            app.featureMinMaxValues[app.graphingFeatureX].y,
            app.featureMinMaxValues[app.graphingFeatureY].y,
            app.featureMinMaxValues[app.graphingFeatureZ].y};
        glm::vec3 axisFactors = axisMaxs - axisMins;

        glm::vec3 hitP;
        glm::vec3 debugHitP;
        glm::vec3 resP;
        struct HitResult
        {
            float t = std::numeric_limits<float>::max();
            uint32_t index = std::numeric_limits<uint32_t>::max();
        };
        HitResult hit;

        for(const auto& graphingBufferElement : app.graphingData)
        {
            glm::vec3 tboP = graphingBufferElement.p;
            tboP = (tboP - axisMins) / axisFactors;
            float t = glm::dot(tboP - rayStart, n) / glm::dot(rayDir, n);
            t = std::max(0.f, t);
            hitP = rayStart + t * rayDir;

            float localX = glm::dot(hitP - tboP, worldCamX);
            float localY = glm::dot(hitP - tboP, worldCamY);
            bool insideSquare =
                std::abs(localX) < 0.5f * app.coverSize3D && std::abs(localY) < 0.5f * app.coverSize3D;
            if(insideSquare && t < hit.t)
            {
                hit.t = t;
                hit.index = graphingBufferElement.originalIndex;
                resP = tboP;
                debugHitP = hitP;
            }
        }
        app.selectedTrack = nullptr;
        if(hit.index != std::numeric_limits<uint32_t>::max())
        {
            app.selectedTrack = &(app.playlist)[hit.index];
        }
    }
}

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    auto* renderer = reinterpret_cast<Renderer*>(glfwGetWindowUserPointer(window));
    App& app = renderer->app;
    if(key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GL_TRUE);
    if(key == GLFW_KEY_TAB && action == GLFW_PRESS)
        app.uiHidden = !app.uiHidden;
}

void scrollCallback(GLFWwindow* window, double xoffset, double yoffset)
{
    Renderer* renderer = reinterpret_cast<Renderer*>(glfwGetWindowUserPointer(window));
    if(!ImGui::IsWindowHovered(ImGuiHoveredFlags_AnyWindow))
    {
        if(renderer->cam.mode == CAMERA_ORBIT)
            renderer->cam.changeRadius(yoffset < 0);
        else if(renderer->cam.mode == CAMERA_FLY)
        {
            float factor = (yoffset > 0) ? 1.1f : 1 / 1.1f;
            renderer->cam.flySpeed *= factor;
        }
    }
}

void resizeCallback(GLFWwindow* window, int w, int h)
{
    Renderer* renderer = reinterpret_cast<Renderer*>(glfwGetWindowUserPointer(window));
    renderer->width = w;
    renderer->height = h;
    renderer->cam.setAspect(static_cast<float>(w) / static_cast<float>(h));
    glViewport(0, 0, w, h);
}