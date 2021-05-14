#include "lvdfilereader.h"
namespace vm
{
inline LVDFileReader::LVDFileReader( ::vm::IRefCnt *cnt ) :
  vm::EverythingBase<I3DBlockFilePluginInterface>( cnt )
{
}
inline void LVDFileReader::Open( const std::string &fileName )
{
	lvdReader = std::make_unique<LVDReader>( fileName );
	if ( lvdReader == nullptr ) {
		throw std::runtime_error( "failed to open lvd file" );
	}
}
inline Size3 LVDFileReader::Get3DPageSize() const
{
	const std::size_t len = lvdReader->BlockSize();
	return Size3{ len, len, len };
}
void LVDFileReader::Flush()
{
}
void LVDFileReader::Write( const void *page, size_t pageID, bool flush )
{
}
void LVDFileReader::Flush( size_t pageID )
{
}
}  // namespace vm
VM_REGISTER_PLUGIN_FACTORY_IMPL( LVDFileReaderFactory )
EXPORT_PLUGIN_FACTORY_IMPLEMENT( LVDFileReaderFactory )