

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
#include <VMFoundation/rawreader.h>
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

void glCall_UpdateTransferFunctionTexture(GL::GLTexture & texture,const string & fileName,int dimension){
	assert(texture.Valid());
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
		GL_EXPR(glTextureSubImage1D(texture,0,0,dimension,GL_RGBA,GL_FLOAT,data.get()));
	}
	else{
		ColorInterpulator a(fileName);
		if(a.valid()){
			std::unique_ptr<float[]> data(new float[dimension*4]);
			a.FetchData(data.get(),dimension);
			GL_EXPR(glTextureSubImage1D(texture,0,0,dimension,GL_RGBA,GL_FLOAT,data.get()));
		}
	}
}

void glCall_CameraUniformUpdate(ViewingTransform & camera,Transform & modelMatrix,GL::GLProgram & positionGenerateProgram)
{
	// camera
	const auto mvpTransform = camera.GetPerspectiveMatrix() * camera.GetViewMatrixWrapper().LookAt();
	const auto viewPos = camera.GetViewMatrixWrapper().GetPosition();
	assert(positionGenerateProgram.Valid());
	GL_EXPR(glProgramUniformMatrix4fv(positionGenerateProgram,0,1,GL_TRUE,mvpTransform.Matrix().FlatData())); // location = 0 is MVPMatrix
	GL_EXPR(glProgramUniformMatrix4fv(positionGenerateProgram,1,1,GL_TRUE,modelMatrix.Matrix().FlatData())); // location = 1 is ModelMatrix
	GL_EXPR(glProgramUniform3fv(positionGenerateProgram,2,1,viewPos.ConstData())); // location = 1 is viewPos
}

GL::GLTexture glCall_RecreateRenderTargetTexture(GL & gl,int width,int height){
	auto rt = gl.CreateTexture(GL_TEXTURE_2D);
	GL_EXPR(glTextureStorage2D(rt,1,GL_RGBA32F,width,height));   // allocate storage
	//GL_EXPR(glTextureParameterf(rt,GL_TEXTURE_MIN_FILTER,GL_LINEAR));  // filter type
	//GL_EXPR(glTextureParameterf(rt,GL_TEXTURE_MAG_FILTER,GL_LINEAR));
	//GL_EXPR(glTextureParameterf(rt,GL_TEXTURE_WRAP_R,GL_CLAMP_TO_EDGE)); // boarder style
	//GL_EXPR(glTextureParameterf(rt,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE));
	return rt;
}

GL::GLTexture glCall_CreateVolumeTexture(GL & gl,int width,int height,int depth)
{
	auto t = gl.CreateTexture(GL_TEXTURE_3D);
	GL_EXPR(glTextureStorage3D(t,1,GL_R8,width,height,depth));
	return t;
}
}




