
#include <iostream>
#include <fstream>
#include <VMFoundation/libraryloader.h>
#include <VMFoundation/pluginloader.h>
#include <VMFoundation/logger.h>
#include "lvdfile.h"

namespace vm
{
void LVDFile::InitLVDIO()
{
#ifdef _WIN32
	lvdIO = PluginLoader::GetPluginLoader()->CreatePlugin<IMappingFile>( "windows" );
#else defined( __linux__ ) || defined( __APPLE__ )
	lvdIO = PluginLoader::GetPluginLoader()->CreatePlugin<IMappingFile>( "linux" );
#endif
	if ( lvdIO == nullptr )
		throw std::runtime_error( "can not load ioplugin" );
}

void LVDFile::InitInfoByHeader(const LVDFileHeader & header){
	int vx = header.dataDim[ 0 ];
	int vy = header.dataDim[ 1 ];
	int vz = header.dataDim[ 2 ];
	logBlockSize = header.blockLengthInLog;
	padding = header.padding;

	const int originalWidth = header.originalDataDim[ 0 ];
	const int originalHeight = header.originalDataDim[ 1 ];
	const int originalDepth = header.originalDataDim[ 2 ];

	if ( logBlockSize != LogBlockSize5 && logBlockSize != LogBlockSize6 && logBlockSize != LogBlockSize7 ) {
		std::cout << "Unsupported block size\n";
		validFlag = false;
		return;
	}

	const size_t aBlockSize = BlockSize();

	// aBlockSize must be power of 2, e.g. 32 or 64
	const int bx = ( ( vx + aBlockSize - 1 ) & ~( aBlockSize - 1 ) ) / aBlockSize;
	const int by = ( ( vy + aBlockSize - 1 ) & ~( aBlockSize - 1 ) ) / aBlockSize;
	const int bz = ( ( vz + aBlockSize - 1 ) & ~( aBlockSize - 1 ) ) / aBlockSize;

	vSize = vm::Size3( ( vx ), ( vy ), ( vz ) );
	bSize = vm::Size3( bx, by, bz );
	oSize = vm::Size3( originalWidth, originalHeight, originalDepth );

}
LVDFile::LVDFile( const std::string &fileName ) :
  validFlag( true ), lvdIO( nullptr )
{
	std::ifstream fileHandle;

	fileHandle.open( fileName, std::fstream::binary );
	if ( !fileHandle.is_open() ) {
		std::cout << "Can not open .lvd\n";
		validFlag = false;
		fileHandle.close();
		return;
	}

	unsigned char headerBuf[ LVD_HEADER_SIZE ];

	fileHandle.read( (char *)headerBuf, LVD_HEADER_SIZE );
	header.Decode( headerBuf );

	fileHandle.close();

	//uint32_t magicNumber;
	//fileHandle.read((char*)&magicNumber, sizeof(int));

	if ( header.magicNum != LVDFileMagicNumber ) {
		std::cout << " This is not a lvd file\n";
		validFlag = false;
		fileHandle.close();
		return;
	}

	//int vx, vy, vz, bx, by, bz;
	//int m_originalWidth, m_originalHeight, m_originalDepth;

	/*fileHandle.read((char*)&vx, sizeof(int));
		fileHandle.read((char*)&vy, sizeof(int));
		fileHandle.read((char*)&vz, sizeof(int));
		fileHandle.read((char*)&logBlockSize, sizeof(int));
		fileHandle.read((char*)&repeat, sizeof(int));
		fileHandle.read((char*)&m_originalWidth, sizeof(int));
		fileHandle.read((char*)&m_originalHeight, sizeof(int));
		fileHandle.read((char*)&m_originalDepth, sizeof(int));*/

	int vx = header.dataDim[ 0 ];
	int vy = header.dataDim[ 1 ];
	int vz = header.dataDim[ 2 ];
	logBlockSize = header.blockLengthInLog;
	padding = header.padding;

	const int originalWidth = header.originalDataDim[ 0 ];
	const int originalHeight = header.originalDataDim[ 1 ];
	const int originalDepth = header.originalDataDim[ 2 ];

	if ( logBlockSize != LogBlockSize5 && logBlockSize != LogBlockSize6 && logBlockSize != LogBlockSize7 ) {
		std::cout << "Unsupported block size\n";
		validFlag = false;
		return;
	}

	const size_t aBlockSize = BlockSize();

	// aBlockSize must be power of 2, e.g. 32 or 64
	const int bx = ( ( vx + aBlockSize - 1 ) & ~( aBlockSize - 1 ) ) / aBlockSize;
	const int by = ( ( vy + aBlockSize - 1 ) & ~( aBlockSize - 1 ) ) / aBlockSize;
	const int bz = ( ( vz + aBlockSize - 1 ) & ~( aBlockSize - 1 ) ) / aBlockSize;

	vSize = vm::Size3( ( vx ), ( vy ), ( vz ) );
	bSize = vm::Size3( bx, by, bz );
	oSize = vm::Size3( originalWidth, originalHeight, originalDepth );

	const std::size_t bytes = std::size_t( vx ) * vy * vz + LVD_HEADER_SIZE;

	InitLVDIO();
	lvdIO->Open( fileName, bytes, FileAccess::ReadWrite, MapAccess::ReadWrite );

	lvdPtr = lvdIO->MemoryMap( 0, bytes );
	if ( !lvdPtr ) throw std::runtime_error( "LVDReader: bad mapping" );
}

LVDFile::LVDFile( const std::vector<std::string> &fileName, const std::vector<int> &lods )
{
	std::vector<int> levelOfDetails;
	if ( lods.size() == 0 ) {
		for ( int i = 0; i < fileName.size(); i++ )
			levelOfDetails.push_back( i );
	} 
}
LVDFile::LVDFile( const std::string & fileName,int blockSideInLog, const Vec3i &dataSize, int padding )
{
	header.magicNum = LVDFileMagicNumber;
	header.blockLengthInLog = (uint32_t)blockSideInLog;
	header.padding = padding;
	if (blockSideInLog < 5 || blockSideInLog > 10) {
		LOG_FATAL << "Too large block size";
	}
	const auto blockSide = (1ULL << blockSideInLog);
	auto f = [ &blockSide, &padding ]( int x ) { return vm::RoundUpDivide(x,blockSide - 2ULL * padding)*blockSide; };
	const size_t dataX = f( dataSize.x );
	const size_t dataY = f( dataSize.y );
	const size_t dataZ = f( dataSize.z );
	header.dataDim[ 0 ] = dataX;
	header.dataDim[ 1 ] = dataY;
	header.dataDim[ 2 ] = dataZ;
	header.originalDataDim[ 0 ] = dataSize.x;
	header.originalDataDim[ 1 ] = dataSize.y;
	header.originalDataDim[ 2 ] = dataSize.z;

	unsigned char headerBuf[ LVD_HEADER_SIZE ];
	header.Encode();
	memcpy( headerBuf, header.Encode(), LVD_HEADER_SIZE );

	InitLVDIO();

	const auto fileSize = dataX * dataY * dataZ + LVD_HEADER_SIZE;

	lvdIO->Open( fileName.c_str(), fileSize, FileAccess::ReadWrite, MapAccess::ReadWrite );
	lvdPtr = lvdIO->MemoryMap( 0, fileSize );
	if ( !lvdPtr ) 
		throw std::runtime_error( "LVDReader: bad mapping" );

	memcpy(lvdPtr,headerBuf,LVD_HEADER_SIZE);
	lvdIO->Flush(lvdPtr,LVD_HEADER_SIZE,0);
	InitInfoByHeader(header);
}


void LVDFile::ReadBlock( char *dest, int blockId, int lod )
{
	const size_t blockCount = BlockDataCount();
	const auto d = lvdPtr + LVD_HEADER_SIZE;

	//fileHandle.seekg(blockCount * blockId + 36, std::ios::beg);
	memcpy( dest, d + blockCount * blockId, sizeof( char ) * blockCount );
	//fileHandle.read(dest, sizeof(char) * blockCount);
}

void LVDFile::WriteBlock( const char *src, int blockId, int lod )
{
	(void)lod;
	const size_t blockCount = BlockDataCount();
	const auto d = lvdPtr + LVD_HEADER_SIZE;
	memcpy(d + blockCount * blockId, src, sizeof( char ) * blockCount );
}

bool LVDFile::Flush( int blockId, int lod )
{
	assert( lvdPtr );
	(void)lod;
	const auto d = lvdPtr + LVD_HEADER_SIZE;
	const size_t blockCount = BlockDataCount();
	return lvdIO->Flush( d + blockCount * blockId, sizeof( char ) * blockCount, 0 );
}

bool LVDFile::Flush()
{
	LOG_CRITICAL << "LVDReader::Flush() -- Not implement yet";
	return false;
}

void LVDFile::Close()
{
	lvdIO = nullptr;
}

unsigned char *LVDFile::ReadBlock( int blockId, int lod )
{
	const size_t blockCount = BlockDataCount();
	const auto d = lvdPtr + LVD_HEADER_SIZE;
	return d + blockCount * blockId;
}

LVDFile::~LVDFile()
{
	//delete lvdIO;
}
}  // namespace ysl
