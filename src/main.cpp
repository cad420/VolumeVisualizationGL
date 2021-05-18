// std related
#include <iostream>
#include <fstream>
#include <memory>
#include <random>

// GL-related

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

#include <GLImpl.hpp>
#include <jsondef.hpp>
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


using DeviceMemoryEvalutor = std::function<Vector4i( const Vector3i & )>;

struct MyEvaluator : IVideoMemoryParamsEvaluator
{
private:
	const Size3 virtualDim;
	const Size3 blockSize;
	const std::size_t videoMem;
	int textureUnitCount = 0;
	Size3 finalBlockDim = { 0, 0, 0 };

public:
	MyEvaluator( const Size3 &virtualDim, const Size3 &blockSize, std::size_t videoMemory ) :
	  virtualDim( virtualDim ),
	  blockSize( blockSize ),
	  videoMem( videoMemory )
	{
		// We assume that we can only use 3/4 of total video memory and there are 4 texture units at most

		textureUnitCount = 4;
		const auto maxBytesPerTexUnit = videoMemory * 3 / 4 / textureUnitCount;
		std::size_t d = 0;
		while ( ++d ) {
			const auto memory = d * d * d * blockSize.Prod();
			if ( memory >= maxBytesPerTexUnit )
				break;
		}
		d--;
		//while ( d > 10 )
		//{
		//	d /= 2;
		//	textureUnitCount++;
		//}
		finalBlockDim = Size3{ d, d, d };
	}

	Size3 EvalPhysicalTextureSize() const override
	{
		return blockSize * EvalPhysicalBlockDim();
	}
	Size3 EvalPhysicalBlockDim() const override
	{
		return finalBlockDim;
	}

	int EvalPhysicalTextureCount() const override
	{
		return textureUnitCount;
	}
	~MyEvaluator() = default;
};

// struct HelperGPUObjectSetCreateInfo{
// 	size_t PageTableEntryBufferLength = 0;
// 	size_t AtomicCounterBufferLength= 0 ;
// 	size_t HashBufferLength=0;
// 	size_t IDBufferLength=0;
// 	HelperGPUObjectSetCreateInfo(size_t pc,size_t al,size_t hl,size_t il):
// 	PageTableEntryBufferLength(pc),
// 	AtomicCounterBufferLength(al),
// 	HashBufferLength(hl),
// 	IDBufferLength(il){}
// };

struct HelperGPUObjectSet
{
	/**
		 * @brief Stores the GPU-end volume data.
		 * 
		 * The size of a single texture unit is limited. Several textures units are necessary
		 * so as to make fully use of the GPU memory
		 */
	vector<GL::GLTexture> GLVolumeTexture;

	/**
		 * \brief Stores the atomic counters for every lod data
		 *
		 * Using the first 4 bytes to store the atomic counter storage for the LOD0,
		 * and the second 4 bytes for the second LOD, etc.
		 *
		 */
	GL::GLBuffer GLAtomicCounterBuffer;
	uint32_t *AtomicCounterBufferPersistentMappedPointer = nullptr;
	size_t AtomicCounterBufferBytes = 0;
	/**
		 * \brief Stores the hash table for every lod data.
		 *
		 * Using the first section of the hash buffer to store the hash table for the LOD0,
		 * and the second section of the hash buffer for the second LOD, etc.
		 */
	GL::GLBuffer GLHashBuffer;
	uint32_t *HashBufferPersistentMappedPointer = nullptr;
	size_t HashBufferBytes = 0;
	/**
		 * \brief Stores the missed block id for every lod data
		 *
		 * Using the first section of the id buffer to store the missed block id for the LOD0,
		 * and the second section of the hash buffer for the second LOD, etc.
		 */
	GL::GLBuffer GLBlockIDBuffer;
	uint32_t *BlockIDBufferPersistentMappedPointer = nullptr;
	size_t BlockIDBufferBytes = 0;
	/**
		 * \brief Stores the page table for every lod data
		 *
		 * Using the first section of the page table buffer to store the page table for the LOD0,
		 * and the second section of the page table buffer for the second LOD, etc.
		 */
	GL::GLBuffer GLPageTableBuffer;
	uint32_t *PageTableBufferPersistentMappedPointer = nullptr;
	size_t PageTableBufferBytes = 0;
	/**
		 * \brief Stores all the lod information.
		 *
		 * \note The memory layout of the buffer is GLSL-dependent. See the definition of \a LODInfo in the fragment shader blockraycasting_f.glsl
		 */
	GL::GLBuffer GLLODInfoBuffer;
	uint32_t *LODInfoBufferPersistentMappedPointer = nullptr;
	size_t LODInfoBufferBytes = 0;
};

struct HelperCPUObjectSet
{
	/**
		 * \brief See the definition of the LODInfo in blockraycasting_f.glsl
		*/
	std::vector<_std140_layout_LODInfo> LODInfoCPUBuffer;

	//HelperGPUObjectSetCreateInfo GPUObjectPropertyHint;

	/**
		 * @brief Stores the CPU-end volume data.
		 * 
		 * Each Ref<Block3DCache> is coressponding to one of a LOD of volume data.
		 */
	vector<Ref<Block3DCache>> VolumeData;
};

struct HelperObjectSet
{
	HelperCPUObjectSet CPUSet;
	HelperGPUObjectSet GPUSet;
	/**
		 * \brief Manages and updates the LOD mapping tables.
		 */
	shared_ptr<MappingTableManager> MappingManager;
};

