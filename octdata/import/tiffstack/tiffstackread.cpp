#include "tiffstackread.h"
#include <datastruct/oct.h>
#include <datastruct/coordslo.h>
#include <datastruct/sloimage.h>
#include <datastruct/bscan.h>

#include<filesystem>

#include <opencv2/opencv.hpp>

// #include <cpp_framework/callback.h>

#include <boost/log/trivial.hpp>

#include <filereadoptions.h>


#include <tiffio.h>
#include <tiffio.hxx>

#include<filereader/filereader.h>

namespace bfs = std::filesystem;

namespace OctData
{

	TiffStackRead::TiffStackRead()
	: OctFileReader(OctExtension{".tiff", ".tif", "Tiff stack"})
	{
	}

	bool TiffStackRead::readFile(FileReader& filereader, OCT& oct, const FileReadOptions& /*op*/, CppFW::Callback* /*callback*/)
	{
		const std::filesystem::path& file = filereader.getFilepath();

		if(file.extension() != ".tiff" && file.extension() != ".tif")
			return false;
		
		if(!bfs::exists(file))
			return false;

		BOOST_LOG_TRIVIAL(trace) << "Try to open OCT file as tiff stack";

		int dircount = 0;

		TIFF* tif = TIFFOpen(file.generic_string().c_str(), "r");
		if(tif)
		{
			Patient& pat    = oct  .getPatient(1);
			Study  & study  = pat  .getStudy(1);
			Series & series = study.getSeries(1);

			// http://www.libtiff.org/man/TIFFGetField.3t.html
			
			uint32_t imageWidth, imageLength;
// 			uint32 tileWidth, tileLength;

			do {
				TIFFGetField(tif, TIFFTAG_IMAGEWIDTH , &imageWidth );
				TIFFGetField(tif, TIFFTAG_IMAGELENGTH, &imageLength);

				BScan::Data bscanData;

				cv::Mat bscanImage(imageLength, imageWidth, CV_MAKETYPE(cv::DataType<uint8_t>::type, 4));
				if(TIFFReadRGBAImageOriented(tif, imageWidth, imageLength, bscanImage.ptr<uint32_t>(0), ORIENTATION_TOPLEFT, 0) != 0)
					cv::cvtColor(bscanImage, bscanImage, cv::COLOR_BGR2GRAY);
				else
					bscanImage = cv::Mat();

				series.addBScan(std::make_shared<BScan>(bscanImage, bscanData));

				++dircount;
			} while(TIFFReadDirectory(tif));

			TIFFClose(tif);
		}

		BOOST_LOG_TRIVIAL(debug) << "read tiff stack \"" << file.generic_string() << "\" finished";
		return dircount>0;
	}

}
