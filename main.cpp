#if defined (__APPLE__)
#define GLFW_INCLUDE_GLCOREARB
#define GL_SILENCE_DEPRECATION
#else
#define GLEW_STATIC
#include <GL/glew.h>
#endif

#include <GLFW/glfw3.h>

#include <glm/glm.hpp> //core glm functionality
#include <glm/gtc/matrix_transform.hpp> //glm extension for generating common transformation matrices
#include <glm/gtc/matrix_inverse.hpp> //glm extension for computing inverse matrices
#include <glm/gtc/type_ptr.hpp> //glm extension for accessing the internal data structure of glm types

#include "Window.h"
#include "Shader.hpp"
#include "Camera.hpp"
#include "Model3D.hpp"
#include "SkyBox.hpp"

#include <iostream>

// window
gps::Window myWindow;

// matrices
glm::mat4 model;
glm::mat4 view;
glm::mat4 projection;
glm::mat3 normalMatrix;

// light parameters
glm::vec3 lightDir;
glm::vec3 lightColor;

// NOU: Point light parameters
glm::vec3 pointLightPos;
glm::vec3 pointLightColor;

// shader uniform locations
GLint modelLoc;
GLint viewLoc;
GLint projectionLoc;
GLint normalMatrixLoc;
GLint lightDirLoc;
GLint lightColorLoc;

// camera
gps::Camera myCamera(
    glm::vec3(0.0f, 0.0f, 3.0f),
    glm::vec3(0.0f, 0.0f, -10.0f),
    glm::vec3(0.0f, 1.0f, 0.0f));

GLfloat cameraSpeed = 0.5f;

GLboolean pressedKeys[1024];

// models
gps::Model3D teapot;
gps::Model3D ground;
gps::Model3D watchTower;
gps::Model3D house;
gps::Model3D trees;
gps::Model3D fence;
gps::Model3D big_tree;
gps::Model3D big_tree2;
gps::Model3D big_tree3;
gps::Model3D lantern;
gps::Model3D bear;

gps::Model3D windmillBase;
gps::Model3D windmillBlades;
float bladesAngle = 0.0f;

gps::Model3D well;
gps::Model3D casuta;

GLfloat angle;

// NOU: Rendering mode
enum RenderMode {
    SOLID = 0,
    WIREFRAME = 1,
    POINT = 2
};
RenderMode currentRenderMode = SOLID;

// shaders
gps::Shader myBasicShader;

// skybox
gps::SkyBox mySkyBox;
gps::Shader skyboxShader;

// Mouse control variables
GLboolean firstMouse = true;
GLfloat lastX = 400, lastY = 300;
GLfloat yaw = -90.0f;
GLfloat pitch = 0.0f;
GLfloat sensitivity = 0.1f;

bool fogEnabled = false;
GLint fogEnabledLoc;

GLuint shadowMapFBO;
GLuint depthMapTexture;
gps::Shader depthMapShader;
const unsigned int SHADOW_WIDTH = 4096, SHADOW_HEIGHT = 4096;

bool startTour = false;
float tourAngle = 0.0f;

GLenum glCheckError_(const char* file, int line)
{
    GLenum errorCode;
    while ((errorCode = glGetError()) != GL_NO_ERROR) {
        std::string error;
        switch (errorCode) {
        case GL_INVALID_ENUM:
            error = "INVALID_ENUM";
            break;
        case GL_INVALID_VALUE:
            error = "INVALID_VALUE";
            break;
        case GL_INVALID_OPERATION:
            error = "INVALID_OPERATION";
            break;
        case GL_OUT_OF_MEMORY:
            error = "OUT_OF_MEMORY";
            break;
        case GL_INVALID_FRAMEBUFFER_OPERATION:
            error = "INVALID_FRAMEBUFFER_OPERATION";
            break;
        }
        std::cout << error << " | " << file << " (" << line << ")" << std::endl;
    }
    return errorCode;
}
#define glCheckError() glCheckError_(__FILE__, __LINE__)

void windowResizeCallback(GLFWwindow* window, int width, int height) {
    fprintf(stdout, "Window resized! New width: %d , and height: %d\n", width, height);
    //TODO
}