constexpr GLbitfield mapping_flags = GL_MAP_WRITE_BIT | GL_MAP_READ_BIT | GL_MAP_PERSISTENT_BIT | GL_MAP_COHERENT_BIT;
constexpr GLbitfield storage_flags = GL_DYNAMIC_STORAGE_BIT | mapping_flags;

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
void PrintVideoMemoryUsageInfo( std::ostream &os, const HelperObjectSet &set, size_t volumeTextureMemoryUsage );
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
								 GL::GLProgram &positionGenerateProgram,
								 GL::GLProgram &outofcoreProgram )
{
	// camera
	const auto mvpTransform = camera.GetPerspectiveMatrix() * camera.GetViewMatrixWrapper().LookAt();
	const auto viewTransform = camera.GetViewMatrixWrapper().LookAt();
	const auto viewPos = camera.GetViewMatrixWrapper().GetPosition();
	assert( positionGenerateProgram.Valid() );
	GL_EXPR( glProgramUniformMatrix4fv( positionGenerateProgram, 0, 1, GL_TRUE, mvpTransform.Matrix().FlatData() ) );  // location = 0 is MVPMatrix
	GL_EXPR( glProgramUniformMatrix4fv( positionGenerateProgram, 1, 1, GL_TRUE, modelMatrix.Matrix().FlatData() ) );   // location = 1 is ModelMatrix
	GL_EXPR( glProgramUniform3fv( positionGenerateProgram, 2, 1, viewPos.ConstData() ) );							   // location = 1 is viewPos
	// out of core prgoram
	assert( outofcoreProgram.Valid() );
	GL_EXPR( glProgramUniformMatrix4fv( outofcoreProgram, 10, 1, GL_TRUE, viewTransform.Matrix().FlatData() ) );  // location = 0 is MVPMatrix
	GL_EXPR( glProgramUniformMatrix4fv( outofcoreProgram, 9, 1, GL_TRUE, modelMatrix.Matrix().FlatData() ) );	  // location = 1 is ModelMatrix
	GL_EXPR( glProgramUniform3fv( outofcoreProgram, 11, 1, viewPos.ConstData() ) );								  // location = 1 is viewPos
}

GL::GLTexture glCall_CreateVolumeTexture( GL &gl, int width, int height, int depth )
{
	auto t = gl.CreateTexture( GL_TEXTURE_3D );
	GL_EXPR( glTextureStorage3D( t, 1, GL_R8, width, height, depth ) );
	return t;
}

void *glCall_MapBufferRangeHelperFunc( GL::GLBuffer &buffer, GLenum target, size_t offset, size_t length, int rangeMapFlags )
{
	assert( buffer.Valid() );
	void *mappedPointer = nullptr;
	GL_EXPR( glBindBuffer( target, buffer ) );
	GL_EXPR( mappedPointer = glMapNamedBufferRange( buffer, offset, length, rangeMapFlags ) );
	GL_EXPR( glBindBuffer( target, 0 ) );
	assert( mappedPointer );
	return mappedPointer;
}

vector<Ref<Block3DCache>> SetupVolumeData(
  const vector<string> &fileNames,
  PluginLoader &pluginLoader,
  size_t availableHostMemoryHint )
{
	try {
		const auto lodCount = fileNames.size();
		vector<Ref<Block3DCache>> volumeData( lodCount );
		for ( int i = 0; i < lodCount; i++ ) {
			const auto cap = fileNames[ i ].substr( fileNames[ i ].find_last_of( '.' ) );
			auto p = pluginLoader.CreatePlugin<I3DBlockFilePluginInterface>( cap );
			if ( !p ) {
				println( "Failed to load plugin to read {} file", cap );
				exit( -1 );
			}
			p->Open( fileNames[ i ] );
			volumeData[ i ] = VM_NEW<Block3DCache>( p, [&availableHostMemoryHint]( I3DBlockDataInterface *p ) {
				// this a
				const auto bytes = p->GetDataSizeWithoutPadding().Prod();
				size_t th = 2 * 1024 * 1024 * size_t( 1024 );  // 2GB as default
				if ( availableHostMemoryHint != 0 )
					th = availableHostMemoryHint;
				size_t d = 0;
				const auto pageSize = p->Get3DPageSize().Prod();
				if ( bytes < th ) {
					while ( d * d * d * pageSize < bytes ) d++;
				} else {
					while ( d * d * d * pageSize < th )
						d++;
				}
				return Size3{ d, d, d };
			} );
		}
		return volumeData;
	} catch ( std::runtime_error &e ) {
		println( "{}", e.what() );
		return {};
	}
}

