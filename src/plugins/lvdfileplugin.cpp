#include "lvdfileplugin.h"
namespace vm
{
inline LVDFilePlugin::LVDFilePlugin( ::vm::IRefCnt *cnt ) :
  vm::EverythingBase<I3DBlockFilePluginInterface>( cnt )
{
}

bool LVDFilePlugin::Create( const Block3DDataFileDesc *desc ){
	lvdReader = std::make_unique<LVDFile>(
		desc->FileName,
		desc->BlockSideInLog,
		vm::Vec3i(desc->DataSize[0],desc->DataSize[1],desc->DataSize[2]),
		desc->Padding);
    return lvdReader != nullptr;
}
inline void LVDFilePlugin::Open( const std::string &fileName )
{
	lvdReader = std::make_unique<LVDFile>( fileName );
	if ( lvdReader == nullptr ) {
		throw std::runtime_error( "failed to open lvd file" );
	}
}
inline Size3 LVDFilePlugin::Get3DPageSize() const
{
	const std::size_t len = lvdReader->BlockSize();
	return Size3{ len, len, len };
}
void LVDFilePlugin::Flush()
{
}
void LVDFilePlugin::Write( const void *page, size_t pageID, bool flush )
{
	lvdReader->WriteBlock((const char*)page,pageID,0);
	if(flush){
		lvdReader->Flush(pageID,0);
	}
}
void LVDFilePlugin::Flush( size_t pageID )
{
	lvdReader->Flush(pageID,0);
}
}  // namespace vm
VM_REGISTER_PLUGIN_FACTORY_IMPL( LVDFilePluginFactory )
EXPORT_PLUGIN_FACTORY_IMPLEMENT( LVDFilePluginFactory )