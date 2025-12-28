#if defined (__APPLE__)
#define GLFW_INCLUDE_GLCOREARB
#define GL_SILENCE_DEPRECATION
#else
#define GLEW_STATIC
#include <GL/glew.h>
#endif

#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/matrix_inverse.hpp>
#include <glm/gtc/type_ptr.hpp>

#include "Window.h"
#include "Shader.hpp"
#include "Camera.hpp"
#include "Model3D.hpp"
#include "SkyBox.hpp"

#include <iostream>
#include <vector>

// --- Window ---
gps::Window myWindow;
const unsigned int SHADOW_WIDTH = 4096, SHADOW_HEIGHT = 4096;

// --- Rendering Modes ---
enum RenderMode { SOLID = 0, WIREFRAME = 1, POINT = 2 };
RenderMode currentRenderMode = SOLID;

// --- Camera ---
gps::Camera myCamera(
    glm::vec3(0.0f, 0.0f, 3.0f),
    glm::vec3(0.0f, 0.0f, -10.0f),
    glm::vec3(0.0f, 1.0f, 0.0f)
);
float cameraSpeed = 0.5f;

// --- Matrices & Shaders ---
glm::mat4 model;
glm::mat4 view;
glm::mat4 projection;
glm::mat3 normalMatrix;

gps::Shader myBasicShader;
gps::Shader depthMapShader;
gps::Shader skyboxShader;

// --- Lighting ---
glm::vec3 lightDir;
glm::vec3 lightColor;
glm::vec3 pointLightPos;
glm::vec3 pointLightColor;

GLint modelLoc, viewLoc, projectionLoc, normalMatrixLoc;
GLint lightDirLoc, lightColorLoc;

// --- Models ---
gps::Model3D teapot;
gps::Model3D ground;
gps::Model3D watchTower;
gps::Model3D house;
gps::Model3D trees;
gps::Model3D fence;
gps::Model3D big_tree, big_tree2, big_tree3;
gps::Model3D lantern;
gps::Model3D bear;
gps::Model3D windmillBase, windmillBlades;
gps::Model3D well;
gps::Model3D casuta;
gps::Model3D campfire;
gps::SkyBox mySkyBox;
gps::SkyBox myNightSkyBox;

// --- Animation & Interaction ---
GLboolean pressedKeys[1024];
float angle = 0.0f;
float bladesAngle = 0.0f;
bool startTour = false;
float tourAngle = 0.0f;
bool fogEnabled = false;
GLint fogEnabledLoc;

// --- Mouse State ---
GLboolean firstMouse = true;
GLfloat lastX = 400, lastY = 300;
GLfloat yaw = -90.0f;
GLfloat pitch = 0.0f;
GLfloat sensitivity = 0.1f;

// --- Shadow Mapping ---
GLuint shadowMapFBO;
GLuint depthMapTexture;

struct SnowParticle {
    glm::vec3 position;
    glm::vec3 velocity;
    float size;
    float lifetime;
};

std::vector<SnowParticle> snowParticles;
const int MAX_SNOW_PARTICLES = 50000;
bool snowEnabled = false;

GLuint snowVAO, snowVBO;
gps::Shader snowShader;

glm::vec3 lanternWorldPos = glm::vec3(-7.0f, -0.4f, -1.0f);
glm::vec3 campfireWorldPos = glm::vec3(-7.0f, -1.1f, -5.0f);

bool lanternLightEnabled = true;
bool campfireLightEnabled = true;
bool sunLightEnabled = true; 

