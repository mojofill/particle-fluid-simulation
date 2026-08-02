#ifndef CAMERA_H
#define CAMERA_H

#include "cglm/cglm.h"
#include <GLFW/glfw3.h>

#define moveSpeed 0.5f
#define rotSpeed 0.025f
#define sensitivity 0.002f

typedef struct Camera {
    vec3 position;
    float padA;
    vec3 forward;
    float padB;
    vec3 right;
    float padC;
    vec3 up;
    float padD;

    // these will only be used on cpu side to compute camera
    float yaw;
    float pitch;
    float lastX;
    float lastY;
} Camera;

void cameraProcessInputs(Camera *cam, GLFWwindow *window, float speedScale);

#endif