void keyboardCallback(GLFWwindow* window, int key, int scancode, int action, int mode) {
    if (key == GLFW_KEY_ESCAPE && action == GLFW_PRESS) {
        glfwSetWindowShouldClose(window, GL_TRUE);
    }

    if (key >= 0 && key < 1024) {
        if (action == GLFW_PRESS) {
            pressedKeys[key] = true;
        }
        else if (action == GLFW_RELEASE) {
            pressedKeys[key] = false;
        }
    }

    if (action == GLFW_PRESS) {
        if (key == GLFW_KEY_1) {
            currentRenderMode = SOLID;
            glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            std::cout << "Render Mode: SOLID" << std::endl;
        }
        else if (key == GLFW_KEY_2) {
            currentRenderMode = WIREFRAME;
            glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            std::cout << "Render Mode: WIREFRAME" << std::endl;
        }
        else if (key == GLFW_KEY_3) {
            currentRenderMode = POINT;
            glPolygonMode(GL_FRONT_AND_BACK, GL_POINT);
            glPointSize(3.0f); 
            std::cout << "Render Mode: POINT" << std::endl;
        }
        else if (key == GLFW_KEY_F) {
            fogEnabled = !fogEnabled;

            myBasicShader.useShaderProgram();
            glUniform1i(fogEnabledLoc, fogEnabled);

            std::cout << "Fog: " << (fogEnabled ? "ON" : "OFF") << std::endl;
        }
    }

    if (key == GLFW_KEY_P && action == GLFW_PRESS) {
        pressedKeys[GLFW_KEY_P] = true;
    }
}

void mouseCallback(GLFWwindow* window, double xpos, double ypos) {
    if (firstMouse)
    {
        lastX = xpos;
        lastY = ypos;
        firstMouse = false;
    }

    GLfloat xoffset = xpos - lastX;
    GLfloat yoffset = lastY - ypos;

    lastX = xpos;
    lastY = ypos;

    xoffset *= sensitivity;
    yoffset *= sensitivity;

    yaw += xoffset;
    pitch += yoffset;

    if (pitch > 89.0f)
        pitch = 89.0f;
    if (pitch < -89.0f)
        pitch = -89.0f;

    myCamera.rotate(pitch, yaw);

    view = myCamera.getViewMatrix();
    myBasicShader.useShaderProgram();
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
    // compute normal matrix for teapot
    normalMatrix = glm::mat3(glm::inverseTranspose(view * model));
}

