#ifndef CAMERA_H
#define CAMERA_H

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

// Defines several possible options for camera movement. Used as abstraction to stay away from window-system specific input methods
enum Camera_Movement {
    FORWARD,
    BACKWARD,
    LEFT,
    RIGHT,
    NONE // We removed UP, jumping is separate
};

// Default camera values
const float YAW = -90.0f;
const float PITCH = 20.0f; // Look down slightly
const float SENSITIVITY = 0.1f;
const float ZOOM = 45.0f;
const float MAX_PITCH_ANGLE = 45.0f; // Your value from main.cpp
const float MIN_PITCH_ANGLE = 5.0f;  // Don't let camera go below player

class Camera
{
public:
    // Camera Attributes
    glm::vec3 Position;
    glm::vec3 Front;
    glm::vec3 Up;
    glm::vec3 Right;
    glm::vec3 WorldUp;

    // Euler Angles
    float Yaw;
    float Pitch;

    // Camera options
    float MouseSensitivity;
    float Zoom;
    float TargetDistance; // The desired distance from the player

    // Constructor
    Camera(float targetDist = 3.0f, glm::vec3 up = glm::vec3(0.0f, 1.0f, 0.0f), float yaw = YAW, float pitch = PITCH)
        : Front(glm::vec3(0.0f, 0.0f, -1.0f)), MouseSensitivity(SENSITIVITY), Zoom(ZOOM),
        WorldUp(up), Yaw(yaw), Pitch(pitch), TargetDistance(targetDist)
    {
        Position = glm::vec3(0.0f); // Will be set by Update
        updateCameraVectors(); // Initial calculation
    }

    // Returns the view matrix
    glm::mat4 GetViewMatrix()
    {
        // Camera looks at its own position + its Front vector
        // The *Position* is what we move around the player
        return glm::lookAt(Position, Position + Front, Up);
    }

    // Call this in your game loop *after* camera collision
    void SetFinalPosition(glm::vec3 pos, glm::vec3 target)
    {
        Position = pos;
        // Look at the player
        Front = glm::normalize(target - Position);
        Right = glm::normalize(glm::cross(Front, WorldUp));
        Up = glm::normalize(glm::cross(Right, Front));
    }

    // Processes mouse movement to orbit the camera
    void ProcessMouseMovement(float xoffset, float yoffset)
    {
        xoffset *= MouseSensitivity;
        yoffset *= MouseSensitivity;

        Yaw += xoffset;
        Pitch += yoffset; // Normal (non-orbit) controls

        // Constrain pitch
        if (Pitch > MAX_PITCH_ANGLE)
            Pitch = MAX_PITCH_ANGLE;
        if (Pitch < -89.0f) // Allow looking up
            Pitch = -89.0f;

        updateCameraVectors();
    }

    // Processes mouse scroll to zoom in/out
    void ProcessMouseScroll(float yoffset)
    {
        TargetDistance -= (float)yoffset;
        if (TargetDistance < 1.0f)
            TargetDistance = 1.0f;
        if (TargetDistance > 10.0f)
            TargetDistance = 10.0f;
    }

    // calculates the front vector from the Camera's (updated) Euler Angles
    void updateCameraVectors()
    {
        // calculate the new Front vector
        glm::vec3 front;
        front.x = cos(glm::radians(Yaw)) * cos(glm::radians(Pitch));
        front.y = sin(glm::radians(Pitch));
        front.z = sin(glm::radians(Yaw)) * cos(glm::radians(Pitch));
        Front = glm::normalize(front);
        // also re-calculate the Right and Up vector
        Right = glm::normalize(glm::cross(Front, WorldUp));
        Up = glm::normalize(glm::cross(Right, Front));
    }
};
#endif