#include "camera.h"

void camera_move(Camera *cam, vec3 dir, float dt) {
    vec3 scaledDir;
    glm_vec3_scale(dir, dt, scaledDir);
    glm_vec3_add(cam->position, scaledDir, cam->position);
}

void cameraProcessInputs(Camera *cam, GLFWwindow *window, float speedScale) {
    double xpos;
    double ypos;
    glfwGetCursorPos(window, &xpos, &ypos);
    
    float xoffset = -(float)(xpos - cam->lastX);
    float yoffset = (float)(cam->lastY - ypos);

    cam->lastX = xpos;
    cam->lastY = ypos;

    xoffset *= sensitivity;
    yoffset *= sensitivity;

    // Update camera yaw and pitch
    cam->yaw += xoffset;
    cam->pitch += yoffset;

    // Clamp pitch
    float limit = GLM_PI_2 - 0.001f;
    cam->pitch = glm_clamp(cam->pitch, -limit, limit);

    // Recalculate direction
    // recompute forward
    cam->forward[0] = cosf(cam->pitch) * cosf(cam->yaw);
    cam->forward[1] = cosf(cam->pitch) * sinf(cam->yaw);
    cam->forward[2] = sinf(cam->pitch);

    glm_normalize(cam->forward);

    // essentially this
    // camera.right = normalize(cross(camera->forward, (vec3) {0, 0, 1}));
    glm_vec3_cross(cam->forward, (vec3) {0, 0, 1}, cam->right);
    glm_normalize(cam->right);

    // camera.up = cross(camera.right, camera.forward);
    // glm_vec3_cross(cam->right, cam->forward, cam->up);
    // glm_normalize(cam->up);

    // camera.c — replace the right/up computation:
    cam->right[0] = sinf(cam->yaw);
    cam->right[1] = -cosf(cam->yaw);
    cam->right[2] = 0.0f;

    glm_vec3_cross(cam->right, cam->forward, cam->up);
    glm_normalize(cam->up);

    // Build flat forward (for WASD move)

    vec3 flatDir = { cam->forward[0], cam->forward[1], 0 };
    glm_normalize(flatDir);

    // Right = cross(flatDir, up)
    vec3 up = {0, 0, 1};
    vec3 right;
    glm_vec3_cross(flatDir, up, right);
    glm_normalize(right);

    // --- MOVEMENT ---
    if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        camera_move(cam, flatDir, moveSpeed * speedScale);

    if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS) {
        vec3 back;
        glm_vec3_scale(flatDir, -1.0f, back);
        camera_move(cam, back, moveSpeed * speedScale);
    }

    if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS) {
        vec3 left;
        glm_vec3_scale(right, -1.0f, left);
        camera_move(cam, left, moveSpeed * speedScale);
    }

    if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        camera_move(cam, right, moveSpeed * speedScale);

    if (glfwGetKey(window, GLFW_KEY_SPACE) == GLFW_PRESS)
        camera_move(cam, up, moveSpeed * speedScale);

    if (glfwGetKey(window, GLFW_KEY_LEFT_SHIFT) == GLFW_PRESS ||
        glfwGetKey(window, GLFW_KEY_RIGHT_SHIFT) == GLFW_PRESS)
    {
        vec3 down = {0, 0, -1};
        camera_move(cam, down, moveSpeed * speedScale);
    }
}
