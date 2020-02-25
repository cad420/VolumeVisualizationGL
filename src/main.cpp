

// std related
#include <iostream>
#include <fstream>
#include <memory>
#include <random>

// GL-related

#include <GLImpl.hpp>

// other dependency
#include <VMat/geometry.h>
#include <VMUtils/ref.hpp>
#include <VMUtils/log.hpp>
#include <VMFoundation/largevolumecache.h>
#include <VMFoundation/mappingtablemanager.h>
#include <VMGraphics/camera.h>
#include <VMGraphics/interpulator.h>

using namespace vm;
using namespace std;


/**
 * @brief Define OpenGL enviroment by the given implementation of context including window manager (GLFW) and api (GL3W)
 */
DEFINE_GL(GLFWImpl,GL3WImpl)  

// gloabl variables

namespace 
{
    Bound3f bound( { 0, 0, 0 }, { 1, 1, 1 } );
	Point3f CubeVertices[ 8 ];
	Point3f CubeTexCoords[ 8 ];
	unsigned int CubeVertexIndices[] = 
    {
		0, 2, 1, 1, 2, 3,
		4, 5, 6, 5, 7, 6,
		0, 1, 4, 1, 5, 4,
		2, 6, 3, 3, 6, 7,
		0, 4, 2, 2, 4, 6,
		1, 3, 5, 3, 7, 5
	};

	// Data object used by out-of-core ray-casting
    Vec2i windowSize = {1024,768};  								/*viewport size*/
    ViewingTransform camera({5,5,5},{0,1,0},{0,0,0}); 							/*camera controller*/
	Vec3i dataResolution;

	vector<uint32_t> BlockIDLocalBuffer;                            /*Reported missed block ID cache*/
	bool Render = false;
	bool FPSCamera = true;

	GL::GLProgram program;

	/**
	 * @brief Stores the CPU-end volume data.
	 * 
	 * Each Block3DCache is coressponding to one of a LOD of volume data.
	 */
    vector<Ref<Block3DCache>> VolumeData;

	/**
	 * \brief Manages and updates the LOD mapping tables.
	 */
	shared_ptr<MappingTableManager> mappingTableManager;

	/**
	 * \brief See the definition of the LODInfo in blockraycasting_f.glsl
	 *
	*/
	std::vector<_std140_layout_LODInfo> lodInfo;

	/**
	 * @brief Stores the transfer function texture
	 */
	GL::GLTexture GLTFTexture;

	/**
	 * @brief Stores the GPU-end volume data.
	 * 
	 * The size of a single texture unit is limited. Several textures units are necessary
	 * so as to make fully use of the GPU memory
	 */
	vector<GL::GLTexture> GLVolumeTexturme;