void processMovement() {

    const GLfloat MIN_CAMERA_HEIGHT = 0.0f;

    if (pressedKeys[GLFW_KEY_P]) {
        startTour = !startTour;
        pressedKeys[GLFW_KEY_P] = false;
    }

    if (startTour) {
        tourAngle += 0.5f;
        float radius = 30.0f;
        float camX = sin(glm::radians(tourAngle)) * radius;
        float camZ = cos(glm::radians(tourAngle)) * radius;
        myCamera.setPosition(glm::vec3(camX, 10.0f, camZ));
        view = glm::lookAt(glm::vec3(camX, 10.0f, camZ), glm::vec3(0.0f, 2.0f, 0.0f), glm::vec3(0.0f, 1.0f, 0.0f));
        myBasicShader.useShaderProgram();
        glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
        normalMatrix = glm::mat3(glm::inverseTranspose(view * model));
    }
    else {
        if (pressedKeys[GLFW_KEY_W]) {
            myCamera.move(gps::MOVE_FORWARD, cameraSpeed);
            //update view matrix
            view = myCamera.getViewMatrix();
            myBasicShader.useShaderProgram();
            glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
            // compute normal matrix for teapot
            normalMatrix = glm::mat3(glm::inverseTranspose(view * model));
        }

        if (pressedKeys[GLFW_KEY_S]) {
            myCamera.move(gps::MOVE_BACKWARD, cameraSpeed);
            //update view matrix
            view = myCamera.getViewMatrix();
            myBasicShader.useShaderProgram();
            glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
            // compute normal matrix for teapot
            normalMatrix = glm::mat3(glm::inverseTranspose(view * model));
        }

        if (pressedKeys[GLFW_KEY_A]) {
            myCamera.move(gps::MOVE_LEFT, cameraSpeed);
            //update view matrix
            view = myCamera.getViewMatrix();
            myBasicShader.useShaderProgram();
            glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
            // compute normal matrix for teapot
            normalMatrix = glm::mat3(glm::inverseTranspose(view * model));
        }

        if (pressedKeys[GLFW_KEY_D]) {
            myCamera.move(gps::MOVE_RIGHT, cameraSpeed);
            //update view matrix
            view = myCamera.getViewMatrix();
            myBasicShader.useShaderProgram();
            glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
            // compute normal matrix for teapot
            normalMatrix = glm::mat3(glm::inverseTranspose(view * model));
        }

        if (pressedKeys[GLFW_KEY_Q]) {
            angle -= 1.0f;
            // update model matrix for teapot
            model = glm::rotate(glm::mat4(1.0f), glm::radians(angle), glm::vec3(0, 1, 0));
            // update normal matrix for teapot
            normalMatrix = glm::mat3(glm::inverseTranspose(view * model));
        }

        if (pressedKeys[GLFW_KEY_E]) {
            angle += 1.0f;
            // update model matrix for teapot
            model = glm::rotate(glm::mat4(1.0f), glm::radians(angle), glm::vec3(0, 1, 0));
            // update normal matrix for teapot
            normalMatrix = glm::mat3(glm::inverseTranspose(view * model));
        }

        glm::vec3 currentPosition = myCamera.getPosition();

        if (currentPosition.y < MIN_CAMERA_HEIGHT) {
            myCamera.setPosition(glm::vec3(currentPosition.x, MIN_CAMERA_HEIGHT, currentPosition.z));


            view = myCamera.getViewMatrix();
            myBasicShader.useShaderProgram();
            glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));

            normalMatrix = glm::mat3(glm::inverseTranspose(view * model));
            glUniformMatrix3fv(normalMatrixLoc, 1, GL_FALSE, glm::value_ptr(normalMatrix));
        }
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
    glEnable(GL_DEPTH_TEST); // enable depth-testing
    glDepthFunc(GL_LESS); // depth-testing interprets a smaller value as "closer"
    glEnable(GL_CULL_FACE); // cull face
    glCullFace(GL_BACK); // cull back face
    glFrontFace(GL_CCW); // GL_CCW for counter clock-wise
}

void initFBO() {
    glGenFramebuffers(1, &shadowMapFBO);
    glGenTextures(1, &depthMapTexture);
    glBindTexture(GL_TEXTURE_2D, depthMapTexture);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT,
        SHADOW_WIDTH, SHADOW_HEIGHT, 0, GL_DEPTH_COMPONENT, GL_FLOAT, NULL);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    float borderColor[] = { 1.0f, 1.0f, 1.0f, 1.0f };
    glTexParameterfv(GL_TEXTURE_2D, GL_TEXTURE_BORDER_COLOR, borderColor);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_BORDER);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_BORDER);

    glBindFramebuffer(GL_FRAMEBUFFER, shadowMapFBO);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, depthMapTexture, 0);

    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);
}

glm::mat4 computeLightSpaceTrMatrix() {
    glm::mat4 lightProjection = glm::ortho(-150.0f, 150.0f, -150.0f, 150.0f, 0.1f, 400.0f);
    glm::vec3 lightPos = normalize(lightDir) * 150.0f;
    glm::vec3 target = glm::vec3(0.0f, 0.0f, 50.0f);
    glm::mat4 lightView = glm::lookAt(lightPos, target, glm::vec3(0.0f, 1.0f, 0.0f));
    return lightProjection * lightView;
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
}

void initShaders() {
    myBasicShader.loadShader(
        "shaders/basic.vert",
        "shaders/basic.frag");

    depthMapShader.loadShader(
        "shaders/depthMap.vert", 
        "shaders/depthMap.frag");
}

