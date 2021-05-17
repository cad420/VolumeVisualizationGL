#include <VMFoundation/blockarray.h>
#include <VMat/numeric.h>
#include <random>
#include <fstream>
#include <lvdfile.h>

TEST( test_lvdwr, basic )
{
	std::string fileName = "test.lvd";

	vm::Vec3i dataSize{256,256,256};
	int padding = 2;
	int sideInLog = 6;

	vm::LVDFile reader( fileName, sideInLog, dataSize, padding );

	auto blockCount = reader.BlockCount();
	size_t blockSide = reader.BlockSize();
	auto blockDim = reader.SizeByBlock();

	std::default_random_engine e;
	std::uniform_int_distribution<int> u( 0, blockCount - 1 );

	const auto size = reader.Size();
	const auto sideZ = size.z;
	const auto sideY = size.y;
	const auto sideX = size.x;

	constexpr double Pi = 3.1415926535;
	constexpr auto minValue = 0.0031238;
	constexpr auto maxValue = 3.4641;
	const auto A = std::sqrt( 3 );
	const auto B = std::sqrt( 2 );
	const auto C = 1;
	std::unique_ptr<char[]> blockBuf( new char[blockSide * blockSide*blockSide] );
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

	reader.Close();

	vm::LVDFile reader2( fileName, sideInLog, dataSize,padding );

	ASSERT_EQ(reader.BlockSizeInLog(),sideInLog);
	ASSERT_EQ(reader.GetBlockPadding() ,padding);
}