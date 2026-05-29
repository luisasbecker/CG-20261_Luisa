#ifndef CAMERA_H
#define CAMERA_H

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <string>

class Camera
{
public:
    // Vetores de estado da camera
    glm::vec3 position;
    glm::vec3 front;
    glm::vec3 up;
    glm::vec3 right;
    glm::vec3 worldUp;

    // Angulos de Euler para rotacao
    float yaw;
    float pitch;

    // Configuracoes de movimentacao
    float movementSpeed;
    float mouseSensitivity;

    // Construtor com valores padrao, olhando para a origem a partir do eixo Z negativo.
    Camera(glm::vec3 startPos = glm::vec3(0.0f, 0.0f, -3.0f),
           glm::vec3 startUp = glm::vec3(0.0f, 1.0f, 0.0f),
           float startYaw = 90.0f,
           float startPitch = 0.0f);

    // Retorna a matriz de View calculada
    glm::mat4 getViewMatrix();

    // Processa entrada de teclado (WASD)
    void processKeyboard(const std::string& direction, float deltaTime);

    // Processa movimento do mouse (mantido para compatibilidade com os exemplos da disciplina)
    void processMouseMovement(float xoffset, float yoffset, bool constrainPitch = true);

private:
    // Atualiza Front, Right e Up com base nos angulos atuais
    void updateCameraVectors();
};

#endif