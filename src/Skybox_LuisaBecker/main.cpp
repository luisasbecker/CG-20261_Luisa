/* 
 * Exercício Skybox 
 * Luisa Becker dos Santos
 */

#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <vector>

using namespace std;

// GLAD
#include <glad/glad.h>

// GLFW
#include <GLFW/glfw3.h>

// GLM
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

// STB_IMAGE
#define STB_IMAGE_IMPLEMENTATION
#include <stb_image.h>

// Camera
#include "Camera.h"

// Protótipos
void key_callback(GLFWwindow *window, int key, int scancode, int action, int mode);

int setupShader(const GLchar *vshader, const GLchar *fshader);

GLuint loadSimpleOBJ(string filePATH, int &nVertices);
GLuint loadTexture(string filePath, int &imgWidth, int &imgHeight);
GLuint loadCubemap(vector<string> faces);
GLuint setupCubemap(float scaleFactor);

// Dimensões da janela
const GLuint WIDTH = 600;
const GLuint HEIGHT = 600;

// Vertex Shader do objeto
const GLchar *vertexShaderSource = R"glsl(
#version 450

layout (location = 0) in vec3 position;
layout (location = 1) in vec3 color;
layout (location = 2) in vec3 normal;
layout (location = 3) in vec2 texc;

uniform mat4 model;
uniform mat4 projection;
uniform mat4 view;

out vec4 finalColor;
out vec3 fragPos;
out vec3 scaledNormal;
out vec2 texcoord;

void main()
{
    gl_Position = projection * view * model * vec4(position, 1.0);

    finalColor = vec4(color, 1.0);
    fragPos = vec3(model * vec4(position, 1.0));

    mat3 normalMatrix = mat3(transpose(inverse(model)));
    scaledNormal = normalMatrix * normal;

    texcoord = vec2(texc.s, 1.0 - texc.t);
}
)glsl";

// Fragment Shader do objeto: Phong + reflexão da skybox
const GLchar *fragmentShaderSource = R"glsl(
#version 450

in vec4 finalColor;
in vec3 fragPos;
in vec3 scaledNormal;
in vec2 texcoord;

uniform sampler2D texBuffer;
uniform samplerCube skybox;

// Propriedades da superfície/material
uniform float ka;
uniform float kd;
uniform float ks;
uniform float q;
uniform float reflectivity;

// Propriedades da fonte de luz
uniform vec3 lightPos;
uniform vec3 lightColor;

// Posição da câmera
uniform vec3 cameraPos;

out vec4 color;

void main()
{
    vec3 N = normalize(scaledNormal);
    vec3 L = normalize(lightPos - fragPos);
    vec3 V = normalize(cameraPos - fragPos);

    // Textura base do objeto
    vec4 texColor = texture(texBuffer, texcoord);

    // Phong: ambiente
    vec3 ambient = ka * lightColor;

    // Phong: difusa
    float diff = max(dot(N, L), 0.0);
    vec3 diffuse = kd * diff * lightColor;

    // Phong: especular
    vec3 Rspec = reflect(-L, N);
    float spec = pow(max(dot(V, Rspec), 0.0), q);
    vec3 specular = ks * spec * lightColor;

    vec3 phongColor = (ambient + diffuse + specular) * texColor.rgb;

    // Reflexão de ambiente usando a skybox
    // I aponta da câmera para o fragmento.
    vec3 I = normalize(fragPos - cameraPos);
    vec3 R = reflect(I, N);
    vec3 skyColor = texture(skybox, R).rgb;

    float fatorReflexao = clamp(reflectivity, 0.0, 1.0);
    vec3 mixedColor = mix(phongColor, skyColor, fatorReflexao);

    color = vec4(mixedColor, texColor.a);
}
)glsl";

// Vertex Shader da skybox
const GLchar *vShaderSkybox = R"glsl(
#version 450

layout (location = 0) in vec3 aPos;

out vec3 TexCoords;

uniform mat4 projection;
uniform mat4 view;

void main()
{
    TexCoords = aPos;

    // Remove a translação da view para que a skybox pareça infinitamente distante.
    mat4 viewNoTranslation = mat4(mat3(view));

    vec4 pos = projection * viewNoTranslation * vec4(aPos, 1.0);

    // Mantém a skybox no fundo do depth buffer.
    gl_Position = pos.xyww;
}
)glsl";

