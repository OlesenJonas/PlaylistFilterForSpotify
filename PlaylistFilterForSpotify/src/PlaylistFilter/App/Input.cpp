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

        Renderer::Ray mouseRay = renderer->getMouseRay();
        // everything gets remapped [0,1] -> [-1,1] in VS, undo that for the ray here
        mouseRay.origin = 0.5f * mouseRay.origin + 0.5f;
        Track* hitTrack = app.raycastAgainstGraphingBuffer(mouseRay.origin, mouseRay.direction);
        // todo: only set if something was hit?
        app.setSelectedTrack(hitTrack);
    }
}

void keyCallback(GLFWwindow* window, int key, int scancode, int action, int mods)
{
    auto* renderer = reinterpret_cast<Renderer*>(glfwGetWindowUserPointer(window));
    App& app = renderer->app;
    if(key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GL_TRUE);
    if(key == GLFW_KEY_TAB && action == GLFW_PRESS)
        app.toggleWindowVisibility();
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