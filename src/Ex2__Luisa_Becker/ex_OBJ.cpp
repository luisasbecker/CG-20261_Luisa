/*
 * Exercício OBJ - Luisa Becker
 * Selecionando e aplicando transformações em objetos 3D
 *
 *
 * Controles:
 *  TAB ........... seleciona o próximo objeto
 *  BACKSPACE ..... seleciona o objeto anterior
 *  R ............. modo rotação
 *  T ............. modo translação
 *  S ............. modo escala
 *  X / Y / Z ..... escolhe o eixo ativo do modo atual
 *  SETA CIMA ..... aplica incremento no eixo ativo
 *  SETA BAIXO .... aplica decremento no eixo ativo
 *  SETA ESQ/DIR .. no modo translação, move diretamente em X
 *  Q / E ......... no modo translação, move diretamente em Z
 *  U / J ......... no modo escala, aumenta/diminui escala uniforme
 *  P ............. alterna projeção perspectiva/ortográfica
 *  M ............. alterna wireframe no objeto selecionado
 *  ESC ........... fecha
 */

#include <iostream>
#include <string>
#include <fstream>
#include <sstream>
#include <vector>
#include <algorithm>

// GLAD
#include <glad/glad.h>

// GLFW
#include <GLFW/glfw3.h>

// GLM
#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

using namespace std;

const GLuint WIDTH = 1000, HEIGHT = 700;

enum class TransformMode
{
    TRANSLATE,
    ROTATE,
    SCALE
};

enum class Axis
{
    X,
    Y,
    Z
};

struct Mesh
{
    GLuint VAO = 0;
    GLuint VBO = 0;
    GLsizei nVertices = 0;
};

struct SceneObject
{
    string name;
    Mesh mesh;
    glm::vec3 basePosition = glm::vec3(0.0f);
    glm::vec3 translation = glm::vec3(0.0f);
    glm::vec3 rotationDeg = glm::vec3(0.0f);
    glm::vec3 scale = glm::vec3(1.0f);
    glm::vec3 color = glm::vec3(1.0f);
};

vector<SceneObject> gObjects;
int gSelectedObject = 0;
TransformMode gCurrentMode = TransformMode::TRANSLATE;
Axis gCurrentAxis = Axis::X;
bool gUsePerspective = true;
bool gDrawSelectedWireframe = true;

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode);

GLuint setupShader();
Mesh loadSimpleOBJ(const string& filePATH);
glm::mat4 buildModelMatrix(const SceneObject& object);
void applyStep(SceneObject& object, float signalDelta);
int getAxisIndex(Axis axis);
void printControls();
string modeToString(TransformMode mode);
string axisToString(Axis axis);

const GLchar* vertexShaderSource = R"glsl(
#version 450 core
layout (location = 0) in vec3 position;

uniform mat4 model;
uniform mat4 view;
uniform mat4 projection;

void main()
{
    gl_Position = projection * view * model * vec4(position, 1.0);
}
)glsl";

const GLchar* fragmentShaderSource = R"glsl(
#version 450 core
out vec4 FragColor;

uniform vec3 objectColor;

void main()
{
    FragColor = vec4(objectColor, 1.0);
}
)glsl";

