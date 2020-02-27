#pragma once
#include <functional>


enum EventAction{
    Press = 0x01,
    Release = 0x02,
    Repeat = 0x03,
    Move= 0x04
};
enum MouseButton
{
	Mouse_Left = 0x01,
	Mouse_Right = 0x02
};
enum KeyButton
{
	Key_0,
	Key_1,
	Key_2,
	Key_3,
	Key_4,
	Key_5,
	Key_6,
	Key_7,
	Key_8,
	Key_9,

	Key_A,
	Key_B,
	Key_C,
	Key_D,
	Key_E,
	Key_F,
	Key_G,
	Key_H,
	Key_I,
	Key_J,
	Key_K,
	Key_L,
	Key_M,
	Key_N,
	Key_O,
	Key_P,
	Key_Q,
	Key_R,
	Key_S,
	Key_T,
	Key_U,
	Key_V,
	Key_W,
	Key_X,
	Key_Y,
	Key_Z,

	Key_Up,
	Key_Down,
	Key_Left,
	Key_Right

};
using MouseEventCallback = std::function<void(void*,MouseButton buttons,EventAction action,int xpos,int ypos)>;
using ScrollEventCallback = std::function<void(void*,int xoffset,int yoffset)>;
using KeyboradEventCallback = std::function<void(void*,KeyButton key,EventAction action)>;
using FileDropEventCallback= std::function<void(void*,int count,const char ** df)>;
using FramebufferResizeEventCallback = std::function<void(void*,int width,int height)>;

struct EventListenerTraits
{
    static MouseEventCallback MouseEvent; 
    static ScrollEventCallback ScrollEvent;
    static KeyboradEventCallback KeyboardEvent;
    static FileDropEventCallback FileDropEvent;
    static FramebufferResizeEventCallback FramebufferResizeEvent;
};

struct GLMAXINTEGER
{
    int MAX_VERTEX_ATTRIBS = -1;
    int MAX_TEXTURE_IMAGE_UNITE = -1;
    int MAX_SHADER_STORAGE_BINDINGS = -1;
    int MAX_ATOMIC_COUNTER_BUFFER_BINDINGS = -1;
    int MAX_IMAGE_UNITS = -1;
    int MAX_3DTEXUTRE_SIZE = -1;
    int MAX_UNIFORM_BLOCKS_COUNT = -1;
    int MAX_GPU_MEMORY_SIZE = -1;
};


MouseEventCallback EventListenerTraits::MouseEvent = MouseEventCallback();
ScrollEventCallback EventListenerTraits::ScrollEvent=ScrollEventCallback();
KeyboradEventCallback EventListenerTraits::KeyboardEvent = KeyboradEventCallback();
FileDropEventCallback EventListenerTraits::FileDropEvent = FileDropEventCallback();
FramebufferResizeEventCallback EventListenerTraits::FramebufferResizeEvent = FramebufferResizeEventCallback();


inline void PrintGLErrorType( GLenum glerr )
{
	std::string error;
	switch ( glerr ) {
	case GL_INVALID_ENUM: error = "INVALID_ENUM"; break;
	case GL_INVALID_VALUE: error = "INVALID_VALUE"; break;
	case GL_INVALID_OPERATION: error = "INVALID_OPERATION"; break;
	case GL_STACK_OVERFLOW: error = "STACK_OVERFLOW"; break;
	case GL_STACK_UNDERFLOW: error = "STACK_UNDERFLOW"; break;
	case GL_OUT_OF_MEMORY: error = "OUT_OF_MEMORY"; break;
	case GL_INVALID_FRAMEBUFFER_OPERATION: error = "INVALID_FRAMEBUFFER_OPERATION"; break;
	default: error = "UNKNOWN_ERROR"; break;
	}
	std::cout<<error<<std::endl;
}

inline GLenum PrintGLErrorMsg( const char *file, int line )
{
	GLenum errorCode;
	while ( ( errorCode = glGetError() ) != GL_NO_ERROR ) {
		std::string error;
		switch ( errorCode ) {
		case GL_INVALID_ENUM: error = "INVALID_ENUM"; break;
		case GL_INVALID_VALUE: error = "INVALID_VALUE"; break;
		case GL_INVALID_OPERATION: error = "INVALID_OPERATION"; break;
		case GL_STACK_OVERFLOW: error = "STACK_OVERFLOW"; break;
		case GL_STACK_UNDERFLOW: error = "STACK_UNDERFLOW"; break;
		case GL_OUT_OF_MEMORY: error = "OUT_OF_MEMORY"; break;
		case GL_INVALID_FRAMEBUFFER_OPERATION: error = "INVALID_FRAMEBUFFER_OPERATION"; break;
		}
        std::cout<<error.c_str()<<" | "<<file<<" "<<line<<std::endl;
	}
	return errorCode;
}

