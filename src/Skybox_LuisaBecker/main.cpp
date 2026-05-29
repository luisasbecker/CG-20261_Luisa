/*
 * Exercicio Grau B - Environment Mapping com Cube Mapping
 * Luisa Becker
 *
 * Baseado no exemplo HelloCubemap do repositorio CG-20261.
 *
 * Alteracoes principais:
 * - Skybox renderizado como fundo da cena usando o cubemap de src/HelloCubemap/skybox.
 * - Objeto com reflexao de ambiente usando reflect(I, N).
 * - Mistura entre Phong e Cubemap usando mix(corPhong, corSkybox, reflectivity).
 * - W/A/S/D movem a camera.
 * - Setas rotacionam o objeto apenas enquanto pressionadas.
 */

#include <algorithm>
#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
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

// Prototipos
void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode);
GLuint setupShader(const GLchar* vshader, const GLchar* fshader);
GLuint loadSimpleOBJ(string filePath, int& nVertices);
GLuint loadTexture(string filePath, int& imgWidth, int& imgHeight);
GLuint loadCubemap(vector<string> faces);
GLuint setupCubemap(float scaleFactor);

bool fileExists(const string& filePath);
string findResource(const string& relativePath);

// Dimensoes da janela
const GLuint WIDTH = 600, HEIGHT = 600;

// Vertex Shader do objeto
const GLchar* vertexShaderSource = R"glsl(
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
out vec3 worldNormal;
out vec2 texcoord;

void main()
{
    vec4 worldPosition = model * vec4(position, 1.0);

    gl_Position = projection * view * worldPosition;

    finalColor = vec4(color, 1.0);
    fragPos = vec3(worldPosition);

    // Transformacao correta da normal em coordenadas de mundo.
    worldNormal = mat3(transpose(inverse(model))) * normal;

    texcoord = vec2(texc.s, 1.0 - texc.t);
}
)glsl";

// Fragment Shader do objeto
const GLchar* fragmentShaderSource = R"glsl(
#version 450

in vec4 finalColor;
in vec3 fragPos;
in vec3 worldNormal;
in vec2 texcoord;

uniform sampler2D texBuffer;
uniform samplerCube skybox;

// Propriedades do material
uniform float ka;
uniform float kd;
uniform float ks;
uniform float q;
uniform float reflectivity;

// Luz
uniform vec3 lightPos;
uniform vec3 lightColor;

// Camera
uniform vec3 cameraPos;

out vec4 color;

void main()
{
    vec3 N = normalize(worldNormal);
    vec3 L = normalize(lightPos - fragPos);
    vec3 V = normalize(cameraPos - fragPos);

    // Iluminacao local Phong
    vec3 ambient = ka * lightColor;

    float diff = max(dot(N, L), 0.0);
    vec3 diffuse = kd * diff * lightColor;

    vec3 Rlocal = reflect(-L, N);
    float spec = 0.0;

    if (diff > 0.0)
    {
        spec = pow(max(dot(V, Rlocal), 0.0), q);
    }

    vec3 specular = ks * spec * lightColor;

    vec4 texColor = texture(texBuffer, texcoord) * finalColor;
    vec3 corPhong = (ambient + diffuse) * texColor.rgb + specular;

    // Reflexao de ambiente:
    // I aponta da camera para o fragmento.
    // R e o vetor refletido usado para amostrar o samplerCube.
    vec3 I = normalize(fragPos - cameraPos);
    vec3 R = reflect(I, N);
    vec3 corSkybox = texture(skybox, R).rgb;

    float fator = clamp(reflectivity, 0.0, 1.0);

    vec3 corFinal = mix(corPhong, corSkybox, fator);

    color = vec4(corFinal, texColor.a);
}
)glsl";

// Vertex Shader do Skybox
const GLchar* vShaderSkybox = R"glsl(
#version 450

layout (location = 0) in vec3 aPos;

out vec3 TexCoords;

uniform mat4 projection;
uniform mat4 view;

void main()
{
    TexCoords = aPos;

    vec4 pos = projection * view * vec4(aPos, 1.0);

    // Mantem o skybox no fundo da cena.
    gl_Position = pos.xyww;
}
)glsl";

// Fragment Shader do Skybox
const GLchar* fShaderSkybox = R"glsl(
#version 450

out vec4 FragColor;

in vec3 TexCoords;