int main()
{
    glfwInit();

    // Se necessário, descomente e ajuste para a sua máquina:
    // glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    // glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
    // glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

    GLFWwindow* window = glfwCreateWindow(WIDTH, HEIGHT, "Ex2 - OBJ - Luisa Becker", nullptr, nullptr);
    if (!window)
    {
        cout << "Erro ao criar a janela GLFW." << endl;
        glfwTerminate();
        return -1;
    }

    glfwMakeContextCurrent(window);
    glfwSetKeyCallback(window, key_callback);
    glfwSwapInterval(1);

    if (!gladLoadGLLoader((GLADloadproc)glfwGetProcAddress))
    {
        cout << "Failed to initialize GLAD" << endl;
        glfwTerminate();
        return -1;
    }

    cout << "OpenGL version: " << glGetString(GL_VERSION) << endl;
    printControls();

    glViewport(0, 0, WIDTH, HEIGHT);
    glEnable(GL_DEPTH_TEST);

    GLuint shaderID = setupShader();
    glUseProgram(shaderID);

    GLint modelLoc = glGetUniformLocation(shaderID, "model");
    GLint viewLoc = glGetUniformLocation(shaderID, "view");
    GLint projLoc = glGetUniformLocation(shaderID, "projection");
    GLint colorLoc = glGetUniformLocation(shaderID, "objectColor");

    // Carregamento hardcoded de 3 objetos do repositório
    SceneObject cube;
    cube.name = "Cube";
    cube.mesh = loadSimpleOBJ("../assets/Modelos3D/Cube.obj");
    cube.basePosition = glm::vec3(-3.2f, 0.0f, 0.0f);
    cube.scale = glm::vec3(0.85f);
    cube.color = glm::vec3(0.85f, 0.35f, 0.35f);
    gObjects.push_back(cube);

    SceneObject suzanne;
    suzanne.name = "Suzanne";
    suzanne.mesh = loadSimpleOBJ("../assets/Modelos3D/Suzanne.obj");
    suzanne.basePosition = glm::vec3(0.0f, 0.0f, 0.0f);
    suzanne.scale = glm::vec3(1.0f);
    suzanne.color = glm::vec3(0.30f, 0.75f, 0.85f);
    gObjects.push_back(suzanne);

    SceneObject suzanneSubdiv;
    suzanneSubdiv.name = "SuzanneSubdiv1";
    suzanneSubdiv.mesh = loadSimpleOBJ("../assets/Modelos3D/SuzanneSubdiv1.obj");
    suzanneSubdiv.basePosition = glm::vec3(3.2f, 0.0f, 0.0f);
    suzanneSubdiv.scale = glm::vec3(1.0f);
    suzanneSubdiv.color = glm::vec3(0.45f, 0.85f, 0.35f);
    gObjects.push_back(suzanneSubdiv);

    for (const auto& object : gObjects)
    {
        if (object.mesh.nVertices == 0)
        {
            cout << "Erro ao carregar um dos modelos OBJ. Encerrando." << endl;
            glfwTerminate();
            return -1;
        }
    }

    glm::mat4 view = glm::lookAt(
        glm::vec3(0.0f, 2.0f, 9.0f),
        glm::vec3(0.0f, 0.0f, 0.0f),
        glm::vec3(0.0f, 1.0f, 0.0f)
    );

    float deltaTime = 0.0f;
    float lastFrame = 0.0f;

    while (!glfwWindowShouldClose(window))
    {
        float currentFrame = glfwGetTime();
        deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        glfwPollEvents();

        // Controles contínuos da transformação no eixo ativo
        if (glfwGetKey(window, GLFW_KEY_UP) == GLFW_PRESS)
            applyStep(gObjects[gSelectedObject], +deltaTime);

        if (glfwGetKey(window, GLFW_KEY_DOWN) == GLFW_PRESS)
            applyStep(gObjects[gSelectedObject], -deltaTime);

        // Atalhos extras para translação direta
        if (gCurrentMode == TransformMode::TRANSLATE)
        {
            const float moveStep = 2.0f * deltaTime;

            if (glfwGetKey(window, GLFW_KEY_LEFT) == GLFW_PRESS)
                gObjects[gSelectedObject].translation.x -= moveStep;

            if (glfwGetKey(window, GLFW_KEY_RIGHT) == GLFW_PRESS)
                gObjects[gSelectedObject].translation.x += moveStep;

            if (glfwGetKey(window, GLFW_KEY_Q) == GLFW_PRESS)
                gObjects[gSelectedObject].translation.z -= moveStep;

            if (glfwGetKey(window, GLFW_KEY_E) == GLFW_PRESS)
                gObjects[gSelectedObject].translation.z += moveStep;
        }

        // Escala uniforme opcional
        if (gCurrentMode == TransformMode::SCALE)
        {
            const float uniformStep = 1.0f * deltaTime;

            if (glfwGetKey(window, GLFW_KEY_U) == GLFW_PRESS)
            {
                gObjects[gSelectedObject].scale += glm::vec3(uniformStep);
            }

            if (glfwGetKey(window, GLFW_KEY_J) == GLFW_PRESS)
            {
                gObjects[gSelectedObject].scale -= glm::vec3(uniformStep);
                gObjects[gSelectedObject].scale.x = std::max(0.1f, gObjects[gSelectedObject].scale.x);
                gObjects[gSelectedObject].scale.y = std::max(0.1f, gObjects[gSelectedObject].scale.y);
                gObjects[gSelectedObject].scale.z = std::max(0.1f, gObjects[gSelectedObject].scale.z);
            }
        }

        glClearColor(0.18f, 0.18f, 0.20f, 1.0f);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

        glUseProgram(shaderID);

        glm::mat4 projection;
        if (gUsePerspective)
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
                -6.0f, 6.0f,
                -4.0f, 4.0f,
                0.1f, 100.0f
            );
        }

        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
        glUniformMatrix4fv(projLoc, 1, GL_FALSE, glm::value_ptr(projection));

        for (int i = 0; i < (int)gObjects.size(); i++)
        {
            const bool selected = (i == gSelectedObject);
            glm::mat4 model = buildModelMatrix(gObjects[i]);

            glm::vec3 drawColor = gObjects[i].color;
            if (selected)
            {
                drawColor = glm::min(drawColor + glm::vec3(0.25f), glm::vec3(1.0f));
            }

            glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(model));
            glUniform3fv(colorLoc, 1, glm::value_ptr(drawColor));

            glBindVertexArray(gObjects[i].mesh.VAO);
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            glDrawArrays(GL_TRIANGLES, 0, gObjects[i].mesh.nVertices);

            if (selected && gDrawSelectedWireframe)
            {
                glm::mat4 wireModel = model * glm::scale(glm::mat4(1.0f), glm::vec3(1.01f));
                glUniformMatrix4fv(modelLoc, 1, GL_FALSE, glm::value_ptr(wireModel));
                glUniform3f(colorLoc, 0.05f, 0.05f, 0.05f);

                glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
                glLineWidth(1.5f);
                glDrawArrays(GL_TRIANGLES, 0, gObjects[i].mesh.nVertices);
                glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            }
        }

        glBindVertexArray(0);
        glfwSwapBuffers(window);
    }

    for (auto& object : gObjects)
    {
        if (object.mesh.VAO != 0)
            glDeleteVertexArrays(1, &object.mesh.VAO);

        if (object.mesh.VBO != 0)
            glDeleteBuffers(1, &object.mesh.VBO);
    }

    glDeleteProgram(shaderID);
    glfwTerminate();
    return 0;
}