// Fragment Shader da skybox
const GLchar *fShaderSkybox = R"glsl(
#version 450

in vec3 TexCoords;

uniform samplerCube skybox;

out vec4 FragColor;

void main()
{
    FragColor = texture(skybox, TexCoords);
}
)glsl";

bool perspective = true;

// Ângulos acumulados de rotação do objeto.
// A rotação só aumenta enquanto a tecla do eixo estiver pressionada.
float objectRotationX = 0.0f;
float objectRotationY = 0.0f;
float objectRotationZ = 0.0f;

// Velocidade da rotação em graus por segundo
const float OBJECT_ROTATION_SPEED = 90.0f;

// Câmera
Camera camera(glm::vec3(0.0f, 0.0f, -3.0f),
              glm::vec3(0.0f, 1.0f, 0.0f),
              90.0f,
              0.0f);

float deltaTime = 0.0f;
float lastFrame = 0.0f;

// Fator de mistura entre Phong e reflexão da skybox.
// 0.0 = somente Phong
// 1.0 = somente reflexão da skybox
float reflectivity = 0.55f;

struct Mesh
{
    GLuint VAO;
    int nVertices;
    GLuint texID;
};

int main()
{
    // Inicialização da GLFW
    glfwInit();

    GLFWwindow *window = glfwCreateWindow(WIDTH, HEIGHT, "Skybox - Luisa Becker", nullptr, nullptr);

    if (window == nullptr)
    {
        cout << "Falha ao criar a janela GLFW" << endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetKeyCallback(window, key_callback);

    // GLAD
    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        cout << "Failed to initialize GLAD" << endl;
        glfwTerminate();
        return -1;
    }

    // Informações de versão
    const GLubyte *renderer = glGetString(GL_RENDERER);
    const GLubyte *version = glGetString(GL_VERSION);

    cout << "Renderer: " << renderer << endl;
    cout << "OpenGL version supported: " << version << endl;

    int width;
    int height;
    glfwGetFramebufferSize(window, &width, &height);
    glViewport(0, 0, width, height);

    glEnable(GL_DEPTH_TEST);

    // Shaders
    GLuint shaderID = setupShader(vertexShaderSource, fragmentShaderSource);
    GLuint skyboxShaderID = setupShader(vShaderSkybox, fShaderSkybox);

    // Matriz de projeção inicial
    glm::mat4 projection = glm::perspective(glm::radians(45.0f),
                                            (float)WIDTH / (float)HEIGHT,
                                            0.1f,
                                            100.0f);

    // Matriz de modelo inicial
    glm::mat4 model = glm::mat4(1.0f);
    model = glm::rotate(model, glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f));

    // Malha do objeto
    Mesh m;
    m.VAO = loadSimpleOBJ("../assets/Modelos3D/SuzanneSubdiv1.obj", m.nVertices);

    if (m.VAO == 0 || m.nVertices == 0)
    {
        cout << "Erro ao carregar o modelo 3D." << endl;
        glfwTerminate();
        return -1;
    }

    // Textura da Suzanne
    int imgWidth;
    int imgHeight;
    m.texID = loadTexture("../src/HelloCubemap/Suzanne.png", imgWidth, imgHeight);

    if (m.texID == 0)
    {
        cout << "Erro ao carregar a textura da Suzanne." << endl;
        glfwTerminate();
        return -1;
    }

    // Cubemap/skybox
    vector<string> faces = {
        "../src/HelloCubemap/skybox/right.jpg",
        "../src/HelloCubemap/skybox/left.jpg",
        "../src/HelloCubemap/skybox/top.jpg",
        "../src/HelloCubemap/skybox/bottom.jpg",
        "../src/HelloCubemap/skybox/front.jpg",
        "../src/HelloCubemap/skybox/back.jpg"};

    GLuint skyboxTexID = loadCubemap(faces);
    GLuint skyboxVAO = setupCubemap(10.0f);

    // Parâmetros de iluminação
    float ka = 0.20f;
    float kd = 0.60f;
    float ks = 0.80f;
    float q = 32.0f;

    glm::vec3 lightPos = glm::vec3(-0.5f, 5.0f, -1.0f);
    glm::vec3 lightColor = glm::vec3(1.0f, 1.0f, 1.0f);

    // Uniforms fixos do shader do objeto
    glUseProgram(shaderID);

    glUniform1i(glGetUniformLocation(shaderID, "texBuffer"), 0);
    glUniform1i(glGetUniformLocation(shaderID, "skybox"), 1);

    glUniform1f(glGetUniformLocation(shaderID, "ka"), ka);
    glUniform1f(glGetUniformLocation(shaderID, "kd"), kd);
    glUniform1f(glGetUniformLocation(shaderID, "ks"), ks);
    glUniform1f(glGetUniformLocation(shaderID, "q"), q);
    glUniform1f(glGetUniformLocation(shaderID, "reflectivity"), reflectivity);

    glUniform3f(glGetUniformLocation(shaderID, "lightPos"), lightPos.x, lightPos.y, lightPos.z);
    glUniform3f(glGetUniformLocation(shaderID, "lightColor"), lightColor.x, lightColor.y, lightColor.z);

    // Uniforms fixos do shader da skybox
    glUseProgram(skyboxShaderID);
    glUniform1i(glGetUniformLocation(skyboxShaderID, "skybox"), 0);

    // Loop principal
    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        glfwPollEvents();

        // Movimento da câmera
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
        {
            camera.processKeyboard("FORWARD", deltaTime);
        }

        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
        {
            camera.processKeyboard("BACKWARD", deltaTime);
        }

        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
        {
            camera.processKeyboard("LEFT", deltaTime);
        }

        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
        {
            camera.processKeyboard("RIGHT", deltaTime);
        }

        // Rotação do objeto 
        float rotationStep = glm::radians(OBJECT_ROTATION_SPEED) * deltaTime;

        if (glfwGetKey(window, GLFW_KEY_X) == GLFW_PRESS)
        {
            objectRotationX += rotationStep;
        }

        if (glfwGetKey(window, GLFW_KEY_Y) == GLFW_PRESS)
        {
            objectRotationY += rotationStep;
        }

        if (glfwGetKey(window, GLFW_KEY_Z) == GLFW_PRESS)
        {
            objectRotationZ += rotationStep;
        }

        // Limpeza dos buffers
        glClearColor(0.2f, 0.2f, 0.2f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        // Atualização da projeção
        if (perspective)
        {
            projection = glm::perspective(glm::radians(45.0f),
                                          (float)WIDTH / (float)HEIGHT,
                                          0.1f,
                                          100.0f);
        }
        else
        {
            projection = glm::ortho(-3.0f, 3.0f,
                                    -3.0f, 3.0f,
                                    0.1f, 100.0f);
        }

        // Atualização da view
        glm::mat4 view = camera.getViewMatrix();

        // Atualização da model
        model = glm::mat4(1.0f);

        // Rotação inicial para manter o objeto na orientação desejada
        model = glm::rotate(model, glm::radians(180.0f), glm::vec3(0.0f, 1.0f, 0.0f));

        // Rotações acumuladas pelo usuário
        model = glm::rotate(model, objectRotationX, glm::vec3(1.0f, 0.0f, 0.0f));
        model = glm::rotate(model, objectRotationY, glm::vec3(0.0f, 1.0f, 0.0f));
        model = glm::rotate(model, objectRotationZ, glm::vec3(0.0f, 0.0f, 1.0f));

        // ------------------------------------------------------------
        // Desenho do objeto: Phong + reflexão da skybox
        // ------------------------------------------------------------
        glUseProgram(shaderID);

        glUniformMatrix4fv(glGetUniformLocation(shaderID, "model"), 1, GL_FALSE, glm::value_ptr(model));
        glUniformMatrix4fv(glGetUniformLocation(shaderID, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniformMatrix4fv(glGetUniformLocation(shaderID, "view"), 1, GL_FALSE, glm::value_ptr(view));

        glUniform3f(glGetUniformLocation(shaderID, "cameraPos"),
                    camera.position.x,
                    camera.position.y,
                    camera.position.z);

        glUniform1f(glGetUniformLocation(shaderID, "reflectivity"), reflectivity);

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, m.texID);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_CUBE_MAP, skyboxTexID);

        glBindVertexArray(m.VAO);
        glDrawArrays(GL_TRIANGLES, 0, m.nVertices);
        glBindVertexArray(0);

        // ------------------------------------------------------------
        // Desenho da skybox
        // ------------------------------------------------------------
        glDepthFunc(GL_LEQUAL);
        glDepthMask(GL_FALSE);

        glUseProgram(skyboxShaderID);

        glUniformMatrix4fv(glGetUniformLocation(skyboxShaderID, "projection"), 1, GL_FALSE, glm::value_ptr(projection));
        glUniformMatrix4fv(glGetUniformLocation(skyboxShaderID, "view"), 1, GL_FALSE, glm::value_ptr(view));

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, skyboxTexID);

        glBindVertexArray(skyboxVAO);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glBindVertexArray(0);

        glDepthMask(GL_TRUE);
        glDepthFunc(GL_LESS);

        glfwSwapBuffers(window);
    }

    glDeleteVertexArrays(1, &m.VAO);
    glDeleteVertexArrays(1, &skyboxVAO);

    glfwTerminate();

    return 0;
}

