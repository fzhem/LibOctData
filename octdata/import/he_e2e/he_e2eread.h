#pragma once

#include <string>
#include<filesystem>

#include "../octfilereader.h"

namespace OctData
{
	class FileReadOptions;
	
	class HeE2ERead : public OctFileReader
	{
	public:
		HeE2ERead();

		virtual bool readFile(FileReader& filereader, OCT& oct, const FileReadOptions& op, CppFW::Callback* callback) override;

	};
}