uniform samplerCube skybox;

void main()
{
    FragColor = texture(skybox, TexCoords);
}
)glsl";

// Estado global
bool perspective = true;

// Refletividade inicial menor para nao deixar a cena escura demais.
// Pode ser ajustada com [ ] em tempo de execucao.
float reflectivity = 0.35f;

// Rotacao acumulada do objeto.
// As setas alteram estes valores apenas enquanto pressionadas.
float rotationX = 0.0f;
float rotationY = 0.0f;

// Camera
Camera camera(
    glm::vec3(0.0f, 0.0f, -3.0f),
    glm::vec3(0.0f, 1.0f, 0.0f),
    90.0f,
    0.0f
);

float deltaTime = 0.0f;
float lastFrame = 0.0f;

struct Mesh
{
    GLuint VAO;
    int nVertices;
    GLuint texID;
};

int main()
{
    glfwInit();

    // Mantenha estas linhas comentadas se seu ambiente nao aceitar OpenGL 4.5 diretamente.
    // glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    // glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    // glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

#ifdef __APPLE__
    glfwWindowHint(GLFW_OPENGL_FORWARD_COMPAT, GL_TRUE);
#endif

    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Skybox - Luisa Becker", nullptr, nullptr);

    if (!window)
    {
        cout << "Falha ao criar janela GLFW" << endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetKeyCallback(window, key_callback);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        cout << "Failed to initialize GLAD" << endl;
        return -1;
    }

    const GLubyte* renderer = glGetString(GL_RENDERER);
    const GLubyte* version = glGetString(GL_VERSION);

    cout << "Renderer: " << renderer << endl;
    cout << "OpenGL version supported: " << version << endl;

    int width, height;
    glfwGetFramebufferSize(window, &width, &height);
    glViewport(0, 0, width, height);

    glEnable(GL_DEPTH_TEST);

    GLuint shaderID = setupShader(vertexShaderSource, fragmentShaderSource);
    GLuint skyboxShaderID = setupShader(vShaderSkybox, fShaderSkybox);

    Mesh mesh;

    mesh.VAO = loadSimpleOBJ(
        findResource("assets/Modelos3D/SuzanneSubdiv1.obj"),
        mesh.nVertices
    );

    int imgWidth, imgHeight;

    mesh.texID = loadTexture(
        findResource("src/HelloCubemap/Suzanne.png"),
        imgWidth,
        imgHeight
    );

    vector<string> faces = {
        findResource("src/HelloCubemap/skybox/right.jpg"),
        findResource("src/HelloCubemap/skybox/left.jpg"),
        findResource("src/HelloCubemap/skybox/top.jpg"),
        findResource("src/HelloCubemap/skybox/bottom.jpg"),
        findResource("src/HelloCubemap/skybox/front.jpg"),
        findResource("src/HelloCubemap/skybox/back.jpg")
    };

    GLuint skyboxTexID = loadCubemap(faces);
    GLuint skyboxVAO = setupCubemap(10.0f);

    glm::mat4 model = glm::mat4(1.0f);

    glm::mat4 projection = glm::perspective(
        glm::radians(45.0f),
        (float)WIDTH / (float)HEIGHT,
        0.1f,
        100.0f
    );

    glm::mat4 view = camera.getViewMatrix();

    // Uniforms do objeto
    glUseProgram(shaderID);

    glUniformMatrix4fv(
        glGetUniformLocation(shaderID, "model"),
        1,
        GL_FALSE,
        glm::value_ptr(model)
    );

    glUniformMatrix4fv(
        glGetUniformLocation(shaderID, "projection"),
        1,
        GL_FALSE,
        glm::value_ptr(projection)
    );

    glUniformMatrix4fv(
        glGetUniformLocation(shaderID, "view"),
        1,
        GL_FALSE,
        glm::value_ptr(view)
    );

    // Aumentei ka e kd para corrigir a aparencia escura.
    // O enunciado nao pede uma cena escura; pede mistura entre Phong e reflexao.
    glUniform1f(glGetUniformLocation(shaderID, "ka"), 0.35f);
    glUniform1f(glGetUniformLocation(shaderID, "kd"), 0.75f);
    glUniform1f(glGetUniformLocation(shaderID, "ks"), 0.80f);
    glUniform1f(glGetUniformLocation(shaderID, "q"), 32.0f);
    glUniform1f(glGetUniformLocation(shaderID, "reflectivity"), reflectivity);

    glm::vec3 lightPos = glm::vec3(-0.5f, 5.0f, -1.0f);
    glm::vec3 lightColor = glm::vec3(1.0f, 1.0f, 1.0f);

    glUniform3f(
        glGetUniformLocation(shaderID, "lightPos"),
        lightPos.x,
        lightPos.y,
        lightPos.z
    );

    glUniform3f(
        glGetUniformLocation(shaderID, "lightColor"),
        lightColor.x,
        lightColor.y,
        lightColor.z
    );

    // Unidade 0: textura 2D da Suzanne.
    // Unidade 1: cubemap usado na reflexao do objeto.
    glUniform1i(glGetUniformLocation(shaderID, "texBuffer"), 0);
    glUniform1i(glGetUniformLocation(shaderID, "skybox"), 1);

    // Uniforms do skybox
    glUseProgram(skyboxShaderID);

    glm::mat4 skyboxView = glm::mat4(glm::mat3(view));

    glUniformMatrix4fv(
        glGetUniformLocation(skyboxShaderID, "projection"),
        1,
        GL_FALSE,
        glm::value_ptr(projection)
    );

    glUniformMatrix4fv(
        glGetUniformLocation(skyboxShaderID, "view"),
        1,
        GL_FALSE,
        glm::value_ptr(skyboxView)
    );

    glUniform1i(glGetUniformLocation(skyboxShaderID, "skybox"), 0);

    cout << endl;
    cout << "Controles:" << endl;
    cout << "W/A/S/D  -> move a camera" << endl;
    cout << "Setas    -> rotaciona o objeto enquanto a tecla estiver pressionada" << endl;
    cout << "P        -> alterna perspectiva/ortografica" << endl;
    cout << "[ ou -   -> diminui refletividade" << endl;
    cout << "] ou =   -> aumenta refletividade" << endl;
    cout << "ESC      -> fecha" << endl;
    cout << "Reflectivity inicial: " << reflectivity << endl;
    cout << endl;

    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = (float)glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        glfwPollEvents();

        // Movimento da camera
        if (glfwGetKey(window, GLFW_KEY_W) == GLFW_PRESS)
            camera.processKeyboard("FORWARD", deltaTime);

        if (glfwGetKey(window, GLFW_KEY_S) == GLFW_PRESS)
            camera.processKeyboard("BACKWARD", deltaTime);

        if (glfwGetKey(window, GLFW_KEY_A) == GLFW_PRESS)
            camera.processKeyboard("LEFT", deltaTime);

        if (glfwGetKey(window, GLFW_KEY_D) == GLFW_PRESS)
            camera.processKeyboard("RIGHT", deltaTime);

        // Rotacao do objeto somente enquanto a tecla estiver pressionada
        float rotationSpeed = glm::radians(90.0f);

        if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
            rotationX += rotationSpeed * deltaTime;

        if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
            rotationX -= rotationSpeed * deltaTime;

        if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
            rotationY += rotationSpeed * deltaTime;

        if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
            rotationY -= rotationSpeed * deltaTime;

        if (perspective)
        {
            projection = glm::perspective(
                glm::radians(45.0f),
                (float)WIDTH / (float)HEIGHT,
                0.1f,
                100.0f
            );
        }
        else
        {
            projection = glm::ortho(
                -3.0f,
                3.0f,
                -3.0f,
                3.0f,
                0.1f,
                100.0f
            );
        }

        view = camera.getViewMatrix();

        model = glm::mat4(1.0f);

        // Mantem a Suzanne virada para a camera inicialmente.
        model = glm::rotate(
            model,
            glm::radians(180.0f),
            glm::vec3(0.0f, 1.0f, 0.0f)
        );

        model = glm::rotate(
            model,
            rotationX,
            glm::vec3(1.0f, 0.0f, 0.0f)
        );

        model = glm::rotate(
            model,
            rotationY,
            glm::vec3(0.0f, 1.0f, 0.0f)
        );

        glClearColor(0.15f, 0.15f, 0.18f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glLineWidth(10);
        glPointSize(10);

        // ------------------------------------------------------------
        // 1. Desenha o objeto primeiro
        // ------------------------------------------------------------
        glUseProgram(shaderID);

        glUniformMatrix4fv(
            glGetUniformLocation(shaderID, "model"),
            1,
            GL_FALSE,
            glm::value_ptr(model)
        );

        glUniformMatrix4fv(
            glGetUniformLocation(shaderID, "projection"),
            1,
            GL_FALSE,
            glm::value_ptr(projection)
        );

        glUniformMatrix4fv(
            glGetUniformLocation(shaderID, "view"),
            1,
            GL_FALSE,
            glm::value_ptr(view)
        );

        glUniform3f(
            glGetUniformLocation(shaderID, "cameraPos"),
            camera.position.x,
            camera.position.y,
            camera.position.z
        );

        glUniform1f(
            glGetUniformLocation(shaderID, "reflectivity"),
            reflectivity
        );

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, mesh.texID);

        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_CUBE_MAP, skyboxTexID);

        glBindVertexArray(mesh.VAO);
        glDrawArrays(GL_TRIANGLES, 0, mesh.nVertices);
        glBindVertexArray(0);

        // ------------------------------------------------------------
        // 2. Desenha o skybox por ultimo, como fundo
        // ------------------------------------------------------------
        glDepthFunc(GL_LEQUAL);
        glDepthMask(GL_FALSE);

        glUseProgram(skyboxShaderID);

        skyboxView = glm::mat4(glm::mat3(view));

        glUniformMatrix4fv(
            glGetUniformLocation(skyboxShaderID, "projection"),
            1,
            GL_FALSE,
            glm::value_ptr(projection)
        );

        glUniformMatrix4fv(
            glGetUniformLocation(skyboxShaderID, "view"),
            1,
            GL_FALSE,
            glm::value_ptr(skyboxView)
        );

        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_CUBE_MAP, skyboxTexID);

        glBindVertexArray(skyboxVAO);
        glDrawArrays(GL_TRIANGLES, 0, 36);
        glBindVertexArray(0);

        glDepthMask(GL_TRUE);
        glDepthFunc(GL_LESS);

        glfwSwapBuffers(window);
    }

    glDeleteVertexArrays(1, &mesh.VAO);
    glDeleteVertexArrays(1, &skyboxVAO);
    glDeleteTextures(1, &mesh.texID);
    glDeleteTextures(1, &skyboxTexID);

    glfwTerminate();

    return 0;
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode)
{
    (void)scancode;
    (void)mode;

    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
    {
        glfwSetWindowShouldClose(window, GL_TRUE);
    }

    if (key == GLFW_KEY_P && action == GLFW_PRESS)
    {
        perspective = !perspective;
    }

    if (
        (action == GLFW_PRESS || action == GLFW_REPEAT) &&
        (key == GLFW_KEY_LEFT_BRACKET || key == GLFW_KEY_MINUS)
    )
    {
        reflectivity = std::max(0.0f, reflectivity - 0.05f);
        cout << "Reflectivity: " << reflectivity << endl;
    }

    if (
        (action == GLFW_PRESS || action == GLFW_REPEAT) &&
        (key == GLFW_KEY_RIGHT_BRACKET || key == GLFW_KEY_EQUAL)
    )
    {
        reflectivity = std::min(1.0f, reflectivity + 0.05f);
        cout << "Reflectivity: " << reflectivity << endl;
    }
}