void initUniforms() {
    myBasicShader.useShaderProgram();
    model = glm::rotate(glm::mat4(1.0f), glm::radians(angle), glm::vec3(0.0f, 1.0f, 0.0f));
    modelLoc = glGetUniformLocation(myBasicShader.shaderProgram, "model");
    view = myCamera.getViewMatrix();
    viewLoc = glGetUniformLocation(myBasicShader.shaderProgram, "view");
    glUniformMatrix4fv(viewLoc, 1, GL_FALSE, glm::value_ptr(view));
    normalMatrix = glm::mat3(glm::inverseTranspose(view * model));
    normalMatrixLoc = glGetUniformLocation(myBasicShader.shaderProgram, "normalMatrix");
    projection = glm::perspective(glm::radians(45.0f),
        (float)myWindow.getWindowDimensions().width / (float)myWindow.getWindowDimensions().height,
        0.1f, 200.0f);
    projectionLoc = glGetUniformLocation(myBasicShader.shaderProgram, "projection");
    glUniformMatrix4fv(projectionLoc, 1, GL_FALSE, glm::value_ptr(projection));

    lightDir = glm::vec3(0.0f, 20.0f, -10.0f);
    lightDirLoc = glGetUniformLocation(myBasicShader.shaderProgram, "lightDir");
    glUniform3fv(lightDirLoc, 1, glm::value_ptr(lightDir));

    lightColor = glm::vec3(1.0f, 1.0f, 1.0f); //white light
    lightColorLoc = glGetUniformLocation(myBasicShader.shaderProgram, "lightColor");
    glUniform3fv(lightColorLoc, 1, glm::value_ptr(lightColor));

    pointLightColor = glm::vec3(1.0f, 0.6f, 0.0f);
    glUniform3fv(glGetUniformLocation(myBasicShader.shaderProgram, "pointLightColor"), 1, glm::value_ptr(pointLightColor));

    fogEnabledLoc = glGetUniformLocation(myBasicShader.shaderProgram, "enableFog");
    glUniform1i(fogEnabledLoc, fogEnabled);
}