GLenum glCheckError_(const char* file, int line) {
    GLenum errorCode;
    while ((errorCode = glGetError()) != GL_NO_ERROR) {
        std::string error;
        switch (errorCode) {
        case GL_INVALID_ENUM: error = "INVALID_ENUM"; break;
        case GL_INVALID_VALUE: error = "INVALID_VALUE"; break;
        case GL_INVALID_OPERATION: error = "INVALID_OPERATION"; break;
        case GL_OUT_OF_MEMORY: error = "OUT_OF_MEMORY"; break;
        case GL_INVALID_FRAMEBUFFER_OPERATION: error = "INVALID_FRAMEBUFFER_OPERATION"; break;
        }
        std::cout << error << " | " << file << " (" << line << ")" << std::endl;
    }
    return errorCode;
}
#define glCheckError() glCheckError_(__FILE__, __LINE__)

glm::mat4 computeLightSpaceTrMatrix() {
    glm::mat4 lightProjection = glm::ortho(-150.0f, 150.0f, -150.0f, 150.0f, 0.1f, 400.0f);
    glm::vec3 lightPos = normalize(lightDir) * 150.0f;
    glm::vec3 target = glm::vec3(0.0f, 0.0f, 50.0f);
    glm::mat4 lightView = glm::lookAt(lightPos, target, glm::vec3(0.0f, 1.0f, 0.0f));
    return lightProjection * lightView;
}

void drawModel(gps::Model3D& modelObj, gps::Shader& shader, glm::vec3 position, glm::vec3 scale = glm::vec3(1.0f), float rotAngle = 0.0f, glm::vec3 rotAxis = glm::vec3(0, 1, 0)) {
    shader.useShaderProgram();

    glm::mat4 m = glm::mat4(1.0f);
    m = glm::translate(m, position);

    if (rotAngle != 0.0f) {
        m = glm::rotate(m, glm::radians(rotAngle), rotAxis);
    }

    m = glm::scale(m, scale);

    glUniformMatrix4fv(glGetUniformLocation(shader.shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(m));

    if (glGetUniformLocation(shader.shaderProgram, "normalMatrix") != -1) {
        glm::mat3 normMat = glm::mat3(glm::inverseTranspose(view * m));
        glUniformMatrix3fv(glGetUniformLocation(shader.shaderProgram, "normalMatrix"), 1, GL_FALSE, glm::value_ptr(normMat));
    }

    modelObj.Draw(shader);
}

void windowResizeCallback(GLFWwindow* window, int width, int height) {
    fprintf(stdout, "Window resized! New width: %d , and height: %d\n", width, height);
}

void keyboardCallback(GLFWwindow* window, int key, int scancode, int action, int mode) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS)
        glfwSetWindowShouldClose(window, GL_TRUE);

    if (key >= 0 && key < 1024) {
        if (action == GLFW_PRESS) pressedKeys[key] = true;
        else if (action == GLFW_RELEASE) pressedKeys[key] = false;
    }

    if (action == GLFW_PRESS) {
        if (key == GLFW_KEY_1) {
            currentRenderMode = SOLID;
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
        }
        else if (key == GLFW_KEY_2) {
            currentRenderMode = WIREFRAME;
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
        }
        else if (key == GLFW_KEY_3) {
            currentRenderMode = POINT;
            glPolygonMode(GL_FRONT_AND_BACK, GL_POINT);
            glPointSize(3.0f);
        }
        else if (key == GLFW_KEY_F) {
            fogEnabled = !fogEnabled;
            myBasicShader.useShaderProgram();
            glUniform1i(fogEnabledLoc, fogEnabled);
        }
        else if (key == GLFW_KEY_P) {
            startTour = !startTour;
        }
        else if (key == GLFW_KEY_N) {
            snowEnabled = !snowEnabled;
        }
        else if (key == GLFW_KEY_L) {
            lanternLightEnabled = !lanternLightEnabled;
            std::cout << "Lantern Light: " << (lanternLightEnabled ? "ON" : "OFF") << std::endl;
        }
        else if (key == GLFW_KEY_C) {
            campfireLightEnabled = !campfireLightEnabled;
            std::cout << "Campfire Light: " << (campfireLightEnabled ? "ON" : "OFF") << std::endl;
        }
        else if (key == GLFW_KEY_K && action == GLFW_PRESS) {
            sunLightEnabled = !sunLightEnabled;
            myBasicShader.useShaderProgram();
            GLint sunEnabledLoc = glGetUniformLocation(myBasicShader.shaderProgram, "sunEnabled");
            glUniform1i(sunEnabledLoc, sunLightEnabled);
            std::cout << "Sun Light: " << (sunLightEnabled ? "ON" : "OFF") << std::endl;
        }
    }
}