GLuint setupShader(const GLchar* vshader, const GLchar* fshader)
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
        cout << "ERROR::SHADER::VERTEX::COMPILATION_FAILED\n" << infoLog << endl;
    }

    GLuint fragmentShader = glCreateShader(GL_FRAGMENT_SHADER);
    glShaderSource(fragmentShader, 1, &fshader, NULL);
    glCompileShader(fragmentShader);

    glGetShaderiv(fragmentShader, GL_COMPILE_STATUS, &success);

    if (!success)
    {
        glGetShaderInfoLog(fragmentShader, 512, NULL, infoLog);
        cout << "ERROR::SHADER::FRAGMENT::COMPILATION_FAILED\n" << infoLog << endl;
    }

    GLuint shaderProgram = glCreateProgram();

    glAttachShader(shaderProgram, vertexShader);
    glAttachShader(shaderProgram, fragmentShader);
    glLinkProgram(shaderProgram);

    glGetProgramiv(shaderProgram, GL_LINK_STATUS, &success);

    if (!success)
    {
        glGetProgramInfoLog(shaderProgram, 512, NULL, infoLog);
        cout << "ERROR::SHADER::PROGRAM::LINKING_FAILED\n" << infoLog << endl;
    }

    glDeleteShader(vertexShader);
    glDeleteShader(fragmentShader);

    return shaderProgram;
}