HelperCPUObjectSet CreateHelperCPUObjectSet( const vector<string> &fileNames,
											 PluginLoader &pluginLoader,
											 size_t availableHostMemoryHint )
{
	HelperCPUObjectSet set;
	set.VolumeData = SetupVolumeData( fileNames, pluginLoader, availableHostMemoryHint );
	auto &cpuVolumeData = set.VolumeData;
	if ( cpuVolumeData.size() == 0 ) {
		return set;
	}

	size_t pageTableTotalEntries = 0;
	size_t hashBufferTotalBlocks = 0;
	size_t idBufferTotalBlocks = 0;

	set.LODInfoCPUBuffer.resize( cpuVolumeData.size() );
	auto &lodInfo = set.LODInfoCPUBuffer;
	for ( int i = 0; i < cpuVolumeData.size(); i++ ) {
		lodInfo[ i ].volumeDataSizeNoRepeat = Vec4i( Vec3i( cpuVolumeData[ i ]->DataSizeWithoutPadding() ) );
		const int padding = cpuVolumeData[ i ]->Padding();
		lodInfo[ i ].blockDataSizeNoRepeat = Vec4i( Vec3i( cpuVolumeData[ i ]->BlockSize() - Size3( 2 * padding, 2 * padding, 2 * padding ) ) );
		lodInfo[ i ].pageTableSize = Vec4i( Vec3i( cpuVolumeData[ i ]->BlockDim() ) );	// GLSL std140 layout
		lodInfo[ i ].pageTableOffset = pageTableTotalEntries;
		lodInfo[ i ].idBufferOffset = idBufferTotalBlocks;
		lodInfo[ i ].hashBufferOffset = hashBufferTotalBlocks;

		const auto blocks = cpuVolumeData[ i ]->BlockDim().Prod();
		pageTableTotalEntries += blocks;  // *sizeof(MappingTableManager::PageTableEntry);
		hashBufferTotalBlocks += blocks;  // *sizeof(uint32_t);
		idBufferTotalBlocks += blocks;	  // *sizeof(uint32_t);
	}

	return set;
}

void glCall_ResourcesBinding( HelperObjectSet &set, GL::GLProgram &outofcoreProgram )
{
	assert( set.GPUSet.GLPageTableBuffer.Valid() );
	assert( set.GPUSet.GLAtomicCounterBuffer.Valid() );
	assert( set.GPUSet.GLBlockIDBuffer.Valid() );
	assert( set.GPUSet.GLHashBuffer.Valid() );
	assert( set.GPUSet.GLLODInfoBuffer.Valid() );

	GL_EXPR( glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 2, set.GPUSet.GLPageTableBuffer ) );

	GL_EXPR( glBindBufferBase( GL_ATOMIC_COUNTER_BUFFER, 3, set.GPUSet.GLAtomicCounterBuffer ) );

	GL_EXPR( glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 1, set.GPUSet.GLBlockIDBuffer ) );

	GL_EXPR( glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 0, set.GPUSet.GLHashBuffer ) );

	GL_EXPR( glBindBufferBase( GL_SHADER_STORAGE_BUFFER, 3, set.GPUSet.GLLODInfoBuffer ) );

	assert( set.GPUSet.LODInfoBufferBytes == set.CPUSet.VolumeData.size() * sizeof( _std140_layout_LODInfo ) );
	GL_EXPR( glNamedBufferSubData( set.GPUSet.GLLODInfoBuffer, 0, set.GPUSet.LODInfoBufferBytes, set.CPUSet.LODInfoCPUBuffer.data() ) );

	for ( int i = 0; i < set.GPUSet.GLVolumeTexture.size(); i++ ) {
		// binding volume textures as unit 1,2,3,4
		assert( set.GPUSet.GLVolumeTexture[ i ] );
		GL_EXPR( glBindTextureUnit( i + 1, set.GPUSet.GLVolumeTexture[ i ] ) );
	}
	assert( outofcoreProgram.Valid() );
	GL_EXPR( glProgramUniform1i( outofcoreProgram, 12, set.CPUSet.VolumeData.size() ) );
}

void glCall_ClearObjectSet( HelperObjectSet &set )
{
	// Clear Atomic counter as 0
	memset( set.GPUSet.AtomicCounterBufferPersistentMappedPointer, 0, set.GPUSet.AtomicCounterBufferBytes );
	memset( set.GPUSet.HashBufferPersistentMappedPointer, 0, set.GPUSet.HashBufferBytes );
}

