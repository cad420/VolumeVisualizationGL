#pragma once
#include <VMUtils/json_binding.hpp>
#include <vector>
#include <string>


struct LVDJSONStruct : vm::json::Serializable<LVDJSONStruct>
{
	VM_JSON_FIELD( std::vector<std::string>, fileNames );
	VM_JSON_FIELD( float, samplingRate );
	VM_JSON_FIELD( std::vector<float>, spacing );
};