int main(int argc,char ** argv)
{

    // Initialize OpenGL, including context, api and window. GL commands are callable after GL object is created
	auto gl = GL::NEW();

	Vec2i windowSize = {1024,768};  								/*viewport size*/
	Transform ModelTransform;										/*Model Matrix*/
	ModelTransform.SetIdentity();
	
    ViewingTransform camera({5,5,5},{0,1,0},{0,0,0}); 				/*camera controller (Projection and view matrix)*/
	Vec3i dataResolution;

	vector<uint32_t> BlockIDLocalBuffer;                            /*Reported missed block ID cache*/
	bool Render = false;
	bool FPSCamera = true;

	
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
	 * @brief Stores the entry and exit position of proxy geometry of volume data. 
	 * The result texture stores the intermediate result
	 * 
	 * Their size is same as the frameebuffer's. They are as the attachments of the FBO
	 */
	GL::GLFramebuffer GLFramebuffer;
	GL::GLTexture GLEntryPosTexture;
	GL::GLTexture GLExitPosTexture;
	GL::GLTexture GLResultTexture;


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

	GLFramebuffer = gl->CreateFramebuffer();
	GLEntryPosTexture = glCall_RecreateRenderTargetTexture(*gl,windowSize.x,windowSize.y);
	GLExitPosTexture = glCall_RecreateRenderTargetTexture(*gl,windowSize.x,windowSize.y);
	GLResultTexture = glCall_RecreateRenderTargetTexture(*gl,windowSize.x,windowSize.y);

	GL_EXPR(glNamedFramebufferTexture(GLFramebuffer,GL_COLOR_ATTACHMENT0,GLEntryPosTexture,0));
	GL_EXPR(glNamedFramebufferTexture(GLFramebuffer,GL_COLOR_ATTACHMENT1,GLExitPosTexture,0));
	GL_EXPR(glNamedFramebufferTexture(GLFramebuffer,GL_COLOR_ATTACHMENT2,GLResultTexture,0));
	// Depth and stencil attachments are not necessary in Ray Casting.

	GL_EXPR(if(glCheckNamedFramebufferStatus(GLFramebuffer,GL_FRAMEBUFFER) != GL_FRAMEBUFFER_COMPLETE){
		println("Framebuffer object is not complete.");
		exit(-1);
	});


	// [2] bounding box vertex shader

	auto vs = GetTextFromFile("resources/position_v.glsl");
	auto pVS = vs.c_str();
	auto vShader = glCall_CreateShaderAndCompileHelper(*gl,GL_VERTEX_SHADER,pVS);

	auto fs = GetTextFromFile("resources/position_f.glsl");
	auto pFS = fs.c_str();
	auto fShader = glCall_CreateShaderAndCompileHelper(*gl,GL_FRAGMENT_SHADER,pFS);


	// attach shader to program
	auto positionGenerateProgram = gl->CreateProgram();
	GL_EXPR(glAttachShader(positionGenerateProgram,vShader));
	GL_EXPR(glAttachShader(positionGenerateProgram,fShader));

	// Set Fragment output location, the processure must be done before linking the program
	GL_EXPR(glBindFragDataLocation(positionGenerateProgram,0,"entryPos"));
	GL_EXPR(glBindFragDataLocation(positionGenerateProgram,1,"exitPos"));

	glCall_LinkProgramAndCheckHelper(positionGenerateProgram);

	// Create Transfer function texture
	GLTFTexture = gl->CreateTexture(GL_TEXTURE_1D);
	assert(GLTFTexture.Valid());
	GL_EXPR(glTextureStorage1D(GLTFTexture,1,GL_RGBA32F,256));
	glCall_UpdateTransferFunctionTexture(GLTFTexture,"",256);



	// Create render target for ray-casting

	RawReader reader("/home/ysl/data/s1_128_128_128.raw",{128,128,128},1);
	auto size = reader.GetDimension().Prod() * reader.GetElementSize();
	std::unique_ptr<uint8_t[]> buf(new uint8_t[size]);
	if(reader.readRegion({0,0,0},{128,128,128},buf.get()) != size){
		println("Failed to read raw data");
		exit(-1);
	}
	auto testTexture = glCall_CreateVolumeTexture(*gl,128,128,128);
	assert(testTexture.Valid());
	GL_EXPR(glTextureSubImage3D(testTexture,0,0,0,0,128,128,128,GL_RED,GL_UNSIGNED_BYTE,buf.get()));


    // create ray casting shader
	vs = GetTextFromFile("resources/screenquad_v.glsl");
	pVS = vs.c_str();
	vShader = glCall_CreateShaderAndCompileHelper(*gl,GL_VERTEX_SHADER,pVS);

	fs = GetTextFromFile("resources/naiveraycast_f.glsl");
	pFS = fs.c_str();
	fShader = glCall_CreateShaderAndCompileHelper(*gl,GL_FRAGMENT_SHADER,pFS);

	auto raycastingProgram = gl->CreateProgram();
	GL_EXPR(glAttachShader(raycastingProgram,vShader));
	GL_EXPR(glAttachShader(raycastingProgram,fShader));
	glCall_LinkProgramAndCheckHelper(raycastingProgram);

	fs = GetTextFromFile("resources/screenquad_f.glsl");
	pFS = fs.c_str();
	fShader = glCall_CreateShaderAndCompileHelper(*gl,GL_FRAGMENT_SHADER,pFS);
	auto screenQuadProgram = gl->CreateProgram();
	GL_EXPR(glAttachShader(screenQuadProgram,vShader));
	GL_EXPR(glAttachShader(screenQuadProgram,fShader));
	glCall_LinkProgramAndCheckHelper(screenQuadProgram);

	gl->DeleteGLObject(vShader);
	gl->DeleteGLObject(fShader);

	// binding texture unit : see the raycasting shader for details
	GL_EXPR(glBindTextureUnit(0,GLTFTexture)); 	            // binds texture unit 0 for tf texture
	GL_EXPR(glBindTextureUnit(1,testTexture));              // binds texture unit 1 for volume texture
	//GL_EXPR(glBindTextureUnit(2,GLResultTexture));          // binds texture unit 2 for result texture
	// binding image unit : see the raycasting shader for details
	GL_EXPR(glBindImageTexture(0,GLEntryPosTexture,0,GL_FALSE,0,GL_READ_WRITE,GL_RGBA32F)); // binds image unit 0 for entry texture (read and write)
	GL_EXPR(glBindImageTexture(1,GLExitPosTexture,0,GL_FALSE,0,GL_READ_WRITE,GL_RGBA32F)); // binds image unit 1 for exit texture (read and write)
	GL_EXPR(glBindImageTexture(2,GLResultTexture,0,GL_FALSE,0,GL_READ_WRITE,GL_RGBA32F));  // binds image unit 2 for result texture (read and write)

	// Uniforms binding for program
	// [1] position shader
	glCall_CameraUniformUpdate(camera,ModelTransform,positionGenerateProgram); // camera-related uniforms for position program
	// [2] ray-casting shader
	GL::GLSampler sampler = gl->CreateSampler();
	GL_EXPR(glSamplerParameterf(sampler,GL_TEXTURE_MIN_FILTER,GL_LINEAR));  // filter type
	GL_EXPR(glSamplerParameterf(sampler,GL_TEXTURE_MAG_FILTER,GL_LINEAR));
	GL_EXPR(glSamplerParameterf(sampler,GL_TEXTURE_WRAP_R,GL_CLAMP_TO_EDGE)); // boarder style
	GL_EXPR(glSamplerParameterf(sampler,GL_TEXTURE_WRAP_S,GL_CLAMP_TO_EDGE));
	GL_EXPR(glSamplerParameterf(sampler,GL_TEXTURE_WRAP_T,GL_CLAMP_TO_EDGE));

	GL_EXPR(glProgramUniform1i(raycastingProgram,0,0)); // sets location = 0 (tf texture sampler) as tf texture unit 0
	GL_EXPR(glBindSampler(0,sampler));
	GL_EXPR(glProgramUniform1i(raycastingProgram,1,1)); // sets location = 1 (volume texture sampler) as volume texture unit 1
	GL_EXPR(glBindSampler(1,sampler));

	GL_EXPR(glProgramUniform1i(raycastingProgram,2,0)); // sets location = 2 as entry image unit 0
	GL_EXPR(glProgramUniform1i(raycastingProgram,3,1)); // sets location = 3 as exit image unit 1
	GL_EXPR(glProgramUniform1i(raycastingProgram,4,2)); // sets location = 4 as result image unit 2

	// [3] screen rendering shader
	GL_EXPR(glProgramUniform1i(screenQuadProgram,0,2)); // sets location = 0 as result image unit 2

		// Install EventListener
	GL::MouseEvent = [&camera,&FPSCamera,&positionGenerateProgram,&ModelTransform](void*,MouseButton buttons,EventAction action,int xpos,int ypos){
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

				glCall_CameraUniformUpdate(camera,ModelTransform,positionGenerateProgram);
			}else if(action == Release){
				pressed = false;
				println("Release");
			}
		};

	GL::KeyboardEvent =[&camera,&FPSCamera,&dataResolution,&positionGenerateProgram,&ModelTransform](void*,KeyButton key,EventAction action){
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
						glCall_CameraUniformUpdate(camera,ModelTransform,positionGenerateProgram);
						//println("camera change");
						//PrintCamera(camera);
					}
				}
			}
		};
	GL::FramebufferResizeEvent = [](void*,int width,int height){};
	GL::FileDropEvent = [&camera,&GLTFTexture](void*,int count ,const char **df){
		vector<string> fileNames;
		for(int i = 0 ;i < count;i++)
		{
			fileNames.push_back(df[i]);
		}

		for ( const auto &each : fileNames ) {
			if ( each.empty() )
				continue;
			const auto extension = each.substr( each.find_last_of( '.' ) );
			bool found = false;
			if ( extension == ".tf" ) {
				glCall_UpdateTransferFunctionTexture(GLTFTexture,each,256);
				found = true;
			} else if ( extension == ".lods" ) {
				//UpdateCPUVolumeData(each);
				found = true;
			} else if ( extension == ".cam" ) {
				camera = ConfigCamera(each);
				found = true;
			}
			if ( found )
				break;
		}};


	// configuration rendering pipeline

	const float zeroRGBA[]={0.f,0.f,0.f,0.f};
	GL_EXPR(glBlendFuncSeparate(GL_ONE,GL_ONE,GL_ONE,GL_ONE)); // Just add dst to src : (srcRBG * 1 + dstRGB * 1,srcAlpha * 1 + dstRGB * 1), so the backround color must be cleared as 0
	GL_EXPR(glFrontFace(GL_CW));
	const GLenum drawBuffers[2]={GL_COLOR_ATTACHMENT0,GL_COLOR_ATTACHMENT1};
	const GLenum allDrawBuffers[3]={GL_COLOR_ATTACHMENT0,GL_COLOR_ATTACHMENT1,GL_COLOR_ATTACHMENT2};

	while(gl->Wait() == false){ // application loop

		// Pass 0: Clears render targets


		// Pass 1: Generates ray position into textures
		
		glEnable(GL_BLEND); // Blend is necessary for ray-casting position generation
		GL_EXPR(glUseProgram(positionGenerateProgram));
		GL_EXPR(glBindFramebuffer(GL_FRAMEBUFFER,GLFramebuffer));

		// GL_EXPR(glClearNamedFramebufferfv(GLFramebuffer,GL_COLOR,0,zeroRGBA)); // Clear EntryPosTexture
		// GL_EXPR(glClearNamedFramebufferfv(GLFramebuffer,GL_COLOR,1,zeroRGBA)); // Clear ExitPosTexture
		// GL_EXPR(glClearNamedFramebufferfv(GLFramebuffer,GL_COLOR,2,zeroRGBA)); // Clear ResultTexture
		/**
		 * @brief Clears framebuffer use the non-DSA api because commented DSA version above has no
		 * effect on intel GPU.
		 */
		glDrawBuffers(3,allDrawBuffers);
		glClearBufferfv(GL_COLOR,0,zeroRGBA);
		glClearBufferfv(GL_COLOR,1,zeroRGBA);
		glClearBufferfv(GL_COLOR,2,zeroRGBA);

		GL_EXPR(glNamedFramebufferDrawBuffers(GLFramebuffer,2,drawBuffers)); // draw into these buffers
		GL_EXPR(glDrawElements(GL_TRIANGLES,36,GL_UNSIGNED_INT,nullptr)); // 12 triangles, 36 vertices in total

		// Pass 2 - n: Ray-casting here
		glDisable(GL_BLEND);
		GL_EXPR(glUseProgram(raycastingProgram));
		GL_EXPR(glNamedFramebufferDrawBuffer(GLFramebuffer,GL_COLOR_ATTACHMENT2)); // draw into result texture


		GL_EXPR(glDrawArrays(GL_TRIANGLE_STRIP,0,4)); // vertex is hard coded in shader

		//While out-of-core refine
		// Pass n + 1: Blit result to default framebuffer
		 GL_EXPR(glBindFramebuffer(GL_FRAMEBUFFER,0));  // prepare to display

	     GL_EXPR(glUseProgram(screenQuadProgram));
		 GL_EXPR(glDrawArrays(GL_TRIANGLE_STRIP,0,4)); // vertex is hard coded in shader
		 // You can use the framebuffer blit to display the result texture, but it maybe has a perfermance issue
		 //GL_EXPR(glNamedFramebufferReadBuffer(GLFramebuffer,GL_COLOR_ATTACHMENT2)); // set the read buffer of the src fbo
		 //GL_EXPR(glNamedFramebufferDrawBuffer(0,GL_BACK)); // set the draw buffer of the dst fbo
		 //GL_EXPR(glBlitNamedFramebuffer(GLFramebuffer,0,0,0,windowSize.x,windowSize.y,0,0,windowSize.x,windowSize.y,GL_COLOR_BUFFER_BIT,GL_LINEAR));

		// Final: Display on window and handle events
		gl->Present();
		gl->DispatchEvent();
	}

    return 0;
}