void mouseCallback(GLFWwindow* window, double xpos, double ypos) {
    if (firstMouse) {
        lastX = xpos; lastY = ypos; firstMouse = false;
    }
    float xoffset = (xpos - lastX) * sensitivity;
    float yoffset = (lastY - ypos) * sensitivity;
    lastX = xpos; lastY = ypos;

    yaw += xoffset;
    pitch += yoffset;

    if (pitch > 89.0f) pitch = 89.0f;
    if (pitch < -89.0f) pitch = -89.0f;

    myCamera.rotate(pitch, yaw);
    view = myCamera.getViewMatrix();
    myBasicShader.useShaderProgram();
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
    normalMatrix = glm::mat3(glm::inverseTranspose(view * model));
}

void processMovement() {
    const GLfloat MIN_CAMERA_HEIGHT = 0.0f;

    if (startTour) {
        tourAngle += 0.5f;
        float radius = 30.0f;
        float camX = sin(glm::radians(tourAngle)) * radius;
        float camZ = cos(glm::radians(tourAngle)) * radius;
        myCamera.setPosition(glm::vec3(camX, 5.0f, camZ));
        view = glm::lookAt(glm::vec3(camX, 10.0f, camZ), glm::vec3(0.0f, 2.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
    }
    else {
        if (pressedKeys[GLFW_KEY_W]) myCamera.move(gps::MOVE_FORWARD, cameraSpeed);
        if (pressedKeys[GLFW_KEY_S]) myCamera.move(gps::MOVE_BACKWARD, cameraSpeed);
        if (pressedKeys[GLFW_KEY_A]) myCamera.move(gps::MOVE_LEFT, cameraSpeed);
        if (pressedKeys[GLFW_KEY_D]) myCamera.move(gps::MOVE_RIGHT, cameraSpeed);

        if (pressedKeys[GLFW_KEY_Q]) angle -= 1.0f;
        if (pressedKeys[GLFW_KEY_E]) angle += 1.0f;

        view = myCamera.getViewMatrix();
    }

    myBasicShader.useShaderProgram();
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
    normalMatrix = glm::mat3(glm::inverseTranspose(view * model));
    glUniformMatrix3fv(normalMatrixLoc, 1, GL_FALSE, glm::value_ptr(normalMatrix));

    // Ground collision
    glm::vec3 currentPosition = myCamera.getPosition();
    if (currentPosition.y < MIN_CAMERA_HEIGHT) {
        myCamera.setPosition(glm::vec3(currentPosition.x, MIN_CAMERA_HEIGHT, currentPosition.z));
    }
}

void initOpenGLWindow() {
    myWindow.Create(1024, 768, "OpenGL Project Core");
}

void setWindowCallbacks() {
    glfwSetWindowSizeCallback(myWindow.getWindow(), windowResizeCallback);
    glfwSetKeyCallback(myWindow.getWindow(), keyboardCallback);
    glfwSetCursorPosCallback(myWindow.getWindow(), mouseCallback);
}

void initOpenGLState() {
    glClearColor(0.5f, 0.5f, 0.5f, 1.0f);
    glViewport(0, 0, myWindow.getWindowDimensions().width, myWindow.getWindowDimensions().height);
    glEnable(GL_FRAMEBUFFER_SRGB);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LESS);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK);
    glFrontFace(GL_CCW);
}