#ifdef NDEBUG
#define GL_REPORT void( 0 );
#define GL_ASSERT void( 0 );
#define GL_EXPR( stmt ) stmt;
#define GL_CHECK void( 0 );
#else
#define GL_REPORT PrintGLErrorMsg( __FILE__, __LINE__ );
//{												\
	//	GLenum err;									\
	//	while((err = glGetError()) != GL_NO_ERROR)	\
	//	{											\
	//		ysl::Warning("OpenGL Error Code:{}. File:{}, Line:{}. \n",err,__FILE__,__LINE__);\
	//	}											\
	//}

#define GL_ASSERT \
	assert( glGetError() == GL_NO_ERROR );

#define GL_EXPR( stmt )                                                         \
	do {                                                                        \
		GLenum glerr;                                                           \
		unsigned int iCounter = 0;                                              \
		while ( ( glerr = glGetError() ) != GL_NO_ERROR ) {                     \
			std::cout<<"GL error calling "<<#stmt<<" before line "<<__LINE__<<" of "<<__FILE__<<": ("<<static_cast<unsigned>( glerr )<<")\n";\
			PrintGLErrorType( glerr );                                          \
			iCounter++;                                                         \
			if ( iCounter > 100 ) break;                                        \
		}                                                                       \
		stmt;                                                                   \
		iCounter = 0;                                                           \
		while ( ( glerr = glGetError() ) != GL_NO_ERROR ) {                     \
			std::cout<<#stmt<<" on line "<<__LINE__<<" of "<<__FILE__<<" caused GL error: ("<< static_cast<unsigned>( glerr ) <<")\n"; \
			PrintGLErrorType( glerr );                                          \
			iCounter++;                                                         \
			if ( iCounter > 100 ) break;                                        \
		}                                                                       \
	} while ( 0 );

#define GL_CHECK                                                     \
	do {                                                             \
		GLenum glerr;                                                \
		unsigned int iCounter = 0;                                   \
		while ( ( glerr = glGetError() ) != GL_NO_ERROR ) {          \
			std::cout<<"before line ("<<__LINE__<<") in file "<<__FILE__<<" caused GL error: ("<<static_cast<unsigned>(glerr)<<")\n";\
			PrintGLErrorType( glerr );                               \
			iCounter++;                                              \
			if ( iCounter > 100 ) break;                             \
		}                                                            \
		iCounter = 0;                                                \
		while ( ( glerr = glGetError() ) != GL_NO_ERROR ) {          \
            std::cout<<"on line ("<<__LINE__<<") in file "<<__FILE__<<" caused GL error: ("<<static_cast<unsigned>(glerr)<<")\n";\
			PrintGLErrorType( glerr );                               \
			iCounter++;                                              \
			if ( iCounter > 100 ) break;                             \
		}                                                            \
	} while ( 0 );

#endif /*NDBUG*/

// class GLContextTraits{
// public:
//     void MakeCurrent();
//     bool HasWindow()const;
//     bool LoopEnd()const;
//     void DispatchEvent();
//     void Destroy();
//     static bool Init();
// };




enum GLObjectType
{
    Unknown = 0,
    ShaderProgram,
    Shader,
    Program,
    Buffer,
    Texture,
    Sampler,
    Framebuffer,
    Renderbuffer,
    VertexArray
};


template<typename T,typename U>
class GLContext;

template<typename GLCtxImpl,typename GLAPIImpl,int TypeValue>
class GLObject final                                  
{                                               
    using CONTEXT = GLContext<GLCtxImpl,GLAPIImpl>;
    std::shared_ptr<CONTEXT> glContext;                  
    uint32_t m_object = 0;
    template<typename T,typename U>                      
    friend class GLContext;            
    GLObject(std::shared_ptr<CONTEXT> ctx,uint32_t object):glContext(std::move(ctx)),m_object(object)
    {

    }

public:
    GLObject()=default;
    GLObject(const GLObject &) = delete;
    GLObject & operator=(const GLObject & ) = delete;

    GLObject(GLObject && rhs)noexcept:glContext(std::move(rhs.glContext)),m_object(std::move(rhs.m_object))
    {
        rhs.m_object = 0;
    }
    GLObject & operator=(GLObject && rhs)noexcept{
        Release();
        glContext = std::move(rhs.glContext);
        m_object = std::move(rhs.m_object);
        rhs.m_object = 0;
        return *this;
    }
    static GLObjectType GetType(){return TypeValue;}
    uint32_t GetGLHandle()const{return m_object;}
    bool Valid()const{return glContext && m_object != 0;}
    operator uint32_t()const{return m_object;}
    void Release(){
        if(glContext && m_object != 0){
            glContext->DeleteGLObject(*this);
        }
        glContext = nullptr;
        m_object = 0;                               
    }
    ~GLObject(){
        Release();
    }
};

