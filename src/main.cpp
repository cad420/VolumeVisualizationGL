// std related
#include <iostream>
#include <fstream>
#include <memory>
#include <random>

// GL-related
#include <GLImpl.hpp>

// other dependences
#include <VMat/geometry.h>
#include <VMUtils/ref.hpp>
#include <VMUtils/vmnew.hpp>
#include <VMUtils/log.hpp>
#include <VMUtils/cmdline.hpp>
#include <VMFoundation/largevolumecache.h>
#include <VMFoundation/mappingtablemanager.h>
#include <VMFoundation/rawreader.h>
#include <VMFoundation/pluginloader.h>
#include <VMCoreExtension/i3dblockfileplugininterface.h>
#include <VMGraphics/camera.h>
#include <VMGraphics/interpulator.h>

using namespace vm;
using namespace std;

/**
 * @brief Define OpenGL enviroment by the given implementation of context including window manager (GLFW) and api (GL3W)
 */
DEFINE_GL( GLFWImpl, GL3WImpl )

// gloabl variables

namespace
{
Bound3f bound( { 0, 0, 0 }, { 1, 1, 1 } );
Point3f CubeVertices[ 8 ];
Point3f CubeTexCoords[ 8 ];

struct LVDJSONStruct : json::Serializable<LVDJSONStruct>
{
	VM_JSON_FIELD( std::vector<std::string>, fileNames );
	VM_JSON_FIELD( float, samplingRate );
	VM_JSON_FIELD( std::vector<float>, spacing );
};

using DeviceMemoryEvalutor = std::function<Vector4i( const Vector3i & )>;


unsigned int CubeVertexIndices[] = {
	0, 2, 1, 1, 2, 3,
	4, 5, 6, 5, 7, 6,
	0, 1, 4, 1, 5, 4,
	2, 6, 3, 3, 6, 7,
	0, 4, 2, 2, 4, 6,
	1, 3, 5, 3, 7, 5
};

}  // namespace
namespace
{
/**
 * @brief Summarizes and prints GPU memory usage of texture and buffer used by ray-cating 
 * 
 */
void PrintCamera( const ViewingTransform &camera )
{
	println( "Position:\t{}\t", camera.GetViewMatrixWrapper().GetPosition() );
	println( "Up:\t{}\t", camera.GetViewMatrixWrapper().GetUp() );
	println( "Front:\t{}\t", camera.GetViewMatrixWrapper().GetFront() );
	println( "Right:\t{}\t", camera.GetViewMatrixWrapper().GetRight() );
	println( "ViewMatrix:\t{}\t", camera.GetViewMatrixWrapper().LookAt() );
}

/**
 * @brief Get the Text From File object
 * 
 * @param fileName text file name
 * @return string text containt string
 */
string GetTextFromFile( const string &fileName )
{
	ifstream in( fileName, std::ios::in );
	if ( in.is_open() == false ) {
		vm::println( "Failed to open file: {}", fileName );
		exit( -1 );
	}
	return string{ std::istreambuf_iterator<char>{ in }, std::istreambuf_iterator<char>{} };
}

/**
 * @brief Returns a shader whose soure code is \a source and the type specified by \a shaderType  
 * 
 * @param gl GL context
 * @param shaderType a GLenum type of shader type
 * @param source shader source code string pointer
 * @return GL::GLShader 
 */
GL::GLShader glCall_CreateShaderAndCompileHelper( GL &gl, GLenum shaderType, const char *source )
{
	auto handle = gl.CreateShader( shaderType );
	GL_EXPR( glShaderSource( handle, 1, &source, nullptr ) );
	GL_EXPR( glCompileShader( handle ) );
	int success = 1;
	char infoLog[ 1024 ];
	GL_EXPR( glGetShaderiv( handle, GL_COMPILE_STATUS, &success ) );
	if ( !success ) {
		GL_EXPR( glGetShaderInfoLog( handle, 1024, nullptr, infoLog ) );
		println( "ERROR::SHADER::COMPILATION_FAILED, {}", infoLog );
		exit( -1 );
	}
	return handle;
}

/**
 * @brief Links program and check if the program has been linked successfully
 * 
 * @param program  
 */
void glCall_LinkProgramAndCheckHelper( GL::GLProgram &program )
{
	// link
	GL_EXPR( glLinkProgram( program ) );
	int success;
	char infoLog[ 1024 ];
	GL_EXPR( glGetProgramiv( program, GL_LINK_STATUS, &success ) );

	if ( !success ) {
		glGetProgramInfoLog( program, 1024, nullptr, infoLog );
		println( "ERROR::SHADER::PROGRAM::LINKING_FAILED:{}", infoLog );
		exit( -1 );
	}
}
/**
 * @brief Updates transfer function data of \a texture , if the \a fileName is empty, the default
 * transfer function will be set.
 * 
 * @param texture an initialized \a GL::GLTexture 1D 256 dimension texture
 * @param fileName  
 * @param dimension 256 only
 */
void glCall_UpdateTransferFunctionTexture( GL::GLTexture &texture,
										   const string &fileName,
										   int dimension )
{
	assert( texture.Valid() );
	assert( dimension == 256 );
	if ( dimension <= 0 )
		return;
	if ( fileName.empty() ) {
		// set linear tf
		std::unique_ptr<float[]> data( new float[ dimension * 4 ] );
		if ( dimension == 1 ) {
			data[ 0 ] = data[ 1 ] = data[ 2 ] = data[ 3 ] = 1.0;
		} else {
			const double slope = 1.0 / ( dimension - 1 );
			for ( int i = 0; i < dimension; i++ ) {
				data[ 4 * i ] =
				  data[ 4 * i + 1 ] =
					data[ 4 * i + 2 ] =
					  data[ 4 * i + 3 ] = slope * i;
			}
		}
		GL_EXPR( glTextureSubImage1D( texture, 0, 0, dimension, GL_RGBA, GL_FLOAT, data.get() ) );
	} else {
		ColorInterpulator a( fileName );
		if ( a.valid() ) {
			std::unique_ptr<float[]> data( new float[ dimension * 4 ] );
			a.FetchData( data.get(), dimension );
			GL_EXPR( glTextureSubImage1D( texture, 0, 0, dimension, GL_RGBA, GL_FLOAT, data.get() ) );
		}
	}
}

void glCall_CameraUniformUpdate( ViewingTransform &camera,
								 Transform &modelMatrix,
								 GL::GLProgram &positionGenerateProgram)
{
	// camera
	const auto mvpTransform = camera.GetPerspectiveMatrix() * camera.GetViewMatrixWrapper().LookAt();
	const auto viewTransform = camera.GetViewMatrixWrapper().LookAt();
	const auto viewPos = camera.GetViewMatrixWrapper().GetPosition();
	assert( positionGenerateProgram.Valid() );
	GL_EXPR( glProgramUniformMatrix4fv( positionGenerateProgram, 0, 1, GL_TRUE, mvpTransform.Matrix().FlatData() ) );  // location = 0 is MVPMatrix
	GL_EXPR( glProgramUniformMatrix4fv( positionGenerateProgram, 1, 1, GL_TRUE, modelMatrix.Matrix().FlatData() ) );   // location = 1 is ModelMatrix
	GL_EXPR( glProgramUniform3fv( positionGenerateProgram, 2, 1, viewPos.ConstData() ) );							   // location = 1 is viewPos
}

GL::GLTexture glCall_SetupResources(GL & gl,const std::string & volumeFile,const Vec3i & dataSize){

   RawReader reader(volumeFile,Size3(dataSize),1);
	 auto size = reader.GetDimension().Prod() * reader.GetElementSize();
	 std::unique_ptr<uint8_t[]> buf(new uint8_t[size]);
	 if(reader.readRegion({0,0,0},Size3(dataSize),buf.get()) != size){
	 	println("Failed to read raw data");
	 	exit(-1);
	 }
	 auto volTex = gl.CreateTexture( GL_TEXTURE_3D );
	 assert( volTex.Valid() );
	 GL_EXPR( glTextureStorage3D( volTex, 1, GL_R8, dataSize.x, dataSize.y, dataSize.z ) );
	 GL_EXPR( glTextureSubImage3D( volTex, 0, 0, 0, 0, dataSize.x, dataSize.y, dataSize.z, GL_RED, GL_UNSIGNED_BYTE, buf.get() ) );
	 GL_EXPR( glBindTextureUnit(1,volTex));              // binds texture unit 1 for volume texture
	 return volTex;
}

}  // namespace