struct ObjIndex
{
    int vi = -1;
    int ti = -1;
    int ni = -1;
};

ObjIndex parseObjIndex(const string& token)
{
    ObjIndex index;

    string part;
    stringstream ss(token);

    if (getline(ss, part, '/') && !part.empty())
    {
        index.vi = stoi(part) - 1;
    }

    if (getline(ss, part, '/') && !part.empty())
    {
        index.ti = stoi(part) - 1;
    }

    if (getline(ss, part) && !part.empty())
    {
        index.ni = stoi(part) - 1;
    }

    return index;
}

void appendObjVertex(
    vector<GLfloat>& vBuffer,
    const ObjIndex& objIndex,
    const vector<glm::vec3>& vertices,
    const vector<glm::vec2>& texCoords,
    const vector<glm::vec3>& normals,
    const glm::vec3& fallbackNormal
)
{
    glm::vec3 position = vertices[objIndex.vi];
    glm::vec2 texCoord = glm::vec2(0.0f, 0.0f);
    glm::vec3 normal = fallbackNormal;
    glm::vec3 color = glm::vec3(1.0f, 1.0f, 1.0f);

    if (objIndex.ti >= 0 && objIndex.ti < (int)texCoords.size())
    {
        texCoord = texCoords[objIndex.ti];
    }

    if (objIndex.ni >= 0 && objIndex.ni < (int)normals.size())
    {
        normal = normals[objIndex.ni];
    }

    vBuffer.push_back(position.x);
    vBuffer.push_back(position.y);
    vBuffer.push_back(position.z);

    vBuffer.push_back(color.r);
    vBuffer.push_back(color.g);
    vBuffer.push_back(color.b);

    vBuffer.push_back(normal.x);
    vBuffer.push_back(normal.y);
    vBuffer.push_back(normal.z);

    vBuffer.push_back(texCoord.s);
    vBuffer.push_back(texCoord.t);
}