void renderTeapot(gps::Shader shader) {
    shader.useShaderProgram();
    glm::mat4 scaleMatrix = glm::mat4(1.0f);
    float scaleFactor = 0.25f;
    scaleMatrix = glm::scale(scaleMatrix, glm::vec3(scaleFactor, scaleFactor, scaleFactor));

    glm::mat4 rotationMatrix = glm::rotate(glm::mat4(1.0f), glm::radians(angle), glm::vec3(0.0f, 1.0f, 0.0f));

    model = rotationMatrix * scaleMatrix;

    normalMatrix = glm::mat3(glm::inverseTranspose(view * model));

    glUniformMatrix4fv(glGetUniformLocation(shader.shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix3fv(glGetUniformLocation(shader.shaderProgram, "normalMatrix"), 1, GL_FALSE, glm::value_ptr(normalMatrix));
    // draw teapot
    teapot.Draw(shader);
}

void renderGround(gps::Shader shader) {
    shader.useShaderProgram();

    // Assuming the ground model is centered at (0, 0, 0) and needs to be lowered.
    model = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -1.0f, 0.0f));

    // 2. Compute Normal Matrix
    // The Normal Matrix depends on the current view and model matrices.
    normalMatrix = glm::mat3(glm::inverseTranspose(view * model));

    // 3. Send uniforms to the shader
    glUniformMatrix4fv(glGetUniformLocation(shader.shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix3fv(glGetUniformLocation(shader.shaderProgram, "normalMatrix"), 1, GL_FALSE, glm::value_ptr(normalMatrix));
    // 4. Draw the ground
    ground.Draw(shader);
}

void renderWatchTower(gps::Shader shader) {
    shader.useShaderProgram();

    model = glm::translate(glm::mat4(1.0f), glm::vec3(2.0f, -1.0f, -3.0f));

    model = glm::scale(model, glm::vec3(1.0f, 1.0f, 1.0f));
    normalMatrix = glm::mat3(glm::inverseTranspose(view * model));
    glUniformMatrix4fv(glGetUniformLocation(shader.shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix3fv(glGetUniformLocation(shader.shaderProgram, "normalMatrix"), 1, GL_FALSE, glm::value_ptr(normalMatrix));
    watchTower.Draw(shader);
}

void renderHouse(gps::Shader shader) {
    shader.useShaderProgram();
    model = glm::translate(glm::mat4(1.0f), glm::vec3(-1.0f, -0.8f, -1.0f));
    model = glm::scale(model, glm::vec3(1.0f, 1.0f, 1.0f));
    normalMatrix = glm::mat3(glm::inverseTranspose(view * model));
    glUniformMatrix4fv(glGetUniformLocation(shader.shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix3fv(glGetUniformLocation(shader.shaderProgram, "normalMatrix"), 1, GL_FALSE, glm::value_ptr(normalMatrix));
    house.Draw(shader);
}

void renderFence(gps::Shader shader) {
    shader.useShaderProgram();
    model = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -1.0f, 0.0f));
    model = glm::scale(model, glm::vec3(1.0f, 1.0f, 1.0f));
    normalMatrix = glm::mat3(glm::inverseTranspose(view * model));
    glUniformMatrix4fv(glGetUniformLocation(shader.shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix3fv(glGetUniformLocation(shader.shaderProgram, "normalMatrix"), 1, GL_FALSE, glm::value_ptr(normalMatrix));
    fence.Draw(shader);
}

void renderTrees(gps::Shader shader) {
    shader.useShaderProgram();
    model = glm::translate(glm::mat4(1.0f), glm::vec3(-2.0f, -1.0f, -2.0f));
    model = glm::scale(model, glm::vec3(1.0f, 1.0f, 1.0f));
    normalMatrix = glm::mat3(glm::inverseTranspose(view * model));
    glUniformMatrix4fv(glGetUniformLocation(shader.shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix3fv(glGetUniformLocation(shader.shaderProgram, "normalMatrix"), 1, GL_FALSE, glm::value_ptr(normalMatrix));
    trees.Draw(shader);
}

void renderBigTree(gps::Shader shader) {
    shader.useShaderProgram();
    model = glm::translate(glm::mat4(1.0f), glm::vec3(3.0f, -1.0f, -4.0f));
    model = glm::scale(model, glm::vec3(1.0f, 1.0f, 1.0f));
    normalMatrix = glm::mat3(glm::inverseTranspose(view * model));
    glUniformMatrix4fv(glGetUniformLocation(shader.shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix3fv(glGetUniformLocation(shader.shaderProgram, "normalMatrix"), 1, GL_FALSE, glm::value_ptr(normalMatrix));
    big_tree.Draw(shader);
}

void renderBigTree2(gps::Shader shader) {
    shader.useShaderProgram();
    model = glm::translate(glm::mat4(1.0f), glm::vec3(-3.0f, -1.0f, -4.0f));
    model = glm::scale(model, glm::vec3(1.0f, 1.0f, 1.0f));
    normalMatrix = glm::mat3(glm::inverseTranspose(view * model));
    glUniformMatrix4fv(glGetUniformLocation(shader.shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix3fv(glGetUniformLocation(shader.shaderProgram, "normalMatrix"), 1, GL_FALSE, glm::value_ptr(normalMatrix));
    big_tree2.Draw(shader);
}

void renderBigTree3(gps::Shader shader) {
    shader.useShaderProgram();
    model = glm::translate(glm::mat4(1.0f), glm::vec3(0.0f, -1.0f, -5.0f));
    model = glm::scale(model, glm::vec3(1.0f, 1.0f, 1.0f));
    normalMatrix = glm::mat3(glm::inverseTranspose(view * model));
    glUniformMatrix4fv(glGetUniformLocation(shader.shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(model));
    glUniformMatrix3fv(glGetUniformLocation(shader.shaderProgram, "normalMatrix"), 1, GL_FALSE, glm::value_ptr(normalMatrix));
    big_tree3.Draw(shader);
}

void renderWindmill(gps::Shader shader) {
    shader.useShaderProgram();
    glm::vec3 windmillPos = glm::vec3(20.0f, 20.0f, 100.0f);

    glm::mat4 modelBase = glm::mat4(1.0f);
    modelBase = glm::translate(modelBase, windmillPos);
    modelBase = glm::scale(modelBase, glm::vec3(0.5f, 0.5f, 0.5f));

    glUniformMatrix4fv(glGetUniformLocation(shader.shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(modelBase));

    if (glGetUniformLocation(shader.shaderProgram, "normalMatrix") != -1) {
        normalMatrix = glm::mat3(glm::inverseTranspose(view * modelBase));
        glUniformMatrix3fv(glGetUniformLocation(shader.shaderProgram, "normalMatrix"), 1, GL_FALSE, glm::value_ptr(normalMatrix));
    }
    windmillBase.Draw(shader);

    glm::mat4 modelBlades = glm::mat4(1.0f);

    modelBlades = glm::translate(modelBlades, windmillPos);

    float inaltimeTurn = 4.0f; 
    modelBlades = glm::translate(modelBlades, glm::vec3(0.0f, inaltimeTurn, -2.8f));

    bladesAngle += 1.0f;
    modelBlades = glm::rotate(modelBlades, glm::radians(bladesAngle), glm::vec3(0.0f, 0.0f, 1.0f));

    modelBlades = glm::scale(modelBlades, glm::vec3(0.5f, 0.5f, 0.5f));

    glUniformMatrix4fv(glGetUniformLocation(shader.shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(modelBlades));

    if (glGetUniformLocation(shader.shaderProgram, "normalMatrix") != -1) {
        normalMatrix = glm::mat3(glm::inverseTranspose(view * modelBlades));
        glUniformMatrix3fv(glGetUniformLocation(shader.shaderProgram, "normalMatrix"), 1, GL_FALSE, glm::value_ptr(normalMatrix));
    }
    windmillBlades.Draw(shader);
}

void renderLantern(gps::Shader shader) {
    shader.useShaderProgram();
    glm::vec3 lanternPos = glm::vec3(-7.0f, -0.4f, -1.0f);
    glm::mat4 modelLantern = glm::mat4(1.0f);
    modelLantern = glm::translate(modelLantern, lanternPos);
    modelLantern = glm::scale(modelLantern, glm::vec3(0.5f, 0.5f, 0.5f));
    glUniformMatrix4fv(glGetUniformLocation(shader.shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(modelLantern));
    if (glGetUniformLocation(shader.shaderProgram, "normalMatrix") != -1) {
        normalMatrix = glm::mat3(glm::inverseTranspose(view * modelLantern));
        glUniformMatrix3fv(glGetUniformLocation(shader.shaderProgram, "normalMatrix"), 1, GL_FALSE, glm::value_ptr(normalMatrix));
    }
    lantern.Draw(shader);
}

void renderWell(gps::Shader shader) {
    shader.useShaderProgram();
    glm::mat4 modelWell = glm::mat4(1.0f);
    modelWell = glm::translate(modelWell, glm::vec3(5.0f, -1.0f, 5.0f));
    modelWell = glm::scale(modelWell, glm::vec3(1.0f, 1.0f, 1.0f));
    glUniformMatrix4fv(glGetUniformLocation(shader.shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(modelWell));
    if (glGetUniformLocation(shader.shaderProgram, "normalMatrix") != -1) {
        normalMatrix = glm::mat3(glm::inverseTranspose(view * modelWell));
        glUniformMatrix3fv(glGetUniformLocation(shader.shaderProgram, "normalMatrix"), 1, GL_FALSE, glm::value_ptr(normalMatrix));
    }
    well.Draw(shader);
}

void renderCasuta(gps::Shader shader) {
    shader.useShaderProgram();
    glm::mat4 modelCasuta = glm::mat4(1.0f);
    modelCasuta = glm::translate(modelCasuta, glm::vec3(-5.0f, -3.0f, 5.0f));
    modelCasuta = glm::scale(modelCasuta, glm::vec3(1.0f, 1.0f, 1.0f));
    glUniformMatrix4fv(glGetUniformLocation(shader.shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(modelCasuta));
    if (glGetUniformLocation(shader.shaderProgram, "normalMatrix") != -1) {
        normalMatrix = glm::mat3(glm::inverseTranspose(view * modelCasuta));
        glUniformMatrix3fv(glGetUniformLocation(shader.shaderProgram, "normalMatrix"), 1, GL_FALSE, glm::value_ptr(normalMatrix));
    }
    casuta.Draw(shader);
}

void renderBear(gps::Shader shader) {
    shader.useShaderProgram();
    glm::mat4 modelBear = glm::mat4(1.0f);
    modelBear = glm::translate(modelBear, glm::vec3(0.0f, -0.05f, -3.0f));
    modelBear = glm::scale(modelBear, glm::vec3(0.5f, 0.5f, 0.5f));
    glUniformMatrix4fv(glGetUniformLocation(shader.shaderProgram, "model"), 1, GL_FALSE, glm::value_ptr(modelBear));
    if (glGetUniformLocation(shader.shaderProgram, "normalMatrix") != -1) {
        normalMatrix = glm::mat3(glm::inverseTranspose(view * modelBear));
        glUniformMatrix3fv(glGetUniformLocation(shader.shaderProgram, "normalMatrix"), 1, GL_FALSE, glm::value_ptr(normalMatrix));
    }
    bear.Draw(shader);
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

    renderTeapot(depthMapShader);
    renderGround(depthMapShader);
    renderWatchTower(depthMapShader);
    renderHouse(depthMapShader);
    renderTrees(depthMapShader);
    renderFence(depthMapShader);
    renderBigTree(depthMapShader);
    renderBigTree2(depthMapShader);
    renderBigTree3(depthMapShader);
    renderWindmill(depthMapShader);
    renderLantern(depthMapShader);
	renderWell(depthMapShader);
	renderCasuta(depthMapShader);
	renderBear(depthMapShader);

    glCullFace(GL_BACK);
    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    glViewport(0, 0, myWindow.getWindowDimensions().width, myWindow.getWindowDimensions().height);
    myBasicShader.useShaderProgram();

    glUniformMatrix4fv(glGetUniformLocation(myBasicShader.shaderProgram, "lightSpaceTrMatrix"),
        1, GL_FALSE, glm::value_ptr(computeLightSpaceTrMatrix()));

    glActiveTexture(GL_TEXTURE3);
    glBindTexture(GL_TEXTURE_2D, depthMapTexture);
    glUniform1i(glGetUniformLocation(myBasicShader.shaderProgram, "shadowMap"), 3);

    glm::vec3 lanternPos = glm::vec3(-13.0f, -0.4f, -4.0f);

    glm::vec3 lightSourcePos = lanternPos + glm::vec3(0.0f, 1.0f, 0.0f); // +1.0f pe Y

    glm::vec3 lightPosEye = glm::vec3(view * glm::vec4(lightSourcePos, 1.0f));

    glUniform3fv(glGetUniformLocation(myBasicShader.shaderProgram, "pointLightPos"), 1, glm::value_ptr(lightPosEye));

    renderTeapot(myBasicShader);
    renderGround(myBasicShader);
    renderWatchTower(myBasicShader);
    renderHouse(myBasicShader);
    renderTrees(myBasicShader);
    renderFence(myBasicShader);
    renderBigTree(myBasicShader);
    renderBigTree2(myBasicShader);
    renderBigTree3(myBasicShader);
    renderWindmill(myBasicShader);
    renderLantern(myBasicShader);
	renderWell(myBasicShader);
	renderCasuta(myBasicShader);
	renderBear(myBasicShader);
    mySkyBox.Draw(skyboxShader, view, projection);
}
void cleanup() {
    myWindow.Delete();
    //cleanup code for your own data
}

void initSkybox() {
    std::vector<const GLchar*> faces;
    faces.push_back("skybox/right.tga");
    faces.push_back("skybox/left.tga");
    faces.push_back("skybox/top.tga");
    faces.push_back("skybox/bottom.tga");
    faces.push_back("skybox/back.tga");
    faces.push_back("skybox/front.tga");
    mySkyBox.Load(faces);

    skyboxShader.loadShader("shaders/skyboxShader.vert", "shaders/skyboxShader.frag");
    skyboxShader.useShaderProgram();
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
    // application loop
    while (!glfwWindowShouldClose(myWindow.getWindow())) {
        processMovement();
        renderScene();

        glfwPollEvents();
        glfwSwapBuffers(myWindow.getWindow());

        glCheckError();
    }

    cleanup();

    return EXIT_SUCCESS;
}