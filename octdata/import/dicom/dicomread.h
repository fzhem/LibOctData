#pragma once

#include <string>
#include<filesystem>

#include "../octfilereader.h"


class DcmElement;
class DcmPixelSequence;
class DcmSequenceOfItems;

namespace OctData
{
	class Series;
	class FileReadOptions;

	class DicomRead : public OctFileReader
	{
		bool readDicomDir(const std::filesystem::path& file, OCT& oct);

		void decodeImage(Series& series, const FileReadOptions& op, const char* pixData, std::size_t length);

		void readPixelData(DcmElement* element, Series& series, const FileReadOptions& op, CppFW::Callback* callback);
		void readPixelItem(DcmPixelSequence* dseq, Series& series, const FileReadOptions& op, unsigned long i);
		void readDict(DcmSequenceOfItems* element, Series& series, const FileReadOptions& op, CppFW::Callback* callback);

		double spacingBetweenSlices = 0;
		double pixelSpaceingX = 0;
		double pixelSpaceingZ = 0;

		const int32_t* registerArray = nullptr;
		unsigned long numRegisterElements = 0;

		std::size_t bscans = 0;


	public:
	    DicomRead();

	    bool readFile(FileReader& filereader, OCT& oct, const FileReadOptions& op, CppFW::Callback* callback) override;
	};

}

