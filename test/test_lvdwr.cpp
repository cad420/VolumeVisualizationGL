#include <gtest/gtest.h>
#include <random>
#include <fstream>
#include <sstream>

#include <lvdfile.h>
#include <jsondef.hpp>
#include <VMUtils/vmnew.hpp>
#include <VMat/numeric.h>

#include <VMFoundation/largevolumecache.h>
#include <VMFoundation/pluginloader.h>
#include <VMFoundation/logger.h>


void OutputLods( const std::string &fileName, const LVDJSONStruct &json )
{
	std::ofstream lodsfile( fileName );
	ASSERT_TRUE( lodsfile.is_open() );
	vm::json::Writer jsonwriter;
	jsonwriter.write( lodsfile, json );
}

template<typename T>
void GenerateABCBlock(int blockSide, size_t sideX,size_t sideY,size_t sideZ, int blockCount, char * src)
{
	constexpr double Pi = 3.1415926535;
	constexpr auto minValue = 0.0031238;
	constexpr auto maxValue = 3.4641;
	const auto A = std::sqrt( 3 );
	const auto B = std::sqrt( 2 );
	const auto C = 1;
	std::unique_ptr<char[]> blockBuf( new char[blockSide * blockSide* blockSide] );
	for ( int i = 0; i < blockCount; i++ ) {
		for ( int z = 0; z < blockSide; z++ ) {
			for ( int y = 0; y < blockSide; y++ ) {
				for ( int x = 0; x < blockSide; x++ ) {
					const auto dim = vm::Dim( i, {blockDim.x, blockDim.y} );
					const auto globalx = x + dim.x * blockSide;
					const auto globaly = y + dim.y * blockSide;
					const auto globalz = z + dim.z * blockSide;
					const auto index = globalx + globaly * sideX + globalz * sideX * sideY;
					const double X = globalx * 2 * Pi / sideX, Y = globaly * 2 * Pi / sideY, Z = globalz * 2 * Pi / sideZ;
					const auto value = std::sqrt( 6 + 2 * A * std::sin( Z ) * std::cos( Y ) + 2 * B * std::sin( Y ) * std::cos( X ) + 2 * std::sqrt( 6 ) * sin( X ) * std::cos( Z ) );
					blockBuf[ index ] = ( ( value - minValue ) / ( maxValue - minValue ) * 255 + 0.5 );
					reader.WriteBlock(blockBuf.get(), i, 0);
				}
			}
		}
	}
}

TEST(test, test_cachewrite){
	using namespace vm;
	vm::Vec3i dataSize{256,256,256};
	int blockSideInLog = 6;
	int padding = 2;
	int blockSide = 1<<blockSideInLog;

	const char * fileName = "test_cache_write.lvd";

	PluginLoader::LoadPlugins("plugins");
	Ref<I3DBlockFilePluginInterface> p = PluginLoader::GetPluginLoader()->CreatePlugin<I3DBlockFilePluginInterface>( ".lvd" );
	if ( !p ) {
		LOG_FATAL<< "Failed to load plugin to read lvd file";
	}


	auto g = [ &blockSide, &padding ]( int x ) { return vm::RoundUpDivide(x,blockSide - 2ULL * padding); };
	vm::Size3 realBlockDim;
	realBlockDim.x = g(dataSize.x);
	realBlockDim.y = g(dataSize.y);
	realBlockDim.z = g(dataSize.z);

	auto f = [ &blockSide, &padding ]( int x ) { return vm::RoundUpDivide(x,blockSide - 2ULL * padding)*blockSide; };
	vm::Size3 realDataSize;
	realDataSize.x = f( dataSize.x );
	realDataSize.y = f( dataSize.y );
	realDataSize.z = f( dataSize.z );

	Block3DDataFileDesc Desc;
	Desc.IsDataSize = true;
	Desc.BlockSideInLog = blockSideInLog;
	Desc.DataSize[0] = dataSize.x;
	Desc.DataSize[1] = dataSize.y;
	Desc.DataSize[2] = dataSize.z;
	Desc.Padding = padding;
	Desc.FileName = fileName;

	ASSERT_TRUE(p->Create( &Desc ));

	vm::Size3 physicalPageDim{8,1,1};
	Ref<Block3DCache> writer = VM_NEW<Block3DCache>( p, [&physicalPageDim]( I3DBlockDataInterface *p ) {return physicalPageDim;});
	ASSERT_TRUE(writer != nullptr);

	size_t pageSize = writer->GetPageSize();
	ASSERT_EQ(pageSize, size_t(blockSide) * blockSide * blockSide);

	ASSERT_EQ(writer->GetPhysicalPageCount(), physicalPageDim.Prod());
	ASSERT_EQ(writer->Padding(), padding);

	auto originalDataSize = writer->DataSizeWithoutPadding();
	ASSERT_EQ(originalDataSize.x, dataSize.x);
	ASSERT_EQ(originalDataSize.y, dataSize.y);
	ASSERT_EQ(originalDataSize.z, dataSize.z);

	auto dataBlockDim = writer->BlockDim();
	ASSERT_EQ(dataBlockDim.x, realBlockDim.x);
	ASSERT_EQ(dataBlockDim.y, realBlockDim.y);
	ASSERT_EQ(dataBlockDim.z, realBlockDim.z);

	auto blockSize = writer->BlockSize();
	ASSERT_EQ( blockSize.x, blockSide );
	ASSERT_EQ( blockSize.y, blockSide );
	ASSERT_EQ( blockSize.z, blockSide );

	const auto count = realBlockDim.Prod();
	std::default_random_engine e;
	std::uniform_int_distribution<int> u( 0,count-1);
	std::vector<char> testdata(count * blockSize.Prod());
	for (int i = 0; i < count; i++) {
		char val = u( e ) % 256;
		char *ptr = testdata.data() + i * blockSize.Prod();
		memset(ptr, val, blockSize.Prod() );
		writer->Write( ptr, i, false );
	}
	writer->Flush();
	writer = nullptr;
	p->Close();

	p->Open( Desc.FileName );
	Ref<Block3DCache> reader = VM_NEW<Block3DCache>( p, [&physicalPageDim]( I3DBlockDataInterface *p ) {return physicalPageDim;});


	
	for (int i = 0; i < count; i++) {
		char *ptr = testdata.data() + i *  blockSize.Prod();
		VirtualMemoryBlockIndex index(i,(int)dataBlockDim.x,(int)dataBlockDim.y,(int)dataBlockDim.z );
		auto data = (const char *)reader->GetPage( index );
		for (int i = 0; i < blockSize.Prod(); i++) {
			ASSERT_EQ( ptr[ i ], data[ i ] );
		}
	}

	LVDJSONStruct json;
	json.fileNames = { fileName };
	json.samplingRate = 0.0001;
	json.spacing = { 2, 2, 2 };

	OutputLods( "test.lods", json );

}