GLuint loadSimpleOBJ(string filePath, int& nVertices)
{
    vector<glm::vec3> vertices;
    vector<glm::vec2> texCoords;
    vector<glm::vec3> normals;
    vector<GLfloat> vBuffer;

    ifstream inputFile(filePath.c_str());

    if (!inputFile.is_open())
    {
        cerr << "Erro ao tentar ler o arquivo " << filePath << endl;
        nVertices = 0;
        return 0;
    }

    cout << "Carregando OBJ: " << filePath << endl;

    string line;

    while (getline(inputFile, line))
    {
        stringstream ssline(line);
        string word;

        ssline >> word;

        if (word == "v")
        {
            glm::vec3 vertex;
            ssline >> vertex.x >> vertex.y >> vertex.z;
            vertices.push_back(vertex);
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
            vector<ObjIndex> face;
            string token;

            while (ssline >> token)
            {
                face.push_back(parseObjIndex(token));
            }

            if (face.size() < 3)
            {
                continue;
            }

            // Triangulacao em fan para aceitar faces triangulares ou quadrangulares.
            for (size_t i = 1; i + 1 < face.size(); ++i)
            {
                ObjIndex tri[3] = {
                    face[0],
                    face[i],
                    face[i + 1]
                };

                glm::vec3 fallbackNormal = glm::vec3(0.0f, 0.0f, 1.0f);

                if (tri[0].vi >= 0 && tri[1].vi >= 0 && tri[2].vi >= 0)
                {
                    glm::vec3 p0 = vertices[tri[0].vi];
                    glm::vec3 p1 = vertices[tri[1].vi];
                    glm::vec3 p2 = vertices[tri[2].vi];

                    fallbackNormal = glm::normalize(glm::cross(p1 - p0, p2 - p0));
                }

                appendObjVertex(
                    vBuffer,
                    tri[0],
                    vertices,
                    texCoords,
                    normals,
                    fallbackNormal
                );

                appendObjVertex(
                    vBuffer,
                    tri[1],
                    vertices,
                    texCoords,
                    normals,
                    fallbackNormal
                );

                appendObjVertex(
                    vBuffer,
                    tri[2],
                    vertices,
                    texCoords,
                    normals,
                    fallbackNormal
                );
            }
        }
    }

    inputFile.close();

    cout << "Gerando o buffer de geometria..." << endl;

    GLuint VBO;
    GLuint VAO;

    glGenBuffers(1, &VBO);
    glBindBuffer(GL_ARRAY_BUFFER, VBO);

    glBufferData(
        GL_ARRAY_BUFFER,
        vBuffer.size() * sizeof(GLfloat),
        vBuffer.data(),
        GL_STATIC_DRAW
    );

    glGenVertexArrays(1, &VAO);
    glBindVertexArray(VAO);

    // position
    glVertexAttribPointer(
        0,
        3,
        GL_FLOAT,
        GL_FALSE,
        11 * sizeof(GLfloat),
        (GLvoid*)0
    );
    glEnableVertexAttribArray(0);

    // color
    glVertexAttribPointer(
        1,
        3,
        GL_FLOAT,
        GL_FALSE,
        11 * sizeof(GLfloat),
        (GLvoid*)(3 * sizeof(GLfloat))
    );
    glEnableVertexAttribArray(1);

    // normal
    glVertexAttribPointer(
        2,
        3,
        GL_FLOAT,
        GL_FALSE,
        11 * sizeof(GLfloat),
        (GLvoid*)(6 * sizeof(GLfloat))
    );
    glEnableVertexAttribArray(2);

    // texcoord
    glVertexAttribPointer(
        3,
        2,
        GL_FLOAT,
        GL_FALSE,
        11 * sizeof(GLfloat),
        (GLvoid*)(9 * sizeof(GLfloat))
    );
    glEnableVertexAttribArray(3);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    nVertices = (int)vBuffer.size() / 11;

    cout << "Vertices carregados: " << nVertices << endl;

    return VAO;
}