template<typename GLCtxImpl,typename GLAPIImpl> class GLContext final:
public std::enable_shared_from_this<GLContext<GLCtxImpl,GLAPIImpl>>,
public GLCtxImpl,public GLAPIImpl
{
    using Self = GLContext<GLCtxImpl,GLAPIImpl>;
    GLContext(){

    }
public:
    template<int typeVal> using GLObjectT = GLObject<GLCtxImpl,GLAPIImpl,typeVal>;
    using GLShader = GLObjectT<GLObjectType::Shader>;
    using GLProgram = GLObjectT<GLObjectType::Program>;
    using GLShaderProgram = GLObjectT<GLObjectType::ShaderProgram>;
    using GLBuffer = GLObjectT<GLObjectType::Buffer>;
    using GLTexture= GLObjectT<GLObjectType::Texture>;
    using GLSampler= GLObjectT<GLObjectType::Sampler>;
    using GLFramebuffer= GLObjectT<GLObjectType::Framebuffer>;
    using GLRenderbuffer = GLObjectT<GLObjectType::Renderbuffer>;
    using GLVertexArray = GLObjectT<GLObjectType::VertexArray>;

    static auto NEW()
    {
        auto self = new Self();
        return std::shared_ptr<Self>(self);
    }

    GLShader CreateShader(GLenum shaderType)
    {
        uint32_t handle = 0;
        GL_EXPR(handle = glCreateShader(shaderType));
        return GLShader(this->shared_from_this(),handle);
    }
    void DeleteGLObject(GLShader & object)
    {
        object.glContext = nullptr;
        object.m_object = 0;
    }

    GLProgram CreateProgram()
    {
        uint32_t handle = 0;
        GL_EXPR( handle = glCreateProgram());
        return GLProgram(this->shared_from_this(),handle);
    }
    void DeleteGLObject(GLProgram & object)
    {
        GL_EXPR(glDeleteProgram(object.m_object));
        object.glContext = nullptr;
        object.m_object = 0;
    }

    GLShaderProgram CreateShaderProgram(GLenum shaderType,const GLchar * const str)
    {
        uint32_t handle = 0;
        GL_EXPR(handle = glCreateShaderProgramv(shaderType,1,str));
        return GLShaderProgram(this->shared_from_this(),handle);
    }

    void DeleteGLObject(GLShaderProgram & object)
    {
        GL_EXPR(glDeleteProgram(object.m_object));
        object.glContext = nullptr;
        object.m_object = 0;
    }

    GLTexture CreateTexture(GLenum textureTarget)
    {
        uint32_t textures = 0;
        GL_EXPR(glCreateTextures(textureTarget,1,&textures));
        return GLTexture(this->shared_from_this(),textures);
    }
    void DeleteGLObject(GLTexture & object)
    {
        GL_EXPR(glDeleteTextures(1,&object.m_object));
        object.glContext = nullptr;
        object.m_object = 0;
    }

    GLBuffer CreateBuffer()
    {
        uint32_t handle = 0;
        GL_EXPR(glCreateBuffers(1,&handle));
        return GLBuffer(this->shared_from_this(),handle);
    }
    void DeleteGLObject(GLBuffer & object)
    {
        GL_EXPR(glDeleteBuffers(1,&object.m_object));
        object.glContext = nullptr;
        object.m_object = 0;
    }

    GLFramebuffer CreateFramebuffer()
    {
        uint32_t handle = 0;
        GL_EXPR(glCreateFramebuffers(1,&handle));
        return GLFramebuffer(this->shared_from_this(),handle);
    }
    void DeleteGLObject(GLFramebuffer & object)
    {
        GL_EXPR(glDeleteFramebuffers(1,&object.m_object));
        object.glContext = nullptr;
        object.m_object = 0;
    }
    GLRenderbuffer CreateRenderbuffer()
    {
        uint32_t handle = 0;
        GL_EXPR(glCreateRenderbuffers(1,&handle));
        return GLRenderbuffer(this->shared_from_this(),handle);
    }
    void DeleteGLObject(GLRenderbuffer & object)
    {
        GL_EXPR(glDeleteRenderbuffers(1,&object.m_object));
        object.glContext = nullptr;
        object.m_object = 0;
    }
    GLSampler CreateSampler()
    {
        uint32_t handle = 0;
        GL_EXPR(glCreateSamplers(1,&handle));
        return GLSampler(this->shared_from_this(),handle);
    }
    void DeleteGLObject(GLSampler & object)
    {
        GL_EXPR(glDeleteSamplers(1,&object.m_object));
        object.glContext = nullptr;
        object.m_object = 0;
    }

    GLVertexArray CreateVertexArray(){
        uint32_t handle = 0;
        GL_EXPR(glCreateVertexArrays(1,&handle))
        return GLVertexArray(this->shared_from_this(),handle);
    }

    void DeleteGLObject(GLVertexArray & object)
    {
        GL_EXPR(glDeleteVertexArrays(1,&object.m_object));
        object.glContext = nullptr;
        object.m_object = 0;
    }
    
};


#define DEFINE_GL(GLCtxImpl,GLAPIImpl) \
using GL = GLContext<GLCtxImpl,GLAPIImpl>;