void key_callback(GLFWwindow* window, int key, int scancode, int action, int mode)
{
    if (action != GLFW_PRESS)
        return;

    if (key == GLFW_KEY_ESCAPE)
    {
        glfwSetWindowShouldClose(window, GL_TRUE);
        return;
    }

    if (!gObjects.empty())
    {
        if (key == GLFW_KEY_TAB)
        {
            gSelectedObject = (gSelectedObject + 1) % (int)gObjects.size();
            cout << "Objeto selecionado: " << gObjects[gSelectedObject].name << endl;
        }

        if (key == GLFW_KEY_BACKSPACE)
        {
            gSelectedObject--;
            if (gSelectedObject < 0)
                gSelectedObject = (int)gObjects.size() - 1;

            cout << "Objeto selecionado: " << gObjects[gSelectedObject].name << endl;
        }
    }

    if (key == GLFW_KEY_R)
    {
        gCurrentMode = TransformMode::ROTATE;
        cout << "Modo atual: " << modeToString(gCurrentMode) << endl;
    }

    if (key == GLFW_KEY_T)
    {
        gCurrentMode = TransformMode::TRANSLATE;
        cout << "Modo atual: " << modeToString(gCurrentMode) << endl;
    }

    if (key == GLFW_KEY_S)
    {
        gCurrentMode = TransformMode::SCALE;
        cout << "Modo atual: " << modeToString(gCurrentMode) << endl;
    }

    if (key == GLFW_KEY_X)
    {
        gCurrentAxis = Axis::X;
        cout << "Eixo atual: " << axisToString(gCurrentAxis) << endl;
    }

    if (key == GLFW_KEY_Y)
    {
        gCurrentAxis = Axis::Y;
        cout << "Eixo atual: " << axisToString(gCurrentAxis) << endl;
    }

    if (key == GLFW_KEY_Z)
    {
        gCurrentAxis = Axis::Z;
        cout << "Eixo atual: " << axisToString(gCurrentAxis) << endl;
    }

    if (key == GLFW_KEY_P)
    {
        gUsePerspective = !gUsePerspective;
        cout << "Projeção: " << (gUsePerspective ? "Perspectiva" : "Ortográfica") << endl;
    }

    if (key == GLFW_KEY_M)
    {
        gDrawSelectedWireframe = !gDrawSelectedWireframe;
        cout << "Wireframe do selecionado: " << (gDrawSelectedWireframe ? "Ligado" : "Desligado") << endl;
    }
}

