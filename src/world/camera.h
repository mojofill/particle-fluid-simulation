#ifndef CAMERA_H
#define CAMERA_H

#include "cglm/cglm.h"
#include <GLFW/glfw3.h>

#define moveSpeed 0.5f
#define rotSpeed 0.025f
#define sensitivity 0.002f

typedef struct Camera {
    vec3 pos;
    vec3 dir;
    float pitch;
    float yaw;
    float lastX;
    float lastY;
} Camera;

void cameraProcessInputs(Camera *cam, GLFWwindow *window);

#endif