void key_callback(GLFWwindow *window, int key, int scancode, int action, int mode)
{
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    {
        glfwSetWindowShouldClose(window, GL_TRUE);
    }

    if (key == GLFW_KEY_P && action == GLFW_PRESS)
    {
        perspective = !perspective;
    }

    if (key == GLFW_KEY_UP && action == GLFW_PRESS)
    {
        reflectivity += 0.05f;

        if (reflectivity > 1.0f)
        {
            reflectivity = 1.0f;
        }

        cout << "Reflectivity: " << reflectivity << endl;
    }

    if (key == GLFW_KEY_DOWN && action == GLFW_PRESS)
    {
        reflectivity -= 0.05f;

        if (reflectivity < 0.0f)
        {
            reflectivity = 0.0f;
        }

        cout << "Reflectivity: " << reflectivity << endl;
    }
}

int setupShader(const GLchar *vshader, const GLchar *fshader)
{
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vshader, NULL);
    glCompileShader(vertexShader);

    GLint success;
    GLchar infoLog[512];

    glGetShaderiv(vertexShader, GL_COMPILE_STATUS, &success);

    if (!success)
    {
        glGetShaderInfoLog(vertexShader, 512, NULL, infoLog);
        cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n"
             << infoLog << endl;
    }

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fshader, NULL);
    glCompileShader(fragmentShader);

    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);

    if (!success)
    {
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n"
             << infoLog << endl;
    }

    GLuint shaderProgram = glCreateProgram();

    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);

    if (!success)
    {
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n"
             << infoLog << endl;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return shaderProgram;
}

