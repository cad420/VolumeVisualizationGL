#pragma once
#include "lvdfile.h"
#include <VMUtils/vmnew.hpp>
#include <VMUtils/ieverything.hpp>
#include <VMCoreExtension/plugin.h>
#include <VMCoreExtension/i3dblockfileplugininterface.h>

namespace vm
{
class LVDFilePlugin : public vm::EverythingBase<I3DBlockFilePluginInterface>
{
	std::unique_ptr<LVDFile> lvdReader;

public:
	LVDFilePlugin( ::vm::IRefCnt *cnt );
	void Open( const std::string &fileName ) override;
	bool Create( const Block3DDataFileDesc *desc ) override;
	void Close() override;
	const void *GetPage( size_t pageID ) override { return lvdReader->ReadBlock( pageID ); }
	size_t GetPageSize() const override { return lvdReader->BlockSize(); }
	size_t GetPhysicalPageCount() const override { return lvdReader->BlockCount(); }
	size_t GetVirtualPageCount() const override { return lvdReader->BlockCount(); }

	int GetPadding() const override { return lvdReader->GetBlockPadding(); }
	Size3 GetDataSizeWithoutPadding() const override { return lvdReader->OriginalDataSize(); }
	Size3 Get3DPageSize() const override;
	int Get3DPageSizeInLog() const override { return lvdReader->BlockSizeInLog(); }
	Size3 Get3DPageCount() const override { return lvdReader->SizeByBlock(); }

	void Flush() override;

	 void Write( const void *page, size_t pageID, bool flush )override;

	 void Flush( size_t pageID ) override;

private:
};

}  // namespace vm

class LVDFilePluginFactory : public vm::IPluginFactory
{
public:
	DECLARE_PLUGIN_FACTORY( "visualman.blockdata.io" )
	std::vector<std::string> Keys() const override { return { ".lvd" }; }
	::vm::IEverything *Create( const std::string &key ) override
	{
		if ( key == ".lvd" ) {
			return VM_NEW<vm::LVDFilePlugin>();
		}
		return nullptr;
	}
};

VM_REGISTER_PLUGIN_FACTORY_DECL( LVDFilePluginFactory )
EXPORT_PLUGIN_FACTORY( LVDFilePluginFactory )