void initFBO() {
    glGenFramebuffers(1, &shadowMapFBO);
    glGenTextures(1, &depthMapTexture);
    glBindTexture(GL_TEXTURE_2D, depthMapTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);
    glBindFramebuffer(GL_FRAMEBUFFER, shadowMapFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMapTexture, 0);
    glDrawBuffer(GL_NONE); glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

void initModels() {
    teapot.LoadModel("models/teapot/teapot20segUT.obj");
    ground.LoadModel("models/ground/ground.obj");
    watchTower.LoadModel("models/watch_tower/watch_tower.obj");
    house.LoadModel("models/house/house.obj");
    trees.LoadModel("models/trees/trees.obj");
    fence.LoadModel("models/fence/gard.obj");
    big_tree.LoadModel("models/big_tree/big_tree.obj");
    big_tree2.LoadModel("models/big_tree2/big_tree2.obj");
    big_tree3.LoadModel("models/big_tree3/big_tree3.obj");
    windmillBase.LoadModel("models/windmill/windmill.obj");
    windmillBlades.LoadModel("models/blades/blades.obj");
    lantern.LoadModel("models/lantern/lantern.obj");
    well.LoadModel("models/well/well.obj");
    casuta.LoadModel("models/casuta/casuta.obj");
    bear.LoadModel("models/bear/bear.obj");
	campfire.LoadModel("models/campfire/campfire.obj");
}

void initShaders() {
    myBasicShader.loadShader("shaders/basic.vert", "shaders/basic.frag");
    depthMapShader.loadShader("shaders/depthMap.vert", "shaders/depthMap.frag");
    skyboxShader.loadShader("shaders/skyboxShader.vert", "shaders/skyboxShader.frag");
    snowShader.loadShader("shaders/snow.vert", "shaders/snow.frag");
}

void initUniforms() {
    myBasicShader.useShaderProgram();
    model = glm::mat4(1.0f);
    modelLoc = glGetUniformLocation(myBasicShader.shaderProgram, "model");
    view = myCamera.getViewMatrix();
    viewLoc = glGetUniformLocation(myBasicShader.shaderProgram, "view");
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
    normalMatrix = glm::mat3(glm::inverseTranspose(view * model));
    normalMatrixLoc = glGetUniformLocation(myBasicShader.shaderProgram, "normalMatrix");
    projection = glm::perspective(glm::radians(45.0f), (float)myWindow.getWindowDimensions().width / (float)myWindow.getWindowDimensions().height, 0.1f, 200.0f);
    projectionLoc = glGetUniformLocation(myBasicShader.shaderProgram, "projection");
    glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(projection));

    lightDir = glm::vec3(0.0f, 20.0f, 20.0f);
    lightDirLoc = glGetUniformLocation(myBasicShader.shaderProgram, "lightDir");
    glUniform3fv(lightDirLoc, 1, glm::value_ptr(lightDir));

    lightColor = glm::vec3(0.2f, 0.2f, 0.2f);
    lightColorLoc = glGetUniformLocation(myBasicShader.shaderProgram, "lightColor");
    glUniform3fv(lightColorLoc, 1, glm::value_ptr(lightColor));

    GLint sunEnabledLoc = glGetUniformLocation(myBasicShader.shaderProgram, "sunEnabled");
    glUniform1i(sunEnabledLoc, sunLightEnabled);

    pointLightColor = glm::vec3(1.0f, 0.6f, 0.0f);
    glUniform3fv(glGetUniformLocation(myBasicShader.shaderProgram, "pointLightColor"), 1, glm::value_ptr(pointLightColor));

    fogEnabledLoc = glGetUniformLocation(myBasicShader.shaderProgram, "enableFog");
    glUniform1i(fogEnabledLoc, fogEnabled);
}

