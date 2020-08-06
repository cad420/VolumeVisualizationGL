#pragma once
#include <GL/gl3w.h>
#include <GLFW/glfw3.h>
#include "GLContext.hpp"

#include <memory>
#include <vector>
#include <iostream>
#include <cstdio>

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
        glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 5);
        glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
        glfwWindowHint(GLFW_DOUBLEBUFFER,true);

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
        return true;
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
        return KeyButton::Key_Unknown;
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
    const GLMAXINTEGER & MaxInteger()const{

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
private:
    static GLMAXINTEGER maxInteger;
    static std::vector<std::string> extensions;
    static void GetOpenGLExtensions()
    {
        auto count = 0;
        GL_EXPR( glGetIntegerv( GL_NUM_EXTENSIONS, &count ) );
        for ( int i = 0; i < count; ++i )
            extensions.emplace_back( (char *)( glGetStringi( GL_EXTENSIONS, i ) ) );
    }
    static void InitMaxInteger()
    {
        GL_EXPR( glGetIntegerv( GL_MAX_VERTEX_ATTRIBS, &maxInteger.MAX_VERTEX_ATTRIBS ) );
        GL_EXPR( glGetIntegerv( GL_MAX_TEXTURE_IMAGE_UNITS, &maxInteger.MAX_TEXTURE_IMAGE_UNITE ) );
        GL_EXPR( glGetIntegerv( GL_MAX_SHADER_STORAGE_BUFFER_BINDINGS, &maxInteger.MAX_SHADER_STORAGE_BINDINGS ) );
        GL_EXPR( glGetIntegerv( GL_MAX_ATOMIC_COUNTER_BUFFER_BINDINGS, &maxInteger.MAX_ATOMIC_COUNTER_BUFFER_BINDINGS ) );
        GL_EXPR( glGetIntegerv( GL_MAX_IMAGE_UNITS, &maxInteger.MAX_IMAGE_UNITS ) );
        GL_EXPR( glGetIntegerv( GL_MAX_3D_TEXTURE_SIZE, &maxInteger.MAX_3DTEXUTRE_SIZE ) );
        GL_EXPR( glGetIntegerv( GL_MAX_COMBINED_UNIFORM_BLOCKS, &maxInteger.MAX_UNIFORM_BLOCKS_COUNT ) );

        if ( CheckSupportForExtension( "GL_NVX_gpu_memory_info" ) ) {  // NVDIA GPU
            constexpr int GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX = 0x9047;		   // dedicated video memory, total size (in kb) of the GPU memory
            constexpr int GPU_MEMORY_INFO_TOTAL_AVAILABEL_MEMORY_NVX = 0x9048;	 // total available memory, total size (in Kb) of the memory available for allocations
            constexpr int GPU_MEMORY_INFO_CURRENT_AVAILABEL_VIDEMEM_NVX = 0x9049;  //current available dedicated video memory (in kb), currently unused GPU memory
            GL_EXPR( glGetIntegerv( GPU_MEMORY_INFO_TOTAL_AVAILABEL_MEMORY_NVX, &maxInteger.MAX_GPU_MEMORY_SIZE ) );

        } else if ( CheckSupportForExtension( "GL_ATI_meminfo" ) ) {   // AMD GPU
            constexpr int GPU_VBO_FREE_MEMORY_ATI = 0x87FB;
            constexpr int GPU_TEXTURE_FREE_MEMORY_ATI = 0x87FC;
            constexpr int GPU_RENDERBUFFER_FREE_MEMORY_ATI = 0x87FD;
            int texFreeMem = 0, vboFreeMem = 0, renderBufferFreeMem = 0;
            GL_EXPR( glGetIntegerv( GPU_TEXTURE_FREE_MEMORY_ATI, &texFreeMem ) );
            GL_EXPR( glGetIntegerv( GPU_VBO_FREE_MEMORY_ATI, &vboFreeMem ) );
            GL_EXPR( glGetIntegerv( GPU_RENDERBUFFER_FREE_MEMORY_ATI, &renderBufferFreeMem ) );
            maxInteger.MAX_GPU_MEMORY_SIZE = texFreeMem + vboFreeMem + renderBufferFreeMem;
        }

        printf( "MAX_VERTEX_ATTRIBS:%d\n", maxInteger.MAX_VERTEX_ATTRIBS );
        printf( "MAX_TEXTURE_IMAGE_UNITE:%d\n", maxInteger.MAX_TEXTURE_IMAGE_UNITE );
        printf( "MAX_SHADER_STORAGE_BINDINGS:%d\n", maxInteger.MAX_SHADER_STORAGE_BINDINGS );
        printf( "MAX_ATOMIC_COUNTER_BUFFER_BINDINGS:%d\n", maxInteger.MAX_ATOMIC_COUNTER_BUFFER_BINDINGS );
        printf( "MAX_IMAGE_UNITS:%d\n", maxInteger.MAX_IMAGE_UNITS );
        printf( "MAX_3DTEXTURE_SIZE:%d\n", maxInteger.MAX_3DTEXUTRE_SIZE );
        printf( "MAX_UNIFORM_BLOCKS_COUNT:%d\n", maxInteger.MAX_UNIFORM_BLOCKS_COUNT );
        printf( "MAX_GPU_MEMORY_SIZE:%d", maxInteger.MAX_GPU_MEMORY_SIZE );

    }

public:
    GL3WImpl(){
        Init();
        GetOpenGLExtensions();
        InitMaxInteger();
        }
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

    const GLMAXINTEGER & GetGLProperties(){
        return maxInteger;
    }
    
    
    static bool CheckSupportForExtension( const std::string &ext )
    {
        for ( const auto &e : extensions )
            if ( ext == e )
                return true;
        return false;
    }
};
GLMAXINTEGER GL3WImpl::maxInteger = GLMAXINTEGER();
std::vector<std::string> GL3WImpl::extensions = std::vector<std::string>();