HelperObjectSet glCall_SetupResources( GL &gl, const std::string &fileName,
									   PluginLoader &pluginLoader,
									   size_t availableHostMemoryHint,
									   std::function<Vec4i( const Vec3i &blockSize )> deviceMemoryEvaluator )
{
	LVDJSONStruct lvdJSON;
	std::ifstream json( fileName );
	json >> lvdJSON;

	//vector<string> testFileNames{"/home/ysl/data/s1.brv"};

	HelperObjectSet set;
	set.CPUSet = CreateHelperCPUObjectSet( lvdJSON.fileNames, pluginLoader, availableHostMemoryHint * 1024 * 1024 );
	if ( set.CPUSet.VolumeData.size() == 0 ) {
		println( "No Volume Data" );
		return set;
	}

	//auto evaluator = make_shared<MyEvaluator>(set.CPUSet.VolumeData[0]->BlockDim(),
	//set.CPUSet.VolumeData[0]->BlockSize(),
	//availableDeviceMemoryHint * 1024*1024);

	//const auto textureSize = evaluator->EvalPhysicalTextureSize();
	auto deviceMemoryHint = deviceMemoryEvaluator( Vec3i( set.CPUSet.VolumeData[ 0 ]->BlockSize() ) );
	const auto textureCount = deviceMemoryHint.w;
	const auto textureBlockDim = Size3( Vec3i( deviceMemoryHint ) );

	const auto textureSize = set.CPUSet.VolumeData[ 0 ]->BlockSize() * textureBlockDim;

	size_t pageTableTotalEntries = 0;
	size_t hashBufferTotalBlocks = 0;
	size_t idBufferTotalBlocks = 0;
	const auto &cpuVolumeData = set.CPUSet.VolumeData;
	const auto lodCount = cpuVolumeData.size();
	for ( int i = 0; i < cpuVolumeData.size(); i++ ) {
		const auto blocks = cpuVolumeData[ i ]->BlockDim().Prod();
		pageTableTotalEntries += blocks;  // *sizeof(MappingTableManager::PageTableEntry);
		hashBufferTotalBlocks += blocks;  // *sizeof(uint32_t);
		idBufferTotalBlocks += blocks;	  // *sizeof(uint32_t);
	}

	/// [1] Create Page Table Buffer and binding
	set.GPUSet.GLPageTableBuffer = gl.CreateBuffer();
	const size_t pageTableBufferBytes = pageTableTotalEntries * sizeof( MappingTableManager::PageTableEntry );
	GL_EXPR( glNamedBufferStorage( set.GPUSet.GLPageTableBuffer, pageTableBufferBytes, nullptr, storage_flags ) );
	set.GPUSet.PageTableBufferPersistentMappedPointer = (uint32_t *)glCall_MapBufferRangeHelperFunc( set.GPUSet.GLPageTableBuffer, GL_ARRAY_BUFFER, 0, pageTableBufferBytes, mapping_flags );
	set.GPUSet.PageTableBufferBytes = pageTableBufferBytes;

	/// [2] Create Atomic Buffer and bingding
	const size_t atomicBufferBytes = lodCount * sizeof( uint32_t );
	set.GPUSet.GLAtomicCounterBuffer = gl.CreateBuffer();
	vector<uint32_t> zero( lodCount, 0 );
	GL_EXPR( glNamedBufferStorage( set.GPUSet.GLAtomicCounterBuffer, atomicBufferBytes, nullptr, storage_flags ) );
	set.GPUSet.AtomicCounterBufferPersistentMappedPointer = (uint32_t *)glCall_MapBufferRangeHelperFunc( set.GPUSet.GLAtomicCounterBuffer, GL_ATOMIC_COUNTER_BUFFER, 0, atomicBufferBytes, mapping_flags );
	set.GPUSet.AtomicCounterBufferBytes = atomicBufferBytes;

	///[3] Create ID Buffer and binding
	const auto idBufferBytes = idBufferTotalBlocks * sizeof( uint32_t );
	set.GPUSet.GLBlockIDBuffer = gl.CreateBuffer();
	GL_EXPR( glNamedBufferStorage( set.GPUSet.GLBlockIDBuffer, idBufferBytes, nullptr, storage_flags ) );
	set.GPUSet.BlockIDBufferPersistentMappedPointer = (uint32_t *)glCall_MapBufferRangeHelperFunc( set.GPUSet.GLBlockIDBuffer, GL_SHADER_STORAGE_BUFFER, 0, idBufferBytes, mapping_flags );
	set.GPUSet.BlockIDBufferBytes = idBufferBytes;

	///[4] Create Hash Buffer and binding
	//vector<uint32_t> emptyBuffer( hashBufferTotalBlocks, 0 );
	set.GPUSet.GLHashBuffer = gl.CreateBuffer();
	const auto hashBufferBytes = hashBufferTotalBlocks * sizeof( uint32_t );
	GL_EXPR( glNamedBufferStorage( set.GPUSet.GLHashBuffer, hashBufferBytes, nullptr, storage_flags ) );
	set.GPUSet.HashBufferPersistentMappedPointer = (uint32_t *)glCall_MapBufferRangeHelperFunc( set.GPUSet.GLHashBuffer, GL_SHADER_STORAGE_BUFFER, 0, hashBufferBytes, mapping_flags );
	set.GPUSet.HashBufferBytes = hashBufferBytes;

	///[5] Create LOD Info Buffer and binding
	const auto &lodInfo = set.CPUSet.LODInfoCPUBuffer;
	const auto lodInfoBytes = sizeof( _std140_layout_LODInfo ) * lodInfo.size();
	set.GPUSet.GLLODInfoBuffer = gl.CreateBuffer();
	GL_EXPR( glNamedBufferStorage( set.GPUSet.GLLODInfoBuffer, lodInfoBytes, nullptr, storage_flags ) );
	set.GPUSet.LODInfoBufferBytes = lodInfoBytes;

	// [6] Create Volume Texture Cache and binding texture unit 1,2,3,4
	auto &volumeDataTexture = set.GPUSet.GLVolumeTexture;
	for ( int i = 0; i < textureCount; i++ ) {
		auto texture = gl.CreateTexture( GL_TEXTURE_3D );
		GL_EXPR( glTextureStorage3D( texture, 1, GL_R8, textureSize.x, textureSize.y, textureSize.z ) );
		volumeDataTexture.push_back( std::move( texture ) );
	}

	// [7] Create Mapping Table manager
	vector<LODPageTableInfo> pageTableInfos;
	for ( int i = 0; i < lodCount; i++ ) {
		LODPageTableInfo info;
		info.virtualSpaceSize = Vec3i( cpuVolumeData[ i ]->BlockDim() );
		info.offset = set.CPUSet.LODInfoCPUBuffer[ i ].pageTableOffset;
		pageTableInfos.push_back( info );
	}
	const auto pageTablePtr = set.GPUSet.PageTableBufferPersistentMappedPointer;
	for ( int i = 0; i < lodCount; i++ ) {
		pageTableInfos[ i ].external = (MappingTableManager::PageTableEntry *)pageTablePtr + pageTableInfos[ i ].offset;
	}

	set.MappingManager = make_shared<MappingTableManager>( pageTableInfos,	// Create Mapping table for lods
														   textureBlockDim,
														   textureCount );

	const size_t volumeTextureMemoryUsage = textureSize.Prod() * textureCount;
	//PrintVideoMemoryUsageInfo(std::cout,set,volumeTextureMemoryUsage);
	PrintVideoMemoryUsageInfo( std::cout, set, volumeTextureMemoryUsage );

	return set;
}
bool glCall_Refine( HelperObjectSet &set,
					vector<uint32_t> &missedBlockIDPool,
					vector<BlockDescriptor> &descs )
{
	GL_EXPR( glFinish() );
	assert( set.GPUSet.AtomicCounterBufferPersistentMappedPointer );
	assert( set.GPUSet.BlockIDBufferPersistentMappedPointer );
	assert( set.GPUSet.HashBufferPersistentMappedPointer );
	assert( set.MappingManager );
	bool refined = true;
	const auto &lodInfo = set.CPUSet.LODInfoCPUBuffer;
	auto &cpuVolumeData = set.CPUSet.VolumeData;
	const auto lodCount = cpuVolumeData.size();
	for ( int curLod = 0; curLod < lodCount; curLod++ ) {
		//missedBlockIDCache.clear();
		const auto counter = set.GPUSet.AtomicCounterBufferPersistentMappedPointer;
		const size_t curLodMissedBlockCount = *( counter + curLod );
		if ( curLodMissedBlockCount == 0 )	// render finished
			continue;
		// Bounding block
		//blocks = ( std::min )( memoryEvaluators->EvalPhysicalBlockDim().Prod() * memoryEvaluators->EvalPhysicalTextureCount(), blocks );
		refined = false;

		missedBlockIDPool.resize( curLodMissedBlockCount );
		memcpy( missedBlockIDPool.data(), set.GPUSet.BlockIDBufferPersistentMappedPointer + lodInfo[ curLod ].idBufferOffset, sizeof( uint32_t ) * curLodMissedBlockCount );
		const auto dim = cpuVolumeData[ curLod ]->BlockDim();
		const auto physicalBlockCount = dim.Prod();
		vector<VirtualMemoryBlockIndex> virtualSpaceAddress;
		virtualSpaceAddress.reserve( curLodMissedBlockCount );
		descs.clear();
		//descs.reserve( curLodMissedBlockCount );
		//println( "lod: {}, blocks: {}", curLod, blocks );
		for ( int i = 0; i < curLodMissedBlockCount && i < physicalBlockCount; i++ ) {
			virtualSpaceAddress.emplace_back( missedBlockIDPool[ i ], dim.x, dim.y, dim.z );
		}

		const auto physicalSpaceAddress = set.MappingManager->UpdatePageTable( curLod, virtualSpaceAddress );

		for ( int i = 0; i < physicalSpaceAddress.size(); i++ ) {
			descs.emplace_back( physicalSpaceAddress[ i ], virtualSpaceAddress[ i ] );
		}

		const auto blockSize = cpuVolumeData[ curLod ]->BlockSize();
		for ( int i = 0; i < descs.size(); i++ ) {
			const auto posInCache = Vec3i( blockSize ) * descs[ i ].Value().ToVec3i();
			const auto d = cpuVolumeData[ curLod ]->GetPage( descs[ i ].Key() );
			const auto texHandle = set.GPUSet.GLVolumeTexture[ descs[ i ].Value().GetPhysicalStorageUnit() ].GetGLHandle();
			GL_EXPR( glTextureSubImage3D( texHandle, 0, posInCache.x, posInCache.y, posInCache.z, blockSize.x, blockSize.y, blockSize.z, GL_RED, GL_UNSIGNED_BYTE, d ) );
		}
	}
	glCall_ClearObjectSet( set );
	return refined;
}