void initSnowParticles() {
    snowParticles.reserve(MAX_SNOW_PARTICLES);

    for (int i = 0; i < MAX_SNOW_PARTICLES; i++) {
        SnowParticle particle;
        particle.position = glm::vec3(
            (rand() % 1000 - 500) / 10.0f, 
            (rand() % 200) / 10.0f + 0.5f,  
            (rand() % 1000 - 500) / 10.0f  
        );
        particle.velocity = glm::vec3(
            (rand() % 20 - 10) / 50.0f,    
            -(rand() % 30 + 20) / 50.0f,
            (rand() % 20 - 10) / 50.0f
        );
        particle.size = (rand() % 80 + 80) / 100.0f;
        particle.lifetime = (rand() % 100) / 10.0f;
        snowParticles.push_back(particle);
    }

    glGenVertexArrays(1, &snowVAO);
    glGenBuffers(1, &snowVBO);
}

void updateSnowParticles(float deltaTime) {
    if (!snowEnabled) return;

    for (auto& particle : snowParticles) {
        particle.position += particle.velocity * deltaTime;
        particle.lifetime += deltaTime;

        if (particle.position.y < -1.0f ||
            abs(particle.position.x) > 100.0f ||
            abs(particle.position.z) > 100.0f) {

            particle.position = glm::vec3(
                (rand() % 1000 - 500) / 10.0f,  
                (rand() % 100) / 10.0f + 0.5f, 
                (rand() % 1000 - 500) / 10.0f
            );
            particle.velocity.y = -(rand() % 50 + 20) / 50.0f;
            particle.lifetime = 0.0f;
        }
    }
}

void renderSnow() {
    if (!snowEnabled || snowParticles.empty()) return;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDepthMask(GL_FALSE); 

    snowShader.useShaderProgram();

    glUniformMatrix4fv(glGetUniformLocation(snowShader.shaderProgram, "view"),
        1, GL_FALSE, glm::value_ptr(view));
    glUniformMatrix4fv(glGetUniformLocation(snowShader.shaderProgram, "projection"),
        1, GL_FALSE, glm::value_ptr(projection));

    std::vector<glm::vec3> positions;
    for (const auto& particle : snowParticles) {
        positions.push_back(particle.position);
    }

    glBindVertexArray(snowVAO);
    glBindBuffer(GL_ARRAY_BUFFER, snowVBO);
    glBufferData(GL_ARRAY_BUFFER, positions.size() * sizeof(glm::vec3),
        positions.data(), GL_DYNAMIC_DRAW);

    glEnableVertexAttribArray(0);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(glm::vec3), (void*)0);

    glUniform1f(glGetUniformLocation(snowShader.shaderProgram, "pointSize"), 10.0f); // fulgi mai mari

    glDrawArrays(GL_POINTS, 0, positions.size());

    glDepthMask(GL_TRUE);
    glDisable(GL_BLEND);
}

void initSkybox() {

    std::vector<const GLchar*> faces;
    faces.push_back("skybox/posx.jpg");
    faces.push_back("skybox/negx.jpg");
    faces.push_back("skybox/posy.jpg");
    faces.push_back("skybox/negy.jpg");
    faces.push_back("skybox/posz.jpg");
    faces.push_back("skybox/negz.jpg");
    mySkyBox.Load(faces);

    std::vector<const GLchar*> darkFaces;
	darkFaces.push_back("skybox/dark_posx.jpg");
	darkFaces.push_back("skybox/dark_negx.jpg");
	darkFaces.push_back("skybox/dark_posy.jpg");
	darkFaces.push_back("skybox/dark_negy.jpg");
	darkFaces.push_back("skybox/dark_posz.jpg");
	darkFaces.push_back("skybox/dark_negz.jpg");
	myNightSkyBox.Load(darkFaces);
}