int main( int argc, char **argv )
{
	Vec2i windowSize;
	cmdline::parser a;
	a.add<int>( "ww", 'w', "width of window", false, 1024 );
	a.add<int>( "wh", 'h', "height of window", false, 768 );
	a.add<string>( "file", 'f', "data json file", false );
	a.add<string>( "cam", '\0', "camera json file", false );
	a.add<string>( "tf", '\0', "transfer function text file", false );
  a.add<int>("width",'x',"data width",false,-1);
  a.add<int>("height",'y',"data height",false,-1);
  a.add<int>("depth",'z',"data depth",false,-1);

	a.parse_check( argc, argv );


	windowSize.x = a.get<int>( "ww" );
	windowSize.y = a.get<int>( "wh" );
	auto camFileName = a.get<string>( "cam" );
	auto tfFileName = a.get<string>( "tf" );
  auto volumeFile = a.get<string>("file");
  auto dataSize = Vec3i(a.get<int>("width"),a.get<int>("height"),a.get<int>("depth"));

  if(volumeFile.empty() == true){
    println("No volumed data");
    return 0;
  }
   if(dataSize.x <= 0 || dataSize.y <=0 || dataSize.z <=0)
   {
     println("Invalid data dimension");
     exit(-1);
   }

	// Initialize OpenGL, including context, api and window. GL commands are callable after GL object is created
	auto gl = GL::NEW();


	println( "Window Size: [{}, {}]", windowSize.x, windowSize.y );
	println( "Camera configuration file: {}", camFileName );
	println( "Transfer Function configuration file: {}", tfFileName );


	Transform ModelTransform; /*Model Matrix*/
	ModelTransform.SetIdentity();

	ViewingTransform camera( { 5, 5, 5 }, { 0, 1, 0 }, { 0, 0, 0 } ); /*camera controller (Projection and view matrix)*/
	if ( camFileName.empty() == false ) {
		try {
			camera = ConfigCamera( camFileName );
		} catch ( exception &e ) {
			println( "Cannot open camera file: {}", e.what() );
		}
	}
	bool FPSCamera = true;
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
	//
	for ( int i = 0; i < 8; i++ ) {
		CubeVertices[ i ] = bound.Corner( i );
		CubeTexCoords[ i ] = bound.Corner( i );
	}

	// Prepare data:
	// [1] Initilize vertex buffer
	auto vao = gl->CreateVertexArray();
	GL_EXPR( glBindVertexArray( vao ) );
	auto vbo = gl->CreateBuffer();
	GL_EXPR( glNamedBufferStorage( vbo, sizeof( CubeVertices ), CubeVertices, GL_DYNAMIC_STORAGE_BIT ) );
	GL_EXPR( glVertexArrayVertexBuffer( vao, 0, vbo, 0, sizeof( Point3f ) ) );

	auto ebo = gl->CreateBuffer();
	GL_EXPR( glNamedBufferStorage( ebo, sizeof( CubeVertexIndices ), CubeVertexIndices, GL_DYNAMIC_STORAGE_BIT ) );
	GL_EXPR( glVertexArrayElementBuffer( vao, ebo ) );

	GL_EXPR( glEnableVertexArrayAttrib( vao, 0 ) );	 // layout(location = 0)
	GL_EXPR( glEnableVertexArrayAttrib( vao, 1 ) );	 // layout(location = 1)

	GL_EXPR( glVertexArrayAttribFormat( vao, 0, 3, GL_FLOAT, GL_FALSE, 0 ) );
	GL_EXPR( glVertexArrayAttribFormat( vao, 1, 3, GL_FLOAT, GL_FALSE, 0 ) );

	GL_EXPR( glVertexArrayAttribBinding( vao, 0, 0 ) );
	GL_EXPR( glVertexArrayAttribBinding( vao, 1, 0 ) );

	GL_EXPR( glNamedBufferSubData( vbo, 0, sizeof( CubeVertices ), CubeVertices ) );
	GL_EXPR( glNamedBufferSubData( ebo, 0, sizeof( CubeVertexIndices ), CubeVertexIndices ) );

	 
	//[2] Create transfer function texture
	GLTFTexture = gl->CreateTexture( GL_TEXTURE_1D );
	assert( GLTFTexture.Valid() );
	GL_EXPR( glTextureStorage1D( GLTFTexture, 1, GL_RGBA32F, 256 ) );

	glCall_UpdateTransferFunctionTexture( GLTFTexture, tfFileName, 256 );

	// Create render targets
	GLFramebuffer = gl->CreateFramebuffer();
	GLEntryPosTexture = gl->CreateTexture( GL_TEXTURE_2D );
	assert( GLEntryPosTexture.Valid() );
	GLExitPosTexture = gl->CreateTexture( GL_TEXTURE_2D );
	assert( GLExitPosTexture.Valid() );
	GLResultTexture = gl->CreateTexture( GL_TEXTURE_2D );
	assert( GLResultTexture.Valid() );
	GL_EXPR( glTextureStorage2D( GLEntryPosTexture, 1, GL_RGBA32F, windowSize.x, windowSize.y ) );
	GL_EXPR( glTextureStorage2D( GLExitPosTexture, 1, GL_RGBA32F, windowSize.x, windowSize.y ) );
	GL_EXPR( glTextureStorage2D( GLResultTexture, 1, GL_RGBA32F, windowSize.x, windowSize.y ) );
	GL_EXPR( glNamedFramebufferTexture( GLFramebuffer, GL_COLOR_ATTACHMENT0, GLEntryPosTexture, 0 ) );
	GL_EXPR( glNamedFramebufferTexture( GLFramebuffer, GL_COLOR_ATTACHMENT1, GLExitPosTexture, 0 ) );
	GL_EXPR( glNamedFramebufferTexture( GLFramebuffer, GL_COLOR_ATTACHMENT2, GLResultTexture, 0 ) );
	// Depth and stencil attachments are not necessary in Ray Casting.

	GL_EXPR( if ( glCheckNamedFramebufferStatus( GLFramebuffer, GL_FRAMEBUFFER ) != GL_FRAMEBUFFER_COMPLETE ) {
		println( "Framebuffer object is not complete." );
		exit( -1 );
	} );

	/*Create shader program:*/
	//[1] bounding box vertex shader
	auto vs = GetTextFromFile( "resources/position_v.glsl" );
	auto pVS = vs.c_str();
	auto vShader = glCall_CreateShaderAndCompileHelper( *gl, GL_VERTEX_SHADER, pVS );

	auto fs = GetTextFromFile( "resources/position_f.glsl" );
	auto pFS = fs.c_str();
	auto fShader = glCall_CreateShaderAndCompileHelper( *gl, GL_FRAGMENT_SHADER, pFS );
	// attach shader to program
	auto positionGenerateProgram = gl->CreateProgram();
	GL_EXPR( glAttachShader( positionGenerateProgram, vShader ) );
	GL_EXPR( glAttachShader( positionGenerateProgram, fShader ) );
	// Set Fragment output location, the processure must be done before linking the program
	GL_EXPR( glBindFragDataLocation( positionGenerateProgram, 0, "entryPos" ) );
	GL_EXPR( glBindFragDataLocation( positionGenerateProgram, 1, "exitPos" ) );
	glCall_LinkProgramAndCheckHelper( positionGenerateProgram );

	//[2] ray casting shader
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


	//[3] screen rendering program (render the result texture onto the screen (Do not use Blit api))
	fs = GetTextFromFile( "resources/screenquad_f.glsl" );
	pFS = fs.c_str();
	fShader = glCall_CreateShaderAndCompileHelper( *gl, GL_FRAGMENT_SHADER, pFS );
	auto screenQuadProgram = gl->CreateProgram();
	GL_EXPR( glAttachShader( screenQuadProgram, vShader ) );
	GL_EXPR( glAttachShader( screenQuadProgram, fShader ) );
	glCall_LinkProgramAndCheckHelper( screenQuadProgram );

	// Shaders could be deleted after linked
	gl->DeleteGLObject( vShader );
	gl->DeleteGLObject( fShader );


  // Create Volume Texture
  auto testTexture = glCall_SetupResources(*gl, volumeFile, dataSize);


	/* Texture unit and image unit binding*/
	// [1] binds texture unit : see the raycasting shader for details
	GL_EXPR( glBindTextureUnit( 0, GLTFTexture ) );	 // binds texture unit 0 for tf texture
	// [2] binds image unit : see the raycasting shader for details
	GL_EXPR( glBindImageTexture( 0, GLEntryPosTexture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F ) );  // binds image unit 0 for entry texture (read and write)
	GL_EXPR( glBindImageTexture( 1, GLExitPosTexture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F ) );   // binds image unit 1 for exit texture (read and write)
	GL_EXPR( glBindImageTexture( 2, GLResultTexture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F ) );	   // binds image unit 2 for result texture (read and write)

	/* Uniforms binding for program*/
	//[1] position shader
	glCall_CameraUniformUpdate( camera, ModelTransform, positionGenerateProgram);  // camera-related uniforms for position program
	//[2] ray-casting shader
	GL::GLSampler sampler = gl->CreateSampler();
	GL_EXPR( glSamplerParameterf( sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR ) );  // filter type
	GL_EXPR( glSamplerParameterf( sampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR ) );
	GL_EXPR( glSamplerParameterf( sampler, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE ) );	 // boarder style
	GL_EXPR( glSamplerParameterf( sampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE ) );
	GL_EXPR( glSamplerParameterf( sampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE ) );

  GL_EXPR(glProgramUniform1i(raycastingProgram,3,0)); // sets location = 0 (tf texture sampler) as tf texture unit 0
	GL_EXPR(glBindSampler(0,sampler));
	GL_EXPR(glProgramUniform1i(raycastingProgram,4,1)); // sets location = 1 (volume texture sampler) as volume texture unit 1
	GL_EXPR(glBindSampler(1,sampler));

	GL_EXPR(glProgramUniform1i(raycastingProgram,0,0)); // sets location = 2 as entry image unit 0
	GL_EXPR(glProgramUniform1i(raycastingProgram,1,1)); // sets location = 3 as exit image unit 1
	GL_EXPR(glProgramUniform1i(raycastingProgram,2,2)); // sets location = 4 as result image unit 2


	//[3] screen rendering shader
	GL_EXPR( glProgramUniform1i( screenQuadProgram, 0, 2 ) );  // sets location = 0 as result image unit 2

	/* Install event listeners */
	GL::MouseEvent = [&]( void *, MouseButton buttons, EventAction action, int xpos, int ypos ) {
		static Vec2i lastMousePos;
		static bool pressed = false;
		if ( action == Press ) {
			lastMousePos = Vec2i( xpos, ypos );
			pressed = true;
		} else if ( action == Move && pressed ) {
			const float dx = xpos - lastMousePos.x;
			const float dy = lastMousePos.y - ypos;

			if ( dx == 0.0 && dy == 0.0 )
				return;

			if ( FPSCamera == false ) {
				if ( ( buttons & Mouse_Left ) && ( buttons & Mouse_Right ) ) {
					const auto directionEx = camera.GetViewMatrixWrapper().GetUp() * dy + dx * camera.GetViewMatrixWrapper().GetRight();
					camera.GetViewMatrixWrapper().Move( directionEx, 0.002 );
				} else if ( buttons == Mouse_Left ) {
					camera.GetViewMatrixWrapper().Rotate( dx, dy, { 0, 0, 0 } );
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

			glCall_CameraUniformUpdate( camera, ModelTransform, positionGenerateProgram);
		} else if ( action == Release ) {
			pressed = false;
			println( "Release" );
		}
	};

	GL::KeyboardEvent = [&]( void *, KeyButton key, EventAction action ) {
		float sensity = 0.1;
		if ( action == Press ) {
			if ( key == KeyButton::Key_C ) {
				SaveCameraAsJson( camera, "vmCamera.cam" );
				println( "Save camera as vmCamera.cam" );
			}  else if ( key == KeyButton::Key_F ) {
				FPSCamera = !FPSCamera;
				if ( FPSCamera ) {
					println( "Switch to FPS camera manipulation" );
				} else {
					println( "Switch to track ball manipulation" );
				}

			} else if ( key == KeyButton::Key_P ) {
				println( "Save screen shot (This feature is not implemented yet.)" );
			}

		} else if ( action == Repeat ) {
			if ( FPSCamera ) {
				bool change = false;
				if ( key == KeyButton::Key_W ) {
					auto dir = camera.GetViewMatrixWrapper().GetFront();
					camera.GetViewMatrixWrapper().Move( sensity * dir.Normalized(), 10 );
					change = true;
					//mrtAgt->CreateGetCamera()->Movement();
				} else if ( key == KeyButton::Key_S ) {
					auto dir = -camera.GetViewMatrixWrapper().GetFront();
					camera.GetViewMatrixWrapper().Move( sensity * dir.Normalized(), 10 );
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
				if ( change == true ) {
					glCall_CameraUniformUpdate( camera, ModelTransform, positionGenerateProgram);
				}
			}
		}
	};
	GL::FramebufferResizeEvent = []( void *, int width, int height ) {};
	GL::FileDropEvent = [&]( void *, int count, const char **df ) {
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
      } else if ( extension == ".cam" ) {
				try{
					camera = ConfigCamera(each);
				}catch(std::exception & e){
					println("Cannot open .cam file: {}",e.what());
				}
				found = true;
			}
			if ( found )
				break;
		} };



	/*Configuration rendering state*/
	const float zeroRGBA[] = { 0.f, 0.f, 0.f, 0.f };
	GL_EXPR( glBlendFuncSeparate( GL_ONE, GL_ONE, GL_ONE, GL_ONE ) );  // Just add dst to src : (srcRBG * 1 + dstRGB * 1,srcAlpha * 1 + dstRGB * 1), so the backround color must be cleared as 0
	GL_EXPR( glFrontFace( GL_CW ) );
	const GLenum drawBuffers[ 2 ] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1 };
	const GLenum allDrawBuffers[ 3 ] = { GL_COLOR_ATTACHMENT0, GL_COLOR_ATTACHMENT1, GL_COLOR_ATTACHMENT2 };

	while ( gl->Wait() == false ) {
		/*Ray Casting Rendering Loop*/
		// Pass [1]: Generates ray position into textures
		glEnable( GL_BLEND );  // Blend is necessary for ray-casting position generation
		GL_EXPR( glUseProgram( positionGenerateProgram ) );
		GL_EXPR( glBindFramebuffer( GL_FRAMEBUFFER, GLFramebuffer ) );

		// GL_EXPR(glClearNamedFramebufferfv(GLFramebuffer,GL_COLOR,0,zeroRGBA)); // Clear EntryPosTexture
		// GL_EXPR(glClearNamedFramebufferfv(GLFramebuffer,GL_COLOR,1,zeroRGBA)); // Clear ExitPosTexture
		// GL_EXPR(glClearNamedFramebufferfv(GLFramebuffer,GL_COLOR,2,zeroRGBA)); // Clear ResultTexture
		/**
		 * @brief Clearing framebuffer by using the non-DSA api because commented DSA version above has no
		 * effect on intel GPU. Maybe it's a graphics driver bug. 
		 * see https://software.intel.com/en-us/forums/graphics-driver-bug-reporting/topic/740117
		 */
		GL_EXPR( glDrawBuffers( 3, allDrawBuffers ) );
		GL_EXPR( glClearBufferfv( GL_COLOR, 0, zeroRGBA ) );  // Clear EntryPosTexture
		GL_EXPR( glClearBufferfv( GL_COLOR, 1, zeroRGBA ) );  // CLear ExitPosTexture
		GL_EXPR( glClearBufferfv( GL_COLOR, 2, zeroRGBA ) );  // Clear ResultTexture

		GL_EXPR( glNamedFramebufferDrawBuffers( GLFramebuffer, 2, drawBuffers ) );	// draw into these buffers
		GL_EXPR( glDrawElements( GL_TRIANGLES, 36, GL_UNSIGNED_INT, nullptr ) );	// 12 triangles, 36 vertices in total

		// Pass [2 - n]: Ray casting here
		GL_EXPR( glDisable( GL_BLEND ) );

		GL_EXPR(glUseProgram(raycastingProgram));
		GL_EXPR( glNamedFramebufferDrawBuffer( GLFramebuffer, GL_COLOR_ATTACHMENT2 ) );	 // draw into result texture
		GL_EXPR( glDrawArrays( GL_TRIANGLE_STRIP, 0, 4 ) );	 // vertex is hard coded in shader

		// Pass [n + 1]: Blit result to default framebuffer
		GL_EXPR( glBindFramebuffer( GL_FRAMEBUFFER, 0 ) );	// prepare to display

		GL_EXPR( glUseProgram( screenQuadProgram ) );
		GL_EXPR( glDrawArrays( GL_TRIANGLE_STRIP, 0, 4 ) );	 // vertex is hard coded in shader
		// You can use the framebuffer blit to display the result texture, but it maybe has a driver-dependent perfermance drawback.
		//GL_EXPR(glNamedFramebufferReadBuffer(GLFramebuffer,GL_COLOR_ATTACHMENT2)); // set the read buffer of the src fbo
		//GL_EXPR(glNamedFramebufferDrawBuffer(0,GL_BACK)); // set the draw buffer of the dst fbo
		//GL_EXPR(glBlitNamedFramebuffer(GLFramebuffer,0,0,0,windowSize.x,windowSize.y,0,0,windowSize.x,windowSize.y,GL_COLOR_BUFFER_BIT,GL_LINEAR)); // setting current fb as default fb is necessary
    
		// Final: Display on window and handle events
		gl->Present();
		gl->DispatchEvent();
	}

	return 0;
}
