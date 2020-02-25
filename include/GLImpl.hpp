
#pragma once
#include <GL/gl3w.h>
#include <GLFW/glfw3.h>
#include "GLContext.hpp"

#include <memory>
#include <iostream>

class GLFWImpl:public EventListenerTraits
{
    struct GLFWWindowDeleter
    {
        void operator()(GLFWwindow * p)const{
            glfwDestroyWindow(p);
        }
    };
    std::unique_ptr<GLFWwindow,GLFWWindowDeleter> window;
    bool InitGLFW()
    {
        if(glfwInit()== GLFW_FALSE)
        {
            std::cout<<"Failed to init GLFW\n";        
            return false;
        }
        glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 4);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);

        auto pWin = glfwCreateWindow(1024,768,"GLContext",nullptr,nullptr);
        if(pWin == nullptr){
            std::cout<<"Failed to create GLFW window\n";
            exit(-1);
        }
        window.reset(pWin);
        MakeCurrent();
        // Window Size

        glfwSetFramebufferSizeCallback(window.get(), glfwFramebufferSizeCallback);
        glfwSetCursorPosCallback(window.get(), glfwCursorPosCallback);
        glfwSetMouseButtonCallback(window.get(), glfwMouseButtonCallback);
        glfwSetScrollCallback(window.get(), glfwMouseScrollCallback);
        glfwSetKeyCallback(window.get(), glfwKeyCallback);
        glfwSetDropCallback(window.get(), glfwDropFileCallback);

    }
    static KeyButton TranslateKey(int key, int scancode, int mods){
        switch ( key ) 
        {
        case GLFW_KEY_0: return Key_0;
        case GLFW_KEY_1: return Key_1;
        case GLFW_KEY_2: return Key_2;
        case GLFW_KEY_3: return Key_3;
        case GLFW_KEY_4: return Key_4;
        case GLFW_KEY_5: return Key_5;
        case GLFW_KEY_6: return Key_6;
        case GLFW_KEY_7: return Key_7;
        case GLFW_KEY_8: return Key_8;
        case GLFW_KEY_9: return Key_9;
        case GLFW_KEY_A: return Key_A;
        case GLFW_KEY_B: return Key_B;
        case GLFW_KEY_C: return Key_C;
        case GLFW_KEY_D: return Key_D;
        case GLFW_KEY_E: return Key_E;
        case GLFW_KEY_F: return Key_F;
        case GLFW_KEY_G: return Key_G;
        case GLFW_KEY_H: return Key_H;
        case GLFW_KEY_I: return Key_I;
        case GLFW_KEY_J: return Key_J;
        case GLFW_KEY_K: return Key_K;
        case GLFW_KEY_L: return Key_L;
        case GLFW_KEY_M: return Key_M;
        case GLFW_KEY_N: return Key_N;
        case GLFW_KEY_O: return Key_O;
        case GLFW_KEY_P: return Key_P;
        case GLFW_KEY_Q: return Key_Q;
        case GLFW_KEY_R: return Key_R;
        case GLFW_KEY_S: return Key_S;
        case GLFW_KEY_T: return Key_T;
        case GLFW_KEY_U: return Key_U;
        case GLFW_KEY_V: return Key_V;
        case GLFW_KEY_W: return Key_W;
        case GLFW_KEY_X: return Key_X;
        case GLFW_KEY_Y: return Key_Y;
        case GLFW_KEY_Z: return Key_Z;

        case GLFW_KEY_RIGHT: return Key_Right;
        case GLFW_KEY_LEFT: return Key_Left;
        case GLFW_KEY_DOWN: return Key_Down;
        case GLFW_KEY_UP: return Key_Up;

        case GLFW_KEY_KP_0: return Key_0;
        case GLFW_KEY_KP_1: return Key_1;
        case GLFW_KEY_KP_2: return Key_2;
        case GLFW_KEY_KP_3: return Key_3;
        case GLFW_KEY_KP_4: return Key_4;
        case GLFW_KEY_KP_5: return Key_5;
        case GLFW_KEY_KP_6: return Key_6;
        case GLFW_KEY_KP_7: return Key_7;
        case GLFW_KEY_KP_8: return Key_8;
        case GLFW_KEY_KP_9: return Key_9;
        default: std::cout << "Unsupported key\n";
        }
    }

    static void glfwCursorPosCallback(GLFWwindow* window, double xpos, double ypos)
    {
        EventListenerTraits::MouseEvent(window,MouseButton(),EventAction::Move,(int)xpos,(int)ypos);
    }

	static void glfwMouseButtonCallback(GLFWwindow * window, int button, int action, int mods){
        double xpos, ypos;
        glfwGetCursorPos( window, &xpos, &ypos );
        int buttons = 0;
        int ea = 0;
        if(action == GLFW_PRESS)
            ea = EventAction::Press;
        else if(action = GLFW_RELEASE)
            ea = EventAction::Release;
        else if(action = GLFW_REPEAT)
            ea = EventAction::Repeat;
        if(button = GLFW_MOUSE_BUTTON_RIGHT)
            buttons |= MouseButton::Mouse_Right;
        if(button = GLFW_MOUSE_BUTTON_LEFT)
            buttons |= MouseButton::Mouse_Left;
        EventListenerTraits::MouseEvent(window,(MouseButton)buttons,(EventAction)ea,(int)xpos,(int)ypos);
    }
	static void glfwFramebufferSizeCallback(GLFWwindow * window, int width, int height){
        EventListenerTraits::FramebufferResizeEvent(window,width,height);
    }
	static void glfwMouseScrollCallback(GLFWwindow * window, double xoffset, double yoffset){
        EventListenerTraits::ScrollEvent(window,xoffset,yoffset);
    }
    static void glfwKeyCallback(GLFWwindow * window, int key, int scancode, int action, int mods){
        int ea = 0;
        if(action == GLFW_PRESS)
            ea = EventAction::Press;
        else if(action = GLFW_RELEASE)
            ea = EventAction::Release;
        else if(action = GLFW_REPEAT)
            ea = EventAction::Repeat;
        EventListenerTraits::KeyboardEvent(window,TranslateKey(key,scancode,mods),(EventAction)ea);
    }
	static void glfwDropFileCallback(GLFWwindow * window,int count ,const char **df){
        EventListenerTraits::FileDropEvent(window,count,df);
    }