void renderAllObjects(gps::Shader shader) {

    // 1. Ceainic 
    drawModel(teapot, shader, glm::vec3(-5.0f, -3.0f, 5.0f), glm::vec3(0.25f), angle);

    // 2. Ground
    drawModel(ground, shader, glm::vec3(0.0f, -1.0f, 0.0f));

    // 3. Watch Tower
    drawModel(watchTower, shader, glm::vec3(2.0f, -1.0f, -3.0f));

    // 4. House
    drawModel(house, shader, glm::vec3(-1.0f, -0.8f, -1.0f));

    // 5. Fence
    drawModel(fence, shader, glm::vec3(0.0f, -1.0f, 0.0f));

    // 6. Trees
    drawModel(trees, shader, glm::vec3(-2.0f, -1.0f, -2.0f));

    // 7. Big Trees
    drawModel(big_tree, shader, glm::vec3(3.0f, -1.0f, -4.0f));
    drawModel(big_tree2, shader, glm::vec3(-3.0f, -1.0f, -4.0f));
    drawModel(big_tree3, shader, glm::vec3(0.0f, -1.0f, -5.0f));

    lanternWorldPos = glm::vec3(-7.0f, -0.4f, -1.0f);
    drawModel(lantern, shader, lanternWorldPos, glm::vec3(0.5f));

    // 9. Well
    drawModel(well, shader, glm::vec3(5.0f, -1.0f, 5.0f));

    // 10. Casuta
    drawModel(casuta, shader, glm::vec3(-5.0f, -3.0f, 5.0f));

    // 11. Bear
    drawModel(bear, shader, glm::vec3(0.0f, -0.2f, -3.0f), glm::vec3(0.5f));

    // 12. Windmill
    glm::vec3 windmillPos = glm::vec3(20.0f, 20.0f, 100.0f);
    drawModel(windmillBase, shader, windmillPos, glm::vec3(0.5f));

    campfireWorldPos = glm::vec3(-7.0f, -1.1f, -5.0f);
    drawModel(campfire, shader, campfireWorldPos);

    // 14. Windmill Blades
    bladesAngle += 1.0f;
    shader.useShaderProgram();
    glm::mat4 modelBlades = glm::mat4(1.0f);
    modelBlades = glm::translate(modelBlades, windmillPos);
    modelBlades = glm::translate(modelBlades, glm::vec3(0.0f, 4.0f, -2.8f));
    modelBlades = glm::rotate(modelBlades, glm::radians(bladesAngle), glm::vec3(0.0f, 0.0f, 1.0f));
    modelBlades = glm::scale(modelBlades, glm::vec3(0.5f));
    glUniformMatrix4fv(glGetUniformLocation(shader.shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(modelBlades));
    if (glGetUniformLocation(shader.shaderProgram, "normalMatrix") != -1) {
        normalMatrix = glm::mat3(glm::inverseTranspose(view * modelBlades));
        glUniformMatrix3fv(glGetUniformLocation(shader.shaderProgram, "normalMatrix"), 1, GL_FALSE, glm::value_ptr(normalMatrix));
    }
    windmillBlades.Draw(shader);
}
void renderScene() {
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    depthMapShader.useShaderProgram();
    glUniformMatrix4fv(glGetUniformLocation(depthMapShader.shaderProgram, "lightSpaceTrMatrix"),
        1, GL_FALSE, glm::value_ptr(computeLightSpaceTrMatrix()));

    glViewport(0, 0, SHADOW_WIDTH, SHADOW_HEIGHT);
    glBindFramebuffer(GL_FRAMEBUFFER, shadowMapFBO);
    glClear(GL_DEPTH_BUFFER_BIT);

    glCullFace(GL_FRONT);
    renderAllObjects(depthMapShader);
    glCullFace(GL_BACK);

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glViewport(0, 0, myWindow.getWindowDimensions().width, myWindow.getWindowDimensions().height);
    myBasicShader.useShaderProgram();

    glUniformMatrix4fv(glGetUniformLocation(myBasicShader.shaderProgram, "lightSpaceTrMatrix"),
        1, GL_FALSE, glm::value_ptr(computeLightSpaceTrMatrix()));

    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, depthMapTexture);
    glUniform1i(glGetUniformLocation(myBasicShader.shaderProgram, "shadowMap"), 3);

    glm::vec3 lightSourcePos = lanternWorldPos + glm::vec3(-6.0f, 0.5f, -1.5f);
    glm::vec3 lightPosEye = glm::vec3(view * glm::vec4(lightSourcePos, 1.0f));
    glUniform3fv(glGetUniformLocation(myBasicShader.shaderProgram, "pointLightPos"), 1, glm::value_ptr(lightPosEye));

    glm::vec3 lanternColor = lanternLightEnabled ? glm::vec3(1.0f, 0.6f, 0.0f) : glm::vec3(0.0f, 0.0f, 0.0f);
    glUniform3fv(glGetUniformLocation(myBasicShader.shaderProgram, "pointLightColor"), 1, glm::value_ptr(lanternColor));

    glm::vec3 fireSourcePos = campfireWorldPos + glm::vec3(-10.0f, 0.5f, -40.0f); 
    glm::vec3 firePosEye = glm::vec3(view * glm::vec4(fireSourcePos, 1.0f));
    glUniform3fv(glGetUniformLocation(myBasicShader.shaderProgram, "campfirePos"), 1, glm::value_ptr(firePosEye));

    glm::vec3 fireColor;
    if (campfireLightEnabled) {
        float time = glfwGetTime();
        float flicker = 0.8f + (sin(time * 10.0f) * 0.1f) + (cos(time * 23.0f) * 0.1f);
        fireColor = glm::vec3(1.0f, 0.4f, 0.0f) * flicker * 5.0f;
    }
    else {
        fireColor = glm::vec3(0.0f, 0.0f, 0.0f); // Stins
    }
    glUniform3fv(glGetUniformLocation(myBasicShader.shaderProgram, "campfireColor"), 1, glm::value_ptr(fireColor));

    renderAllObjects(myBasicShader);
    if (sunLightEnabled) {
        mySkyBox.Draw(skyboxShader, view, projection);
    }
    else {
        myNightSkyBox.Draw(skyboxShader, view, projection);
    }
}
void cleanup() {
    myWindow.Delete();
}