GLuint loadSimpleOBJ(string filePATH, int &nVertices)
{
    vector<glm::vec3> vertices;
    vector<glm::vec2> texCoords;
    vector<glm::vec3> normals;
    vector<GLfloat> vBuffer;

    glm::vec3 color = glm::vec3(1.0f, 1.0f, 1.0f);

    ifstream arqEntrada(filePATH.c_str());

    if (!arqEntrada.is_open())
    {
        cerr << "Erro ao tentar ler o arquivo " << filePATH << endl;
        nVertices = 0;
        return 0;
    }

    string line;

    while (getline(arqEntrada, line))
    {
        istringstream ssline(line);
        string word;

        ssline >> word;

        if (word == "v")
        {
            glm::vec3 vertice;
            ssline >> vertice.x >> vertice.y >> vertice.z;
            vertices.push_back(vertice);
        }
        else if (word == "vt")
        {
            glm::vec2 vt;
            ssline >> vt.s >> vt.t;
            texCoords.push_back(vt);
        }
        else if (word == "vn")
        {
            glm::vec3 normal;
            ssline >> normal.x >> normal.y >> normal.z;
            normals.push_back(normal);
        }
        else if (word == "f")
        {
            while (ssline >> word)
            {
                int vi = 0;
                int ti = 0;
                int ni = 0;

                istringstream ss(word);
                string index;

                if (getline(ss, index, '/'))
                {
                    vi = !index.empty() ? stoi(index) - 1 : 0;
                }

                if (getline(ss, index, '/'))
                {
                    ti = !index.empty() ? stoi(index) - 1 : 0;
                }

                if (getline(ss, index))
                {
                    ni = !index.empty() ? stoi(index) - 1 : 0;
                }

                if (vi < 0 || vi >= (int)vertices.size())
                {
                    continue;
                }

                glm::vec3 vertex = vertices[vi];
                glm::vec2 texCoord = glm::vec2(0.0f, 0.0f);
                glm::vec3 normal = glm::vec3(0.0f, 1.0f, 0.0f);

                if (ti >= 0 && ti < (int)texCoords.size())
                {
                    texCoord = texCoords[ti];
                }

                if (ni >= 0 && ni < (int)normals.size())
                {
                    normal = normals[ni];
                }

                vBuffer.push_back(vertex.x);
                vBuffer.push_back(vertex.y);
                vBuffer.push_back(vertex.z);

                vBuffer.push_back(color.r);
                vBuffer.push_back(color.g);
                vBuffer.push_back(color.b);

                vBuffer.push_back(normal.x);
                vBuffer.push_back(normal.y);
                vBuffer.push_back(normal.z);

                vBuffer.push_back(texCoord.s);
                vBuffer.push_back(texCoord.t);
            }
        }
    }

    arqEntrada.close();

    cout << "Gerando o buffer de geometria..." << endl;

    GLuint VBO;
    GLuint VAO;

    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);
    glBufferData(GL_ARRAY_BUFFER, vBuffer.size() * sizeof(GLfloat), vBuffer.data(), GL_STATIC_DRAW);

    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);

    // Atributo 0: posição
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(GLfloat), (GLvoid *)0);
    glEnableVertexAttribArray(0);

    // Atributo 1: cor
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(GLfloat), (GLvoid *)(3 * sizeof(GLfloat)));
    glEnableVertexAttribArray(1);

    // Atributo 2: normal
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, 11 * sizeof(GLfloat), (GLvoid *)(6 * sizeof(GLfloat)));
    glEnableVertexAttribArray(2);

    // Atributo 3: coordenada de textura
    glVertexAttribPointer(3, 2, GL_FLOAT, GL_FALSE, 11 * sizeof(GLfloat), (GLvoid *)(9 * sizeof(GLfloat)));
    glEnableVertexAttribArray(3);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    nVertices = vBuffer.size() / 11;

    return VAO;
}