	/**
	 * \brief Stores the atomic counters for every lod data
	 *
	 * Using the first 4 bytes to store the atomic counter storage for the LOD0,
	 * and the second 4 bytes for the second LOD, etc.
	 *
	 */
	GL::GLBuffer GLAtomicCounterBufferHandle;
	/**
	 * \brief Stores the hash table for every lod data.
	 *
	 * Using the first section of the hash buffer to store the hash table for the LOD0,
	 * and the second section of the hash buffer for the second LOD, etc.
	 */
	GL::GLBuffer GLHashBufferHandle;
	/**
	 * \brief Stores the missed block id for every lod data
	 *
	 * Using the first section of the id buffer to store the missed block id for the LOD0,
	 * and the second section of the hash buffer for the second LOD, etc.
	 */
	GL::GLBuffer GLBlockIDBufferHandle;
	/**
	 * \brief Stores the page table for every lod data
	 *
	 * Using the first section of the page table buffer to store the page table for the LOD0,
	 * and the second section of the page table buffer for the second LOD, etc.
	 */
	GL::GLBuffer GLPageTableBufferHandle;
	/**
	 * \brief Stores all the lod information.
	 *
	 * \note The memory layout of the buffer is GLSL-dependent. See the definition of \a LODInfo in the fragment shader blockraycasting_f.glsl
	 */
	GL::GLBuffer GLLODInfoBufferHandle;

}
namespace {

void PrintCamera( const ViewingTransform & camera )
{
	println( "Position:{}", camera.GetViewMatrixWrapper().GetPosition() );
	println( "Up:{}", camera.GetViewMatrixWrapper().GetUp() );
	println( "Front:{}", camera.GetViewMatrixWrapper().GetFront() );
	println( "Right:{}", camera.GetViewMatrixWrapper().GetRight() );
	println( "ViewMatrix:{}", camera.GetViewMatrixWrapper().LookAt() );
}

string GetTextFromFile(const string & fileName)
{
	ifstream in(fileName,std::ios::in);
	if(in.is_open() == false)
	{
		vm::println("Failed to open file: {}",fileName);
		exit(-1);
	}
	return string{std::istreambuf_iterator<char>{in},std::istreambuf_iterator<char>{}};
}

GL::GLShader glCall_CreateShaderAndCompileHelper(GL & gl,GLenum shaderType, const char * source){

	auto handle = gl.CreateShader(shaderType);
	GL_EXPR(glShaderSource(handle,1,&source,nullptr));
	GL_EXPR(glCompileShader(handle));
	int success = 1;
	char infoLog[ 1024 ];
	GL_EXPR( glGetShaderiv( handle, GL_COMPILE_STATUS, &success ) );
	if ( !success ) {
		GL_EXPR( glGetShaderInfoLog( handle, 1024, nullptr, infoLog ) );
		vm::println( "ERROR::SHADER::COMPILATION_FAILED, {}\n", infoLog );
		exit(-1);
	}
	return handle;
}

void glCall_LinkProgramAndCheckHelper(GL::GLProgram & program)
{
	// link
	GL_EXPR(glLinkProgram(program));
	int success;
	char infoLog[ 1024];
	GL_EXPR( glGetProgramiv( program, GL_LINK_STATUS, &success ) );

	if ( !success ) 
	{
		glGetProgramInfoLog( program, 1024, nullptr, infoLog );
		vm::println( "ERROR::SHADER::PROGRAM::LINKING_FAILED\n:{}", infoLog );
		exit(-1);
	}
}

void glCall_SaveTextureAsImage(GL::GLTexture & texture,const std::string & fileName){

}

void glCall_UpdateTransferFunctionTexture(const string & fileName,int dimension){
	assert(GLTFTexture.Valid());
	assert(dimension == 256);
	if(dimension <= 0)
		return;
	if(fileName.empty()){
		// set linear tf
		std::unique_ptr<float[]> data(new float[dimension*4]);
		if(dimension == 1)
		{
			data[0] = data[1] = data[2] = data[3] = 1.0;
		}else{
			const double slope = 1.0/(dimension-1);
			for(int i = 0 ;i< dimension;i++){
				data[4*i] =
				data[4*i+1]=
				data[4*i+2] =
				data[4*i+3] = slope * i;
			}
		}
		GL_EXPR(glTextureSubImage1D(GLTFTexture,0,0,dimension,GL_RGBA,GL_FLOAT,data.get()));
	}
	else{
		ColorInterpulator a(fileName);
		if(a.valid()){
			std::unique_ptr<float[]> data(new float[dimension*4]);
			a.FetchData(data.get(),dimension);
			GL_EXPR(glTextureSubImage1D(GLTFTexture,0,0,dimension,GL_RGBA,GL_FLOAT,data.get()));
		}
	}
}

void glCall_CameraUniformUpdate(){
	// camera
	const auto mvpTransform = camera.GetPerspectiveMatrix() * camera.GetViewMatrixWrapper().LookAt();
	assert(program.Valid());
	GL_EXPR(glProgramUniformMatrix4fv(program,0,1,GL_TRUE,mvpTransform.Matrix().FlatData()));
}

void UpdateCPUVolumeData(const string & fileName){
	//println("updateVolumeData");

	Render = true;
}

void WindowResizeEvent(int width,int height){
	println("WindowResizeEvent");
}

void MouseEvent(MouseButton buttons,EventAction action,int xpos,int ypos){
	static Vec2i lastMousePos;
	static bool pressed = false;
	if(action == Press){
		lastMousePos = Vec2i(xpos,ypos);
		pressed = true;
	}else if(action == Move && pressed){
		const float dx = xpos - lastMousePos.x;
		const float dy = lastMousePos.y - ypos;

		if ( dx == 0.0 && dy == 0.0 )
			return;

		if ( FPSCamera == false ) {
			if ( ( buttons & Mouse_Left ) && ( buttons & Mouse_Right ) ) {
				const auto directionEx = camera.GetViewMatrixWrapper().GetUp() * dy + dx * camera.GetViewMatrixWrapper().GetRight();
				camera.GetViewMatrixWrapper().Move( directionEx, 0.002 );
			} else if ( buttons == Mouse_Left ) {
				camera.GetViewMatrixWrapper().Rotate( dx, dy,{0,0,0});
			} else if ( buttons == Mouse_Right && dy != 0.0 ) {
				const auto directionEx = camera.GetViewMatrixWrapper().GetFront() * dy;
				camera.GetViewMatrixWrapper().Move( directionEx, 1.0 );
			}

		} else {
			const auto front = camera.GetViewMatrixWrapper().GetFront().Normalized();
			const auto up = camera.GetViewMatrixWrapper().GetUp().Normalized();
			const auto right = Vec3f::Cross( front, up );
			const auto dir = ( up * dy - right * dx ).Normalized();
			const auto axis = Vec3f::Cross( dir, front );
			camera.GetViewMatrixWrapper().SetFront( Rotate( axis, 5.0 ) * front );
		}

		lastMousePos.x = xpos;
		lastMousePos.y = ypos;

		glCall_CameraUniformUpdate();
	}else if(action == Release){
		pressed = false;
		println("Release");
	}
}

void KeyboardEvent(KeyButton key,EventAction action){
	float sensity = 0.1;
	if(action == Press){
		if ( key == KeyButton::Key_C ) {
			SaveCameraAsJson( camera, "vmCamera.cam" );
			println("Save camera as vmCamera.cam");
		} else if ( key == KeyButton::Key_R ) {
			using std::default_random_engine;
			using std::uniform_int_distribution;
			default_random_engine e( time( 0 ) );
			uniform_int_distribution<int> u(0,100000);
			camera.GetViewMatrixWrapper().SetPosition( Point3f( u(e)%dataResolution.x,u(e)%dataResolution.y,u(e)&dataResolution.z ) );
			println("A random camera position generated");
		} else if ( key == KeyButton::Key_F ) {
			FPSCamera = !FPSCamera;
			if(FPSCamera){
				println("Switch to FPS camera manipulation");
			}
			else{
				println("Switch to track ball manipulation");
			}
			
		} else if ( key == KeyButton::Key_P ) {
			//intermediateResult->SaveTextureAs( "E:\\Desktop\\lab\\res_" + GetTimeStampString() + ".png" );
			//glCall_SaveTextureAsImage();
			println("Save screen shot");
		}

	}else if(action ==Repeat){
		if(FPSCamera){
			bool change = false;
			if ( key == KeyButton::Key_W ) {
				auto dir = camera.GetViewMatrixWrapper().GetFront();
				camera.GetViewMatrixWrapper().Move( sensity* dir.Normalized(), 10 );
				change = true;
				//mrtAgt->CreateGetCamera()->Movement();
			} else if ( key == KeyButton::Key_S ) {
				auto dir = -camera.GetViewMatrixWrapper().GetFront();
				camera.GetViewMatrixWrapper().Move(sensity* dir.Normalized(), 10 );
				change = true;
			} else if ( key == KeyButton::Key_A ) {
				auto dir = ( Vec3f::Cross( camera.GetViewMatrixWrapper().GetUp(), camera.GetViewMatrixWrapper().GetFront() ).Normalized() * sensity );
				camera.GetViewMatrixWrapper().Move( dir, 10 );
				change = true;
			} else if ( key == KeyButton::Key_D ) {
				auto dir = ( -Vec3f::Cross( camera.GetViewMatrixWrapper().GetUp(), camera.GetViewMatrixWrapper().GetFront() ).Normalized() ) * sensity;
				camera.GetViewMatrixWrapper().Move( dir, 10 );
				change = true;
			}
			if(change == true)
			{
				glCall_CameraUniformUpdate();
				//println("camera change");
				//PrintCamera(camera);
			}
		}
	}

}

void FileDropEvent(int count,const char ** df){
	println("FileDropEvent");
	vector<string> fileNames;
	for(int i = 0 ;i < count;i++){
		fileNames.push_back(df[i]);
	}

	for ( const auto &each : fileNames ) {
		if ( each.empty() )
			continue;
		const auto extension = each.substr( each.find_last_of( '.' ) );
		bool found = false;
		if ( extension == ".tf" ) {
			glCall_UpdateTransferFunctionTexture(each,256);
			found = true;
		} else if ( extension == ".lods" ) {
			UpdateCPUVolumeData(each);
			found = true;
		} else if ( extension == ".cam" ) {
			camera = ConfigCamera(each);
			found = true;
		}
		if ( found )
			break;
	}

	};
}