int main(int argc, const char* argv[]) {
    try {
        initOpenGLWindow();
    }
    catch (const std::exception& e) {
        std::cerr << e.what() << std::endl;
        return EXIT_FAILURE;
    }

    initOpenGLState();
    initModels();
    initShaders();
    initUniforms();
    initSkybox();
    initFBO();
    setWindowCallbacks();

    glfwSetInputMode(myWindow.getWindow(), GLFW_CURSOR, GLFW_CURSOR_DISABLED);
    glCheckError();

    initSnowParticles();
    std::cout << "\n=== CONTROALE ===\n";
    std::cout << "WASD - Miscare camera\n";
    std::cout << "Mouse - Rotire camera\n";
    std::cout << "Q/E - Rotire obiect\n";
    std::cout << "P - Tour automat\n";
    std::cout << "F - Fog ON/OFF\n";
    std::cout << "N - Ninsoare ON/OFF\n";
    std::cout << "L - Lanterna ON/OFF\n";
    std::cout << "C - Campfire ON/OFF\n";
    std::cout << "1/2/3 - Mod randare (Solid/Wireframe/Point)\n";
    std::cout << "ESC - Iesire\n";
    std::cout << "==================\n\n";
    float lastFrame = 0.0f;

    while (!glfwWindowShouldClose(myWindow.getWindow())) {
        float currentFrame = glfwGetTime();
        float deltaTime = currentFrame - lastFrame;
        lastFrame = currentFrame;

        updateSnowParticles(deltaTime);
        processMovement();
        renderScene();
        renderSnow();
        glfwPollEvents();
        glfwSwapBuffers(myWindow.getWindow());
        glCheckError();
    }

    cleanup();
    return EXIT_SUCCESS;
}