GLuint loadTexture(string filePath, int& imgWidth, int& imgHeight)
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

    stbi_set_flip_vertically_on_load(false);

    unsigned char* data = stbi_load(
        filePath.c_str(),
        &width,
        &height,
        &nrChannels,
        0
    );

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

        glTexImage2D(
            GL_TEXTURE_2D,
            0,
            format,
            width,
            height,
            0,
            format,
            GL_UNSIGNED_BYTE,
            data
        );

        glGenerateMipmap(GL_TEXTURE_2D);

        imgWidth = width;
        imgHeight = height;

        stbi_image_free(data);
        glBindTexture(GL_TEXTURE_2D, 0);

        cout << "Textura carregada: " << filePath << endl;

        return texID;
    }

    cout << "Failed to load texture: " << filePath << endl;

    imgWidth = 0;
    imgHeight = 0;

    stbi_image_free(data);

    return texID;
}

GLuint loadCubemap(vector<string> faces)
{
    GLuint textureID;

    glGenTextures(1, &textureID);
    glBindTexture(GL_TEXTURE_CUBE_MAP, textureID);

    stbi_set_flip_vertically_on_load(false);

    int width;
    int height;
    int nrChannels;

    for (unsigned int i = 0; i < faces.size(); i++)
    {
        unsigned char* data = stbi_load(
            faces[i].c_str(),
            &width,
            &height,
            &nrChannels,
            0
        );

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

            glTexImage2D(
                GL_TEXTURE_CUBE_MAP_POSITIVE_X + i,
                0,
                format,
                width,
                height,
                0,
                format,
                GL_UNSIGNED_BYTE,
                data
            );

            cout << "Face do cubemap carregada: " << faces[i] << endl;

            stbi_image_free(data);
        }
        else
        {
            cout << "Cubemap tex failed to load at path: " << faces[i] << endl;
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
        scaleFactor * -0.5f, scaleFactor *  0.5f, scaleFactor * -0.5f,
        scaleFactor * -0.5f, scaleFactor * -0.5f, scaleFactor * -0.5f,
        scaleFactor *  0.5f, scaleFactor * -0.5f, scaleFactor * -0.5f,
        scaleFactor *  0.5f, scaleFactor * -0.5f, scaleFactor * -0.5f,
        scaleFactor *  0.5f, scaleFactor *  0.5f, scaleFactor * -0.5f,
        scaleFactor * -0.5f, scaleFactor *  0.5f, scaleFactor * -0.5f,

        scaleFactor * -0.5f, scaleFactor * -0.5f, scaleFactor *  0.5f,
        scaleFactor * -0.5f, scaleFactor * -0.5f, scaleFactor * -0.5f,
        scaleFactor * -0.5f, scaleFactor *  0.5f, scaleFactor * -0.5f,
        scaleFactor * -0.5f, scaleFactor *  0.5f, scaleFactor * -0.5f,
        scaleFactor * -0.5f, scaleFactor *  0.5f, scaleFactor *  0.5f,
        scaleFactor * -0.5f, scaleFactor * -0.5f, scaleFactor *  0.5f,

        scaleFactor *  0.5f, scaleFactor * -0.5f, scaleFactor * -0.5f,
        scaleFactor *  0.5f, scaleFactor * -0.5f, scaleFactor *  0.5f,
        scaleFactor *  0.5f, scaleFactor *  0.5f, scaleFactor *  0.5f,
        scaleFactor *  0.5f, scaleFactor *  0.5f, scaleFactor *  0.5f,
        scaleFactor *  0.5f, scaleFactor *  0.5f, scaleFactor * -0.5f,
        scaleFactor *  0.5f, scaleFactor * -0.5f, scaleFactor * -0.5f,

        scaleFactor * -0.5f, scaleFactor * -0.5f, scaleFactor *  0.5f,
        scaleFactor * -0.5f, scaleFactor *  0.5f, scaleFactor *  0.5f,
        scaleFactor *  0.5f, scaleFactor *  0.5f, scaleFactor *  0.5f,
        scaleFactor *  0.5f, scaleFactor *  0.5f, scaleFactor *  0.5f,
        scaleFactor *  0.5f, scaleFactor * -0.5f, scaleFactor *  0.5f,
        scaleFactor * -0.5f, scaleFactor * -0.5f, scaleFactor *  0.5f,

        scaleFactor * -0.5f, scaleFactor *  0.5f, scaleFactor * -0.5f,
        scaleFactor *  0.5f, scaleFactor *  0.5f, scaleFactor * -0.5f,
        scaleFactor *  0.5f, scaleFactor *  0.5f, scaleFactor *  0.5f,
        scaleFactor *  0.5f, scaleFactor *  0.5f, scaleFactor *  0.5f,
        scaleFactor * -0.5f, scaleFactor *  0.5f, scaleFactor *  0.5f,
        scaleFactor * -0.5f, scaleFactor *  0.5f, scaleFactor * -0.5f,

        scaleFactor * -0.5f, scaleFactor * -0.5f, scaleFactor * -0.5f,
        scaleFactor * -0.5f, scaleFactor * -0.5f, scaleFactor *  0.5f,
        scaleFactor *  0.5f, scaleFactor * -0.5f, scaleFactor * -0.5f,
        scaleFactor *  0.5f, scaleFactor * -0.5f, scaleFactor * -0.5f,
        scaleFactor * -0.5f, scaleFactor * -0.5f, scaleFactor *  0.5f,
        scaleFactor *  0.5f, scaleFactor * -0.5f, scaleFactor *  0.5f
    };

    GLuint skyboxVAO;
    GLuint skyboxVBO;

    glGenVertexArrays(1, &skyboxVAO);
    glGenBuffers(1, &skyboxVBO);

    glBindVertexArray(skyboxVAO);

    glBindBuffer(GL_ARRAY_BUFFER, skyboxVBO);

    glBufferData(
        GL_ARRAY_BUFFER,
        sizeof(skyboxVertices),
        skyboxVertices,
        GL_STATIC_DRAW
    );

    glEnableVertexAttribArray(0);

    glVertexAttribPointer(
        0,
        3,
        GL_FLOAT,
        GL_FALSE,
        3 * sizeof(float),
        (void*)0
    );

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    return skyboxVAO;
}

bool fileExists(const string& filePath)
{
    ifstream file(filePath.c_str());
    return file.good();
}

string findResource(const string& relativePath)
{
    vector<string> prefixes = {
        "",
        "../",
        "../../",
        "../../../",
        "../../../../",
        "../../../../../"
    };

    for (const string& prefix : prefixes)
    {
        string candidate = prefix + relativePath;

        if (fileExists(candidate))
        {
            return candidate;
        }
    }

    cerr << "ERRO: recurso nao encontrado: " << relativePath << endl;
    cerr << "Verifique se voce esta executando a partir da raiz do projeto ou da pasta build." << endl;

    return relativePath;
}