int main(int argc,char ** argv)
{

    // Initialize OpenGL, including context, api and window. GL commands are callable after GL object is created
	auto gl = GL::NEW();

	// Install EventListener
	GL::MouseEvent = [](void*,MouseButton buttons,EventAction action,int xpos,int ypos){MouseEvent(buttons,action,xpos,ypos);};
	GL::KeyboardEvent =[](void*,KeyButton key,EventAction action){KeyboardEvent(key,action);};
	GL::FramebufferResizeEvent = [](void*,int width,int height){WindowResizeEvent(width,height);};
	GL::FileDropEvent = [](void*,int count ,const char **df){FileDropEvent(count,df);};

    // 
	for ( int i = 0; i < 8; i++ )
    {
		CubeVertices[ i ] = bound.Corner( i );
		CubeTexCoords[ i ] = bound.Corner( i );
	}
    // Configuration Rendering Pipeline

	// [1] Initilize vertex buffer

	auto vao = gl->CreateVertexArray();
	GL_EXPR(glBindVertexArray(vao));
	auto vbo = gl->CreateBuffer();
	GL_EXPR(glNamedBufferStorage(vbo,sizeof(CubeVertices),CubeVertices,GL_DYNAMIC_STORAGE_BIT));
	GL_EXPR(glVertexArrayVertexBuffer(vao,0,vbo,0,sizeof(Point3f)));

	auto ebo = gl->CreateBuffer();
	GL_EXPR(glNamedBufferStorage(ebo,sizeof(CubeVertexIndices),CubeVertexIndices,GL_DYNAMIC_STORAGE_BIT));
	GL_EXPR(glVertexArrayElementBuffer(vao,ebo));

	GL_EXPR(glEnableVertexArrayAttrib(vao,0)); // layout(location = 0)
	GL_EXPR(glEnableVertexArrayAttrib(vao,1)); // layout(location = 1)

	GL_EXPR(glVertexArrayAttribFormat(vao,0,3,GL_FLOAT,GL_FALSE,0));
	GL_EXPR(glVertexArrayAttribFormat(vao,1,3,GL_FLOAT,GL_FALSE,0));

	GL_EXPR(glVertexArrayAttribBinding(vao,0,0));
	GL_EXPR(glVertexArrayAttribBinding(vao,1,0));

	// transfer data
	GL_EXPR(glNamedBufferSubData(vbo,0,sizeof(CubeVertices),CubeVertices));
	GL_EXPR(glNamedBufferSubData(ebo,0,sizeof(CubeVertexIndices),CubeVertexIndices));

	// [2] Initialize vertex shader

	auto vs = GetTextFromFile("resources/trivial_vs.glsl");
	const auto pVS = vs.c_str();
	auto vShader = glCall_CreateShaderAndCompileHelper(*gl,GL_VERTEX_SHADER,pVS);

	auto fs = GetTextFromFile("resources/trivial_fs.glsl");
	const auto pFS = fs.c_str();
	auto fShader = glCall_CreateShaderAndCompileHelper(*gl,GL_FRAGMENT_SHADER,pFS);


	// attach shader to program
	program = gl->CreateProgram();
	GL_EXPR(glAttachShader(program,vShader));
	GL_EXPR(glAttachShader(program,fShader));

	glCall_LinkProgramAndCheckHelper(program);
	gl->DeleteGLObject(vShader);
	gl->DeleteGLObject(fShader);


	// Set program uniforms 
	glCall_CameraUniformUpdate();
	
	// configuration rendering pipeline
	glClearColor(0.35,0.46,0.15,1.0);

	while(gl->Wait() == false){
		glClear(GL_COLOR_BUFFER_BIT);
		glUseProgram(program);
		glDrawElements(GL_TRIANGLES,36,GL_UNSIGNED_INT,nullptr);
		gl->DispatchEvent();
		gl->Present();
	}
    return 0;
}

