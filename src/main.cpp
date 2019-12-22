#include <stdio.h>
#define GLAD_DEBUG
#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <GL/glu.h>

#include <imgui.h>
#include <imgui_impl_glfw.h>
#include <imgui_impl_opengl3.h>

#include <stbi.h>
#include <stb_image_write.h>

GLFWwindow* window;

char buffer[10*1024*1024];

const float quadTexCoordVerts[] = {0,1,  0,0,  1,1,  1,0 };
GLuint quadTexCoordVbo = 0;
GLuint quadTexCoordVao = 0;

GLuint gammaUncorrectedFramebuffer = 0;
GLuint gammaCorrectedFramebuffer = 0;
GLuint gcDownsampledFramebuffer = 0;
GLuint texture = 0;
GLuint gammaCorrectedTexture = 0;
GLuint gammaUncorrectedTexture = 0;
GLuint downsampledTexture = 0;
GLuint gcDownsampledTexture = 0;
int texW = 0, texH = 0;
int downsampledTexW = 0, downsampledTexH = 0;

const char imgProcVertShaderSrc[] = R"GLSL(
#version 330

layout(location = 0) in vec2 a_tc;

out vec2 tc;

void main()
{
    tc = a_tc;
    gl_Position = vec4(2.0*a_tc-1.0, 0.0, 1.0);
}

)GLSL";
GLuint imgProcVertShader;

const char exponentFragShaderSrc[] = R"GLSL(
#version 330
layout(location = 0) out vec4 o_color;

uniform sampler2D u_texture;
uniform float u_exp;

in vec2 tc;

void main()
{
    vec4 c = texture(u_texture, tc);
    c = vec4(pow(c.rgb, vec3(u_exp)), c.a);
    o_color = c;
}
)GLSL";
GLuint exponentFragShader;
GLuint exponentShader;

void glErrorCallback(const char *name, void *funcptr, int len_args, ...) {
    GLenum error_code;
    error_code = glad_glGetError();

    if (error_code != GL_NO_ERROR) {
        fprintf(stderr, "ERROR %s in %s\n", gluErrorString(error_code), name);
    }
}

void downsampleImage(unsigned char* outData, int& outW, int& outH,
    const unsigned char* data, int w, int h, int c, int div)
{
    outW = (w+div-1) / div;
    outH = (h+div-1) / div;
    //return;

    for(int y = 0; y < h / div; y++)
    for(int x = 0; x < w / div; x++)
    for(int ic = 0; ic < c; ic++)
    {
        unsigned val = 0;
        for(int yy = div*y; yy < div*(y+1); yy++)
        for(int xx = div*x; xx < div*(x+1); xx++)
            val += data[ic + c * (xx + yy*w)];
        outData[ic + c * (x + y*outW)] = val / (div*div);
    }

    if(w % div != 0)
    {
        const int x = w/div +1;
        const unsigned nn = div * (w % div);
        for(int y = 0; y < h / div; y++)
        for(int ic = 0; ic < c; ic++)
        {
            unsigned val = 0;
            for(int yy = div*y; yy < div*(y+1); yy++)
            for(int xx = div*(w / div); xx < w; xx++)
                val += data[ic + c * (xx + yy*w)];
            outData[ic + c * (x + y*outW)] = val / nn;
        }
    }

    if(h % div != 0)
    {
        const int y = h/div +1;
        const unsigned nn = div * (h % div);
        for(int x = 0; x < w / div; x++)
        for(int ic = 0; ic < c; ic++)
        {
            unsigned val = 0;
            for(int yy = div * (h/div); yy < h; yy++)
            for(int xx = div*x; div*(x+1); xx++)
                val += data[ic + c * (xx + yy*w)];
            outData[ic + c * (y*outW)] = val / nn;
        }
    }

    if(w % div != 0 && h % div != 0)
    {
        const int x = w/div +1;
        const int y = h/div +1;
        const unsigned nn = (w % div) * (h % div);
        for(int ic = 0; ic < c; ic++)
        {
            unsigned val = 0;
            for(int yy = div * (h/div); yy < h; yy++)
            for(int xx = div * (w/div); xx < w; xx++)
                val += data[ic + c * (xx + yy*w)];
            outData[ic + c * (y*outW)] = val / nn;
        }
    }
}