GLuint setupShader()
{
    GLuint vertexShader = glCreateShader(GL_VERTEX_SHADER);
    glShaderSource(vertexShader, 1, &vertexShaderSource, NULL);
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
    glShaderSource(fragmentShader, 1, &fragmentShaderSource, NULL);
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

Mesh loadSimpleOBJ(const string& filePATH)
{
    vector<glm::vec3> vertices;
    vector<GLfloat> vBuffer;

    ifstream arqEntrada(filePATH.c_str());
    if (!arqEntrada.is_open())
    {
        cerr << "Erro ao tentar ler o arquivo " << filePATH << endl;
        return {};
    }

    string line;
    while (getline(arqEntrada, line))
    {
        if (line.empty() || line[0] == '#')
            continue;

        istringstream ssline(line);
        string word;
        ssline >> word;

        if (word == "v")
        {
            glm::vec3 vertice;
            ssline >> vertice.x >> vertice.y >> vertice.z;
            vertices.push_back(vertice);
        }
        else if (word == "f")
        {
            vector<int> faceIndices;
            string token;

            while (ssline >> token)
            {
                istringstream ss(token);
                string index;
                if (getline(ss, index, '/'))
                {
                    if (!index.empty())
                    {
                        int vi = stoi(index);

                        // suporta índice positivo e negativo do OBJ
                        if (vi > 0)
                            vi = vi - 1;
                        else
                            vi = (int)vertices.size() + vi;

                        faceIndices.push_back(vi);
                    }
                }
            }

            // triangulação em fan: (0, i, i+1)
            for (size_t i = 1; i + 1 < faceIndices.size(); i++)
            {
                int ids[3] = {
                    faceIndices[0],
                    faceIndices[i],
                    faceIndices[i + 1]
                };

                for (int k = 0; k < 3; k++)
                {
                    const glm::vec3& v = vertices[ids[k]];
                    vBuffer.push_back(v.x);
                    vBuffer.push_back(v.y);
                    vBuffer.push_back(v.z);
                }
            }
        }
    }

    arqEntrada.close();

    Mesh mesh;

    glGenVertexArrays(1, &mesh.VAO);
    glGenBuffers(1, &mesh.VBO);

    glBindVertexArray(mesh.VAO);
    glBindBuffer(GL_ARRAY_BUFFER, mesh.VBO);
    glBufferData(GL_ARRAY_BUFFER, vBuffer.size() * sizeof(GLfloat), vBuffer.data(), GL_STATIC_DRAW);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(GLfloat), (GLvoid*)0);
    glEnableVertexAttribArray(0);

    glBindBuffer(GL_ARRAY_BUFFER, 0);
    glBindVertexArray(0);

    mesh.nVertices = (GLsizei)(vBuffer.size() / 3);

    cout << "OBJ carregado: " << filePATH << " | vertices para draw: " << mesh.nVertices << endl;

    return mesh;
}

glm::mat4 buildModelMatrix(const SceneObject& object)
{
    glm::mat4 model = glm::mat4(1.0f);

    model = glm::translate(model, object.basePosition + object.translation);

    model = glm::rotate(model, glm::radians(object.rotationDeg.x), glm::vec3(1.0f, 0.0f, 0.0f));
    model = glm::rotate(model, glm::radians(object.rotationDeg.y), glm::vec3(0.0f, 1.0f, 0.0f));
    model = glm::rotate(model, glm::radians(object.rotationDeg.z), glm::vec3(0.0f, 0.0f, 1.0f));

    model = glm::scale(model, object.scale);

    return model;
}

void applyStep(SceneObject& object, float signalDelta)
{
    int axis = getAxisIndex(gCurrentAxis);

    if (gCurrentMode == TransformMode::TRANSLATE)
    {
        object.translation[axis] += 2.0f * signalDelta;
    }
    else if (gCurrentMode == TransformMode::ROTATE)
    {
        object.rotationDeg[axis] += 90.0f * signalDelta;
    }
    else if (gCurrentMode == TransformMode::SCALE)
    {
        object.scale[axis] += 1.0f * signalDelta;
        object.scale[axis] = std::max(0.1f, object.scale[axis]);
    }
}

int getAxisIndex(Axis axis)
{
    switch (axis)
    {
        case Axis::X: return 0;
        case Axis::Y: return 1;
        case Axis::Z: return 2;
    }
    return 0;
}

string modeToString(TransformMode mode)
{
    switch (mode)
    {
        case TransformMode::TRANSLATE: return "Translacao";
        case TransformMode::ROTATE:    return "Rotacao";
        case TransformMode::SCALE:     return "Escala";
    }
    return "";
}

string axisToString(Axis axis)
{
    switch (axis)
    {
        case Axis::X: return "X";
        case Axis::Y: return "Y";
        case Axis::Z: return "Z";
    }
    return "";
}

void printControls()
{
    cout << "\n=========== CONTROLES ===========\n";
    cout << "TAB ........... seleciona o proximo objeto\n";
    cout << "BACKSPACE ..... seleciona o anterior\n";
    cout << "R / T / S ..... escolhe modo: rotacao / translacao / escala\n";
    cout << "X / Y / Z ..... escolhe o eixo ativo\n";
    cout << "SETA CIMA ..... incrementa no eixo ativo\n";
    cout << "SETA BAIXO .... decrementa no eixo ativo\n";
    cout << "SETA ESQ/DIR .. translacao direta em X (modo T)\n";
    cout << "Q / E ......... translacao direta em Z (modo T)\n";
    cout << "U / J ......... escala uniforme (modo S)\n";
    cout << "P ............. alterna projecao\n";
    cout << "M ............. alterna wireframe no selecionado\n";
    cout << "ESC ........... sair\n";
    cout << "=================================\n\n";
}