GLuint loadTexture(string filePath, int &imgWidth, int &imgHeight)
{
    GLuint texID;

    glGenTextures(1, &texID);
    glBindTexture(GL_TEXTURE_2D, texID);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR_MIPMAP_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    int width;
    int height;
    int nrChannels;

    unsigned char *data = stbi_load(filePath.c_str(), &width, &height, &nrChannels, 0);

    if (data)
    {
        GLenum format = GL_RGB;

        if (nrChannels == 1)
        {
            format = GL_RED;
        }
        else if (nrChannels == 3)
        {
            format = GL_RGB;
        }
        else if (nrChannels == 4)
        {
            format = GL_RGBA;
        }

        glTexImage2D(GL_TEXTURE_2D,
                     0,
                     format,
                     width,
                     height,
                     0,
                     format,
                     GL_UNSIGNED_BYTE,
                     data);

        glGenerateMipmap(GL_TEXTURE_2D);

        imgWidth = width;
        imgHeight = height;

        stbi_image_free(data);
        glBindTexture(GL_TEXTURE_2D, 0);

        return texID;
    }

    cout << "Failed to load texture: " << filePath << endl;

    stbi_image_free(data);

    return 0;
}

GLuint loadCubemap(vector<string> faces)
{
    GLuint textureID;

    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

    int width;
    int height;
    int nrChannels;

    for (unsigned int i = 0; i < faces.size(); i++)
    {
        unsigned char *data = stbi_load(faces[i].c_str(), &width, &height, &nrChannels, 0);

        if (data)
        {
            GLenum format = GL_RGB;

            if (nrChannels == 1)
            {
                format = GL_RED;
            }
            else if (nrChannels == 3)
            {
                format = GL_RGB;
            }
            else if (nrChannels == 4)
            {
                format = GL_RGBA;
            }

            glTexImage2D(GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
                         0,
                         format,
                         width,
                         height,
                         0,
                         format,
                         GL_UNSIGNED_BYTE,
                         data);

            stbi_image_free(data);
        }
        else
        {
            cout << "Cubemap texture failed to load at path: " << faces[i] << endl;
            stbi_image_free(data);
        }
    }

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_CUBE_MAP, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE);

    return textureID;
}

