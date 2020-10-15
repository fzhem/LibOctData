#pragma once
#include <string>

#include "../octfilereader.h"

#include <filesystem>

namespace OctData
{
	class FileReadOptions;
	
	class HeXmlRead : public OctFileReader
	{
		std::filesystem::path xmlFilename;
		std::filesystem::path xmlPath;

	public:
		HeXmlRead();

		virtual bool readFile(FileReader& filereader, OCT& oct, const FileReadOptions& op, CppFW::Callback* callback) override;
	};
}