public:
    GLFWImpl()
    {
        InitGLFW();
    }
    GLFWImpl(const GLFWImpl &)=delete;
    GLFWImpl & operator=(const GLFWImpl &)=delete;
    GLFWImpl(GLFWImpl && rhs)noexcept:window(std::move(rhs.window)){}
    GLFWImpl & operator=(GLFWImpl && rhs)noexcept{
        Destroy();
        window = std::move(rhs.window);
        return *this;
    }
    void MakeCurrent()
    {
        glfwMakeContextCurrent(window.get());
    }

    bool HasWindow()const
    {
        return true;
    }

    bool Wait()const
    {
        return glfwWindowShouldClose(window.get());
    }

    void DispatchEvent()
    {
        glfwPollEvents();
    }
    void Present(){
        glfwSwapBuffers(window.get());
    }
    void Destroy()
    {
        window.reset();
        glfwTerminate();
    }
    ~GLFWImpl()
    {
        Destroy();
    }
};

struct GL3WImpl
{
    GL3WImpl(){
        Init();}
    static void Init()
    {
        auto res = gl3wInit();
        if(res != GL3W_OK)
        {
            std::cout<<"Failed to init GL3W\n";
            exit(-1);
        }
        if ( !gl3wIsSupported( 4, 5 ) ) 
        {
            throw std::runtime_error("OpenGL 4.5 or higher is required\n");
        }
    }
};