GLuint setupCubemap(float scaleFactor)
{
    float skyboxVertices[] = {
        // positions
        scaleFactor * -0.5f, scaleFactor * 0.5f, scaleFactor * -0.5f,
        scaleFactor * -0.5f, scaleFactor * -0.5f, scaleFactor * -0.5f,
        scaleFactor * 0.5f, scaleFactor * -0.5f, scaleFactor * -0.5f,
        scaleFactor * 0.5f, scaleFactor * -0.5f, scaleFactor * -0.5f,
        scaleFactor * 0.5f, scaleFactor * 0.5f, scaleFactor * -0.5f,
        scaleFactor * -0.5f, scaleFactor * 0.5f, scaleFactor * -0.5f,

        scaleFactor * -0.5f, scaleFactor * -0.5f, scaleFactor * 0.5f,
        scaleFactor * -0.5f, scaleFactor * -0.5f, scaleFactor * -0.5f,
        scaleFactor * -0.5f, scaleFactor * 0.5f, scaleFactor * -0.5f,
        scaleFactor * -0.5f, scaleFactor * 0.5f, scaleFactor * -0.5f,
        scaleFactor * -0.5f, scaleFactor * 0.5f, scaleFactor * 0.5f,
        scaleFactor * -0.5f, scaleFactor * -0.5f, scaleFactor * 0.5f,

        scaleFactor * 0.5f, scaleFactor * -0.5f, scaleFactor * -0.5f,
        scaleFactor * 0.5f, scaleFactor * -0.5f, scaleFactor * 0.5f,
        scaleFactor * 0.5f, scaleFactor * 0.5f, scaleFactor * 0.5f,
        scaleFactor * 0.5f, scaleFactor * 0.5f, scaleFactor * 0.5f,
        scaleFactor * 0.5f, scaleFactor * 0.5f, scaleFactor * -0.5f,
        scaleFactor * 0.5f, scaleFactor * -0.5f, scaleFactor * -0.5f,

        scaleFactor * -0.5f, scaleFactor * -0.5f, scaleFactor * 0.5f,
        scaleFactor * -0.5f, scaleFactor * 0.5f, scaleFactor * 0.5f,
        scaleFactor * 0.5f, scaleFactor * 0.5f, scaleFactor * 0.5f,
        scaleFactor * 0.5f, scaleFactor * 0.5f, scaleFactor * 0.5f,
        scaleFactor * 0.5f, scaleFactor * -0.5f, scaleFactor * 0.5f,
        scaleFactor * -0.5f, scaleFactor * -0.5f, scaleFactor * 0.5f,

        scaleFactor * -0.5f, scaleFactor * 0.5f, scaleFactor * -0.5f,
        scaleFactor * 0.5f, scaleFactor * 0.5f, scaleFactor * -0.5f,
        scaleFactor * 0.5f, scaleFactor * 0.5f, scaleFactor * 0.5f,
        scaleFactor * 0.5f, scaleFactor * 0.5f, scaleFactor * 0.5f,
        scaleFactor * -0.5f, scaleFactor * 0.5f, scaleFactor * 0.5f,
        scaleFactor * -0.5f, scaleFactor * 0.5f, scaleFactor * -0.5f,

        scaleFactor * -0.5f, scaleFactor * -0.5f, scaleFactor * -0.5f,
        scaleFactor * -0.5f, scaleFactor * -0.5f, scaleFactor * 0.5f,
        scaleFactor * 0.5f, scaleFactor * -0.5f, scaleFactor * -0.5f,
        scaleFactor * 0.5f, scaleFactor * -0.5f, scaleFactor * -0.5f,
        scaleFactor * -0.5f, scaleFactor * -0.5f, scaleFactor * 0.5f,
        scaleFactor * 0.5f, scaleFactor * -0.5f, scaleFactor * 0.5f};

    GLuint skyboxVAO;
    GLuint skyboxVBO;

    glGenVertexArrays(1, &skyboxVAO);
    glGenBuffers(1, &skyboxVBO);

    glBindVertexArray(skyboxVAO);

    glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);
    glBufferData(GL_ARRAY_BUFFER, sizeof(skyboxVertices), skyboxVertices, GL_STATIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), (void *)0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    return skyboxVAO;
}