void PrintVideoMemoryUsageInfo( std::ostream &os, const HelperObjectSet &set,
								size_t volumeTextureMemoryUsage )
{
	//const size_t volumeTextureMemoryUsage = memoryEvaluators->EvalPhysicalTextureSize().Prod() * memoryEvaluators->EvalPhysicalTextureCount();
	size_t pageTableBufferBytes = 0;
	size_t totalCPUMemoryUsage = 0;
	const auto lodCount = set.CPUSet.VolumeData.size();
	auto &cpuVolumeData = set.CPUSet.VolumeData;
	auto &mappingTableManager = set.MappingManager;
	auto &lodInfo = set.CPUSet.LODInfoCPUBuffer;
	for ( int i = 0; i < lodCount; i++ ) {
		fprintln( os, "===========LOD[{}]==============", i );
		fprintln( os, "Data Resolution: {}", cpuVolumeData[ i ]->DataSizeWithoutPadding() );
		fprintln( os, "Block Dimension: {}", cpuVolumeData[ i ]->BlockDim() );
		fprintln( os, "Block Size: {}", cpuVolumeData[ i ]->BlockSize() );
		fprintln( os, "Data Size: {.2} GB", ( cpuVolumeData[ i ]->BlockDim() * cpuVolumeData[ i ]->BlockSize() ).Prod() * 1.0 / 1024 / 1024 / 1024 );
		fprintln( os, "CPU Memory Usage: {.2} GB", cpuVolumeData[ i ]->CPUCacheSize().Prod() * 1.0 / 1024 / 1024 / 1024 );

		const auto blocks = cpuVolumeData[ i ]->BlockDim().Prod();
		fprintln( os, "Hash Memory Usage: {}, Offset: {}", blocks * sizeof( uint32_t ), lodInfo[ i ].hashBufferOffset );
		fprintln( os, "IDBuffer Memory Usage: {}, Offset: {}", blocks * sizeof( uint32_t ), lodInfo[ i ].idBufferOffset );
		fprintln( os, "PageTable Memory Usage: {}, Offset: {}", mappingTableManager->GetBytes( i ), lodInfo[ i ].pageTableOffset );

		pageTableBufferBytes += mappingTableManager->GetBytes( i );
		totalCPUMemoryUsage += cpuVolumeData[ i ]->CPUCacheSize().Prod();
	}

	const auto totalGPUMemoryUsage = pageTableBufferBytes +
									 volumeTextureMemoryUsage +
									 set.GPUSet.BlockIDBufferBytes +
									 set.GPUSet.HashBufferBytes;

	//println( "BlockDim: {} | Texture Size: {}", memoryEvaluators->EvalPhysicalBlockDim(), memoryEvaluators->EvalPhysicalTextureSize() );
	fprintln( os, "------------Summary Memory Usage ---------------" );
	fprintln( os, "Data Resolution: {}", cpuVolumeData[ 0 ]->DataSizeWithoutPadding() );
	fprintln( os, "Volume Texture Memory Usage: {} Bytes = {.2} MB", volumeTextureMemoryUsage, volumeTextureMemoryUsage * 1.0 / 1024 / 1024 );
	fprintln( os, "Page Table Memory Usage: {} Bytes = {.2} MB", pageTableBufferBytes, pageTableBufferBytes * 1.0 / 1024 / 1024 );
	fprintln( os, "Total ID Buffer Block Memory Usage: {} Bytes = {.2} MB", set.GPUSet.BlockIDBufferBytes, set.GPUSet.BlockIDBufferBytes * 1.0 / 1024 / 1024 );
	fprintln( os, "Total Hash Buffer Block Memory Usage: {} Bytes = {.2} MB", set.GPUSet.HashBufferBytes, set.GPUSet.HashBufferBytes * 1.0 / 1024 / 1024 );
	fprintln( os, "Total Volume Data GPU Memory Usage: {} Bytes = {.2} GB", totalGPUMemoryUsage, totalGPUMemoryUsage * 1.0 / 1024 / 1024 / 1024 );
	fprintln( os, "Total CPU Memory Usage: {} Bytes = {.2} GB", totalCPUMemoryUsage, totalCPUMemoryUsage * 1.0 / 1024 / 1024 / 1024 );
	fprintln( os, "================================" );
}

}  // namespace