void saveBoundImage(const char* fileName, int level = 0)
{
    int w, h;
    glGetTexLevelParameteriv(GL_TEXTURE_2D, level, GL_TEXTURE_WIDTH, &w);
    glGetTexLevelParameteriv(GL_TEXTURE_2D, level, GL_TEXTURE_HEIGHT, &h);
    glGetTexImage(GL_TEXTURE_2D, level, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
    stbi_write_png(fileName, w, h, 4, buffer, 4*w);
}

void changeImage(const char* path)
{
    int w, h, c;
    unsigned char* data = stbi_load(path, &w, &h, &c, 4);
    if(data)
    {
        glViewport(0, 0, w, h);
        glScissor(0, 0, w, h);
        glActiveTexture(GL_TEXTURE0);
        glUseProgram(exponentShader);
        glBindVertexArray(quadTexCoordVao);

        glBindFramebuffer(GL_FRAMEBUFFER, gammaUncorrectedFramebuffer);
        glBindTexture(GL_TEXTURE_2D, gammaUncorrectedTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glBindTexture(GL_TEXTURE_2D, texture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, data);
        glGenerateMipmap(GL_TEXTURE_2D);
        glUniform1f(glGetUniformLocation(exponentShader, "u_exp"), 1.f/2.2f);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        glBindFramebuffer(GL_FRAMEBUFFER, gammaCorrectedFramebuffer);
        glBindTexture(GL_TEXTURE_2D, gammaCorrectedTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glBindTexture(GL_TEXTURE_2D, texture);
        glUniform1f(glGetUniformLocation(exponentShader, "u_exp"), 2.2f);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);

        {
        glFlush();
        glBindTexture(GL_TEXTURE_2D, gammaUncorrectedTexture);
        glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, buffer);
        char* outBuffer = buffer + w*h*4;
        downsampleImage((unsigned char*)outBuffer, downsampledTexW, downsampledTexH, (unsigned char*)buffer, w, h, 4, 2);
        glBindTexture(GL_TEXTURE_2D, gcDownsampledTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, downsampledTexW, downsampledTexH, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glBindTexture(GL_TEXTURE_2D, downsampledTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, downsampledTexW, downsampledTexH, 0, GL_RGBA, GL_UNSIGNED_BYTE, outBuffer);
        glBindFramebuffer(GL_FRAMEBUFFER, gcDownsampledFramebuffer);
        glUniform1f(glGetUniformLocation(exponentShader, "u_exp"), 2.2f);
        glViewport(0, 0, downsampledTexW, downsampledTexH);
        glScissor(0, 0, downsampledTexW, downsampledTexH);
        glDrawArrays(GL_TRIANGLE_STRIP, 0, 4);
        glFlush();
        glBindTexture(GL_TEXTURE_2D, gcDownsampledTexture);
        saveBoundImage("good.png");
        glBindTexture(GL_TEXTURE_2D, texture);
        saveBoundImage("mipmapping.png", 1);
        }

        downsampleImage((unsigned char*)buffer, downsampledTexW, downsampledTexH, data, w, h, 4, 2);
        stbi_write_png("bad.png", downsampledTexW, downsampledTexH, 4, buffer, 4*downsampledTexW);
        glBindTexture(GL_TEXTURE_2D, downsampledTexture);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, downsampledTexW, downsampledTexH, 0, GL_RGBA, GL_UNSIGNED_BYTE, buffer);

        stbi_image_free(data);
        texW = w;
        texH = h;
    }
}

void dropFileCallback(GLFWwindow* window, int count, const char** paths)
{
    const char* path = paths[0];
    changeImage(path);
}

template <typename T, int N>
constexpr size_t size(const T (&) [N]) { return N; }

void loadStr(char* buffer, int bufferSize, const char* fileName)
{
    FILE* file = fopen(fileName, "r");
    size_t numRead = fread(buffer, sizeof(char), bufferSize-1, file);
    buffer[numRead] = '\0';
}

template <int N>
void loadStr(char (&buffer)[N], const char* fileName)
{
    loadStr(buffer, N, fileName);
}

GLuint createShader(const char* src, GLenum type)
{
    GLint success;
    GLuint shader = glCreateShader(type);
    glShaderSource(shader, 1, &src, nullptr);
    glCompileShader(shader);
    glGetShaderiv(shader, GL_COMPILE_STATUS, &success);
    if(!success) {
        glGetShaderInfoLog(shader, size(buffer), nullptr, buffer);
        fprintf(stderr, "%s\n", buffer);
    }
    return shader;
}

GLuint createShaderProgram(GLuint vertShader, GLuint fragShader)
{
    GLuint prog = glCreateProgram();
    glAttachShader(prog, vertShader);
    glAttachShader(prog, fragShader);
    glLinkProgram(prog);
    GLint success;
    glGetProgramiv(prog, GL_LINK_STATUS, &success);
    if(success) {
        glGetProgramInfoLog(prog, size(buffer), nullptr, buffer);
        fprintf(stderr, "%s\n", buffer);
    }
    return prog;
}

GLuint processTexture(GLuint inTexture, GLuint outFrameBuffer)
{

}

int main(int argc, char* argv[])
{
    glfwSetErrorCallback(+[](int error, const char* description) {
        fprintf(stderr, "Glfw Error %d: %s\n", error, description);
    });

    if (!glfwInit())
        return 1;

    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 3);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 3);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    glfwWindowHint(GLFW_MAXIMIZED, GLFW_TRUE);

    window = glfwCreateWindow(1280, 720, "test", NULL, NULL);
    if (window == NULL)
        return 1;

    glfwMakeContextCurrent(window);
    glfwSwapInterval(0); // Enable vsync

    if (gladLoadGL() == 0) {
        fprintf(stderr, "Failed to initialize OpenGL loader!\n");
        return 1;
    }
    glad_set_post_callback(glErrorCallback);

    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO(); (void)io;
    //io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard;     // Enable Keyboard Controls
    ImGui::StyleColorsDark();
    ImGui_ImplGlfw_InitForOpenGL(window, true);
    ImGui_ImplOpenGL3_Init();

    glGenTextures(1, &texture);
    glBindTexture(GL_TEXTURE_2D, texture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glGenTextures(1, &gammaCorrectedTexture);
    glBindTexture(GL_TEXTURE_2D, gammaCorrectedTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glGenTextures(1, &gammaUncorrectedTexture);
    glBindTexture(GL_TEXTURE_2D, gammaUncorrectedTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glGenTextures(1, &downsampledTexture);
    glBindTexture(GL_TEXTURE_2D, downsampledTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glGenTextures(1, &gcDownsampledTexture);
    glBindTexture(GL_TEXTURE_2D, gcDownsampledTexture);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);

    glGenFramebuffers(1, &gammaUncorrectedFramebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, gammaUncorrectedFramebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gammaUncorrectedTexture, 0);
    glGenFramebuffers(1, &gammaCorrectedFramebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, gammaCorrectedFramebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gammaCorrectedTexture, 0);
    glGenFramebuffers(1, &gcDownsampledFramebuffer);
    glBindFramebuffer(GL_FRAMEBUFFER, gcDownsampledFramebuffer);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, gcDownsampledTexture, 0);

    glGenVertexArrays(1, &quadTexCoordVao);
    glBindVertexArray(quadTexCoordVao);
    glGenBuffers(1, &quadTexCoordVbo);
    glBindBuffer(GL_ARRAY_BUFFER, quadTexCoordVbo);
    glBufferData(GL_ARRAY_BUFFER, sizeof(quadTexCoordVerts), quadTexCoordVerts, GL_STATIC_DRAW);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, 2*sizeof(float), (void*)0);
    glEnableVertexAttribArray(0);

    imgProcVertShader = createShader(imgProcVertShaderSrc, GL_VERTEX_SHADER);
    exponentFragShader = createShader(exponentFragShaderSrc, GL_FRAGMENT_SHADER);
    exponentShader = createShaderProgram(imgProcVertShader, exponentFragShader);
    glUseProgram(exponentShader);
    glUniform1i(glGetUniformLocation(exponentShader, "u_texture"), 0);

    if(argc > 1)
        changeImage(argv[1]);

    glfwSetDropCallback(window, dropFileCallback);

    while (!glfwWindowShouldClose(window))
    {
        glfwPollEvents();

        ImGui_ImplOpenGL3_NewFrame();
        ImGui_ImplGlfw_NewFrame();
        ImGui::NewFrame();

        //ImGui::ShowDemoWindow();
        ImGui::Begin("test");
        if(texW && texH) {
            ImGui::Text("Normal");
            ImGui::Image((ImTextureID)(intptr_t)texture, {(float)texW, (float)texH},
                {0,0}, {1,1}, {1, 1, 1, 1}, {1, 1, 1, 1});
            ImGui::Text("Gamma uncorrected");
            ImGui::Image((ImTextureID)(intptr_t)gammaUncorrectedTexture, {(float)texW, (float)texH},
                {0,0}, {1,1}, {1, 1, 1, 1}, {1, 1, 1, 1});
            ImGui::Text("Gamma corrected");
            ImGui::Image((ImTextureID)(intptr_t)gammaCorrectedTexture, {(float)texW, (float)texH},
                {0,0}, {1,1}, {1, 1, 1, 1}, {1, 1, 1, 1});
            ImGui::Text("Down sampled");
            ImGui::Image((ImTextureID)(intptr_t)downsampledTexture,
                {(float)downsampledTexW, (float)downsampledTexH},
                {0,0}, {1,1}, {1, 1, 1, 1}, {1, 1, 1, 1}); 
            ImGui::Text("gamma uncorrected - downsampled - gamma corrected");
            ImGui::Image((ImTextureID)(intptr_t)gcDownsampledTexture,
                {(float)downsampledTexW, (float)downsampledTexH},
                {0,0}, {1,1}, {1, 1, 1, 1}, {1, 1, 1, 1}); 
        }
        ImGui::End();

        // Rendering
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        ImGui::Render();
        int display_w, display_h;
        glfwGetFramebufferSize(window, &display_w, &display_h);
        glViewport(0, 0, display_w, display_h);
        glClearColor(0.1, 0.2, 0.1, 1.0);
        glClear(GL_COLOR_BUFFER_BIT);
        ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());

        glfwSwapBuffers(window);
        glfwWaitEventsTimeout(0.01);
    }

    // Cleanup
    ImGui_ImplOpenGL3_Shutdown();
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();

    glfwDestroyWindow(window);
    glfwTerminate();

    return 0;
}