TEST( test_lvdwr, basic )
{
	//std::string fileName = "test.lvd";

	//vm::Vec3i dataSize{256,256,256};
	//int padding = 2;
	//int sideInLog = 6;

	//vm::LVDFile reader( fileName, sideInLog, dataSize, padding );

	//auto blockCount = reader.BlockCount();
	//size_t blockSide = reader.BlockSize();
	//auto blockDim = reader.SizeByBlock();

	//std::default_random_engine e;
	//std::uniform_int_distribution<int> u( 0, blockCount - 1 );

	//const auto size = reader.Size();
	//const auto sideZ = size.z;
	//const auto sideY = size.y;
	//const auto sideX = size.x;

	//constexpr double Pi = 3.1415926535;
	//constexpr auto minValue = 0.0031238;
	//constexpr auto maxValue = 3.4641;
	//const auto A = std::sqrt( 3 );
	//const auto B = std::sqrt( 2 );
	//const auto C = 1;
	//std::unique_ptr<char[]> blockBuf( new char[blockSide * blockSide*blockSide] );
	//for ( int i = 0; i < blockCount; i++ ) {
	//	for ( int z = 0; z < blockSide; z++ ) {
	//		for ( int y = 0; y < blockSide; y++ ) {
	//			for ( int x = 0; x < blockSide; x++ ) {
	//				const auto dim = vm::Dim( i, {blockDim.x, blockDim.y} );
	//				const auto globalx = x + dim.x * blockSide;
	//				const auto globaly = y + dim.y * blockSide;
	//				const auto globalz = z + dim.z * blockSide;
	//				const auto index = globalx + globaly * sideX + globalz * sideX * sideY;
	//				const double X = globalx * 2 * Pi / sideX, Y = globaly * 2 * Pi / sideY, Z = globalz * 2 * Pi / sideZ;
	//				const auto value = std::sqrt( 6 + 2 * A * std::sin( Z ) * std::cos( Y ) + 2 * B * std::sin( Y ) * std::cos( X ) + 2 * std::sqrt( 6 ) * sin( X ) * std::cos( Z ) );
	//				blockBuf[ index ] = ( ( value - minValue ) / ( maxValue - minValue ) * 255 + 0.5 );
	//				reader.WriteBlock(blockBuf.get(), i, 0);
	//			}
	//		}
	//	}
	//}

	//reader.Close();

	//vm::LVDFile reader2( fileName, sideInLog, dataSize,padding );

	//ASSERT_EQ(reader.BlockSizeInLog(),sideInLog);
	//ASSERT_EQ(reader.GetBlockPadding() ,padding);
}
