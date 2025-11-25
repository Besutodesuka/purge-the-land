#version 330 core
out vec4 FragColor;

uniform vec4 color; // Changed from vec3 to vec4 to support Alpha

void main() {
    FragColor = color; 
}