int main( int argc, char **argv )
{
	Vec2i windowSize;
	cmdline::parser a;
	a.add<int>( "width", 'w', "width of window", false, 1024 );
	a.add<int>( "height", 'h', "height of window", false, 768 );
	a.add<size_t>( "hmem", '\0', "specifices available host memory in MB", false, 8000 );
	a.add<size_t>( "dmem", '\0', "specifices available device memory in MB", false, 50 );
	a.add<string>( "lods", '\0', "data json file", false );
	a.add<string>( "cam", '\0', "camera json file", false );
	a.add<string>( "tf", '\0', "transfer function text file", false );
	a.add<string>( "pd", '\0', "specifies plugin load directoy", false, "plugins" );
	a.parse_check( argc, argv );


	windowSize.x = a.get<int>( "width" );
	windowSize.y = a.get<int>( "height" );
	auto lodsFileName = a.get<string>( "lods" );
	auto camFileName = a.get<string>( "cam" );
	auto tfFileName = a.get<string>( "tf" );

	// Initialize OpenGL, including context, api and window. GL commands are callable after GL object is created
	auto gl = GL::NEW();

	const int gpuMem = gl->GetGLProperties().MAX_GPU_MEMORY_SIZE;
	size_t availableHostMemory = 0;
	size_t availableDeviceMemory = 0;
	if ( gpuMem >= 0 ) {
		availableDeviceMemory = ( std::min )( size_t( gpuMem ), a.get<size_t>( "dmem" ) * 1024 * 1024 );
	} else {
		println( "Your OpenGL driver does not support extension of querying device memory." );
		availableDeviceMemory = a.get<size_t>( "dmem" );
	}
	availableHostMemory = a.get<size_t>( "hmem" );

	auto de = [availableDeviceMemory]( const Vector3i &blockSize ) {
		int textureUnitCount = 4;
		const auto maxBytesPerTexUnit = availableDeviceMemory * 3 / 4 / textureUnitCount;
		int d = 0;
		while ( ++d ) {
			const auto memory = d * d * d * blockSize.Prod();
			if ( memory >= maxBytesPerTexUnit )
				break;
		}
		d--;
		//while ( d > 10 )
		//{
		//	d /= 2;
		//	textureUnitCount++;
		//}
		return Vec4i{ d, d, d, textureUnitCount };
	};

	println( "Window Size: [{}, {}]", windowSize.x, windowSize.y );
	println( "Data configuration file: {}", lodsFileName );
	println( "Camera configuration file: {}", camFileName );
	println( "Transfer Function configuration file: {}", tfFileName );
	println( "Specified Avalable Host Memory Hint: {}", availableHostMemory );
	println( "Specified Avalable Device Memory Hint: {}", availableDeviceMemory );
	println( "Plugin directory: {}", a.get<string>( "pd" ) );
  

	println( "Load Plugin..." );
	vm::PluginLoader::LoadPlugins( a.get<string>( "pd" ) );	 // load plugins from the directory

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
	bool RenderPause = true;
	bool FPSCamera = true;

	Vec3i dataResolution;
	vector<uint32_t> missedBlockHostPool; /*Reported missed block ID cache*/
	vector<BlockDescriptor> blockDescHostPool;
	HelperObjectSet set;

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

	// // test volume texture
	// RawReader reader("/home/ysl/data/s1_128_128_128.raw",{128,128,128},1);
	// auto size = reader.GetDimension().Prod() * reader.GetElementSize();
	// std::unique_ptr<uint8_t[]> buf(new uint8_t[size]);
	// if(reader.readRegion({0,0,0},{128,128,128},buf.get()) != size){
	// 	println("Failed to read raw data");
	// 	exit(-1);
	// }
	// auto testTexture = glCall_CreateVolumeTexture(*gl,128,128,128);
	// assert(testTexture.Valid());
	// GL_EXPR(glTextureSubImage3D(testTexture,0,0,0,0,128,128,128,GL_RED,GL_UNSIGNED_BYTE,buf.get()));

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
	// vs = GetTextFromFile("resources/screenquad_v.glsl");
	// pVS = vs.c_str();
	// vShader = glCall_CreateShaderAndCompileHelper(*gl,GL_VERTEX_SHADER,pVS);

	// fs = GetTextFromFile("resources/naiveraycast_f.glsl");
	// pFS = fs.c_str();
	// fShader = glCall_CreateShaderAndCompileHelper(*gl,GL_FRAGMENT_SHADER,pFS);

	// auto raycastingProgram = gl->CreateProgram();
	// GL_EXPR(glAttachShader(raycastingProgram,vShader));
	// GL_EXPR(glAttachShader(raycastingProgram,fShader));
	// glCall_LinkProgramAndCheckHelper(raycastingProgram);

	//[2] out-of-core raycasting shader
	vs = GetTextFromFile( "resources/screenquad_v.glsl" );
	pVS = vs.c_str();
	vShader = glCall_CreateShaderAndCompileHelper( *gl, GL_VERTEX_SHADER, pVS );
	fs = GetTextFromFile( "resources/blockraycasting_f.glsl" );
	pFS = fs.c_str();
	fShader = glCall_CreateShaderAndCompileHelper( *gl, GL_FRAGMENT_SHADER, pFS );

	auto outofcoreProgram = gl->CreateProgram();
	GL_EXPR( glAttachShader( outofcoreProgram, vShader ) );
	GL_EXPR( glAttachShader( outofcoreProgram, fShader ) );
	glCall_LinkProgramAndCheckHelper( outofcoreProgram );

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

	/* Texture unit and image unit binding*/
	// [1] binds texture unit : see the raycasting shader for details
	GL_EXPR( glBindTextureUnit( 0, GLTFTexture ) );	 // binds texture unit 0 for tf texture
	//GL_EXPR(glBindTextureUnit(1,testTexture));              // binds texture unit 1 for volume texture
	// [2] binds image unit : see the raycasting shader for details
	GL_EXPR( glBindImageTexture( 0, GLEntryPosTexture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F ) );  // binds image unit 0 for entry texture (read and write)
	GL_EXPR( glBindImageTexture( 1, GLExitPosTexture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F ) );   // binds image unit 1 for exit texture (read and write)
	GL_EXPR( glBindImageTexture( 2, GLResultTexture, 0, GL_FALSE, 0, GL_READ_WRITE, GL_RGBA32F ) );	   // binds image unit 2 for result texture (read and write)

	/* Uniforms binding for program*/
	//[1] position shader
	glCall_CameraUniformUpdate( camera, ModelTransform, positionGenerateProgram, outofcoreProgram );  // camera-related uniforms for position program
	//[2] ray-casting shader
	GL::GLSampler sampler = gl->CreateSampler();
	GL_EXPR( glSamplerParameterf( sampler, GL_TEXTURE_MIN_FILTER, GL_LINEAR ) );  // filter type
	GL_EXPR( glSamplerParameterf( sampler, GL_TEXTURE_MAG_FILTER, GL_LINEAR ) );
	GL_EXPR( glSamplerParameterf( sampler, GL_TEXTURE_WRAP_R, GL_CLAMP_TO_EDGE ) );	 // boarder style
	GL_EXPR( glSamplerParameterf( sampler, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE ) );
	GL_EXPR( glSamplerParameterf( sampler, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE ) );

	// GL_EXPR(glProgramUniform1i(raycastingProgram,3,0)); // sets location = 0 (tf texture sampler) as tf texture unit 0
	// GL_EXPR(glBindSampler(0,sampler));
	// GL_EXPR(glProgramUniform1i(raycastingProgram,4,1)); // sets location = 1 (volume texture sampler) as volume texture unit 1
	// GL_EXPR(glBindSampler(1,sampler));

	// GL_EXPR(glProgramUniform1i(raycastingProgram,0,0)); // sets location = 2 as entry image unit 0
	// GL_EXPR(glProgramUniform1i(raycastingProgram,1,1)); // sets location = 3 as exit image unit 1
	// GL_EXPR(glProgramUniform1i(raycastingProgram,2,2)); // sets location = 4 as result image unit 2

	//[2] out-of-core raycasting shader
	GL_EXPR( glProgramUniform1i( outofcoreProgram, 0, 0 ) );  // sets location = 0 as entry image unit 0
	GL_EXPR( glProgramUniform1i( outofcoreProgram, 1, 1 ) );  // sets location = 1 as exit image unit 1
	GL_EXPR( glProgramUniform1i( outofcoreProgram, 2, 2 ) );  // sets location = 2 as result image unit 2
	GL_EXPR( glProgramUniform1i( outofcoreProgram, 4, 0 ) );  // sets location = 4 (tf texture sampler) as tf texture unit 0

	GL_EXPR( glProgramUniform1i( outofcoreProgram, 5, 1 ) );  // sets location = 1 (volume texture sampler) as tf texture unit 0
	GL_EXPR( glBindSampler( 1, sampler ) );
	GL_EXPR( glProgramUniform1i( outofcoreProgram, 6, 2 ) );  // sets location = 2 (volume texture sampler) as tf texture unit 0
	GL_EXPR( glBindSampler( 2, sampler ) );
	GL_EXPR( glProgramUniform1i( outofcoreProgram, 7, 3 ) );  // sets location = 3 (volume texture sampler) as tf texture unit 0
	GL_EXPR( glBindSampler( 3, sampler ) );
	GL_EXPR( glProgramUniform1i( outofcoreProgram, 8, 4 ) );  // sets location = 4 (volume texture sampler) as tf texture unit 0
	GL_EXPR( glBindSampler( 4, sampler ) );

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

			glCall_CameraUniformUpdate( camera, ModelTransform, positionGenerateProgram, outofcoreProgram );
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
			} else if ( key == KeyButton::Key_R ) {
				using std::default_random_engine;
				using std::uniform_int_distribution;
				default_random_engine e( time( 0 ) );
				uniform_int_distribution<int> u( 0, 100000 );
				camera.GetViewMatrixWrapper().SetPosition( Point3f( u( e ) % dataResolution.x, u( e ) % dataResolution.y, u( e ) & dataResolution.z ) );
				println( "A random camera position generated" );
			} else if ( key == KeyButton::Key_F ) {
				FPSCamera = !FPSCamera;
				if ( FPSCamera ) {
					println( "Switch to FPS camera manipulation" );
				} else {
					println( "Switch to track ball manipulation" );
				}

			} else if ( key == KeyButton::Key_P ) {
				//intermediateResult->SaveTextureAs( "E:\\Desktop\\lab\\res_" + GetTimeStampString() + ".png" );
				//glCall_SaveTextureAsImage();
				println( "Save screen shot" );
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
					glCall_CameraUniformUpdate( camera, ModelTransform, positionGenerateProgram, outofcoreProgram );
					//println("camera change");
					//PrintCamera(camera);
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
			} else if ( extension == ".lods" ) {
				//UpdateCPUVolumeData(each);
				set = glCall_SetupResources(*gl,each,*PluginLoader::GetPluginLoader(),availableHostMemory,de);
				glCall_ResourcesBinding(set,outofcoreProgram);
				glCall_ClearObjectSet(set);

				RenderPause = false;
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

	//lodsFileName ="/home/ysl/data/s1.brv";
	if ( lodsFileName.empty() == false ) {
		try {
			set = glCall_SetupResources( *gl, lodsFileName, *PluginLoader::GetPluginLoader(), availableHostMemory, de );
			glCall_ResourcesBinding( set, outofcoreProgram );
			glCall_ClearObjectSet( set );
			RenderPause = false;  // If data is loaded successfully, starts rendering.
		} catch ( exception &e ) {
			println( "Cannot open .lods file: {}", e.what() );
		}
	}

	while ( gl->Wait() == false && RenderPause ) { gl->DispatchEvent(); }

	/*Configuration rendering state*/
	const float zeroRGBA[] = { 0.f, 0.f, 0.f, 0.f };
	GL_EXPR( glBlendFuncSeparate( GL_ONE, GL_ONE, GL_ONE, GL_ONE ) );  // Just add dst to src : (srcRBG * 1 + dstRGB * 1,srcAlpha * 1 + dstRGB * 1), so the backround color must be cleared as 0
	GL_EXPR( glFrontFace( GL_CCW ) );
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

		//GL_EXPR(glUseProgram(raycastingProgram));
		GL_EXPR( glUseProgram( outofcoreProgram ) );
		GL_EXPR( glNamedFramebufferDrawBuffer( GLFramebuffer, GL_COLOR_ATTACHMENT2 ) );	 // draw into result texture
		//While out-of-core refine
		do {
			GL_EXPR( glDrawArrays( GL_TRIANGLE_STRIP, 0, 4 ) );	 // vertex is hard coded in shader
		} while ( glCall_Refine( set, missedBlockHostPool, blockDescHostPool ) == false );

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
