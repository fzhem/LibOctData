#include "volread.h"
#include "voldatastruct.h"

#include <datastruct/oct.h>
#include <datastruct/coordslo.h>
#include <datastruct/sloimage.h>
#include <datastruct/bscan.h>

#include <ostream>
#include <thread>
#include <chrono>
#include <ctime>
#include<filesystem>
#include<optional>

#include <opencv2/opencv.hpp>

#include <filereadoptions.h>

#include <boost/log/trivial.hpp>
#include <boost/lexical_cast.hpp>

#include <emmintrin.h>
#include <oct_cpp_framework/callback.h>

#include<filereader/filereader.h>

namespace bfs = std::filesystem;


namespace
{
	template<typename T>
	void readFStream(std::istream& stream, T* dest, std::size_t num = 1)
	{
		stream.read(reinterpret_cast<char*>(dest), sizeof(T)*num);
	}

	template<typename T>
	void readCVImage(std::istream& stream, cv::Mat& image, std::size_t sizeX, std::size_t sizeY)
	{
		image = cv::Mat(static_cast<int>(sizeX), static_cast<int>(sizeY), cv::DataType<T>::type);

		std::size_t num = sizeX*sizeY;

		stream.read(reinterpret_cast<char*>(image.data), num*sizeof(T));
	}


	void copyMetaData(const OctData::VolHeader& header, OctData::Patient& pat, OctData::Study& study, OctData::Series& series)
	{
		// Patient data
		pat.setId(header.data.patientID);
		pat.setBirthdate(OctData::Date::fromWindowsTimeFormat(header.data.dob));

		// Study data
		study.setStudyDate(OctData::Date::fromWindowsTicks(header.data.examTime));
		
		// Series data
		series.setScanDate(OctData::Date::fromWindowsTimeFormat(header.data.visitDate));
		if(strcmp("OD", header.data.scanPosition) == 0)
			series.setLaterality(OctData::Series::Laterality::OD);
		else if(strcmp("OS", header.data.scanPosition) == 0)
			series.setLaterality(OctData::Series::Laterality::OS);
		else
			BOOST_LOG_TRIVIAL(warning) << "Unknown scan position: " << std::string(header.data.scanPosition, header.data.scanPosition+4);
		
		series.setSeriesUID   (header.data.id);
		series.setRefSeriesUID(header.data.referenceID);
		
		// series.setScanDate(OctData::Date::fromWindowsTicks(data.examTime));

		switch(header.data.scanPattern)
		{
			case 1: series.setScanPattern(OctData::Series::ScanPattern::SingleLine   ); break;
			case 2: series.setScanPattern(OctData::Series::ScanPattern::Circular     ); break;
			case 3: series.setScanPattern(OctData::Series::ScanPattern::Volume       ); break;
			case 4: series.setScanPattern(OctData::Series::ScanPattern::FastVolume   ); break;
			case 5: series.setScanPattern(OctData::Series::ScanPattern::Radial       ); break;
			case 6: series.setScanPattern(OctData::Series::ScanPattern::RadialCircles); break;
			default:
				series.setScanPattern(OctData::Series::ScanPattern::Unknown);
				series.setScanPatternText(boost::lexical_cast<std::string>(header.data.scanPattern));
				BOOST_LOG_TRIVIAL(warning) << "Unknown scan pattern: " << header.data.scanPattern;
				break;
		}

		series.setScanFocus(header.data.scanFocus);
	}


	void simdQuadRoot(const cv::Mat& in, cv::Mat& out)
	{
		if(in.type() == cv::DataType<float>::type)
		{
			out.create(in.size(), in.type());

			std::size_t size = in.rows*in.cols;
			const float* dataPtr = in .ptr<float>(0);
			      float* outPtr  = out.ptr<float>(0);

			// SIMD
			std::size_t nb_iters = size / 4;
			const __m128* ptr = reinterpret_cast<const __m128*>(dataPtr);
			for(std::size_t i = 0; i < nb_iters; ++i)
			{
				_mm_store_ps(outPtr, _mm_sqrt_ps(_mm_sqrt_ps(*ptr)));
				++ptr;
				outPtr  += 4;
			}

			// handel rest
			outPtr = out.ptr<float>(0);

			for(std::size_t pos = nb_iters*4; pos<size; ++pos)
			{
				outPtr[pos] = std::sqrt(std::sqrt(dataPtr[pos]));
			}
		}
		else
		{
			cv::pow(in, 0.25, out);
		}
	}
}



namespace OctData
{
	VOLRead::VOLRead()
	: OctFileReader(OctExtension{".vol", ".vol.gz", "Heidelberg Engineering Raw File"})
	{
	}

	bool VOLRead::readFile(FileReader& filereader, OCT& oct, const FileReadOptions& op, CppFW::Callback* callback)
	{
//
//     BOOST_LOG_TRIVIAL(trace) << "A trace severity message";
//     BOOST_LOG_TRIVIAL(debug) << "A debug severity message";
//     BOOST_LOG_TRIVIAL(info) << "An informational severity message";
//     BOOST_LOG_TRIVIAL(warning) << "A warning severity message";
//     BOOST_LOG_TRIVIAL(error) << "An error severity message";
//     BOOST_LOG_TRIVIAL(fatal) << "A fatal severity message";

		if(filereader.getExtension() != ".vol")
			return false;

		const std::string filename = filereader.getFilepath().generic_string();
		BOOST_LOG_TRIVIAL(trace) << "Try to open OCT file as vol";

		if(!filereader.openFile())
		{
			BOOST_LOG_TRIVIAL(error) << "Can't open vol file " << filename;
			return false;
		}
/*
		std::fstream stream(filepathConv(file), std::ios::binary | std::ios::in);
		if(!stream.good())
		{
			BOOST_LOG_TRIVIAL(error) << "Can't open vol file " << filepathConv(file);
			return false;
		}
		*/

		BOOST_LOG_TRIVIAL(debug) << "open " << filename << " as vol file";


		const std::size_t formatstringlength = 8;
		char fileformatstring[formatstringlength];
		filereader.readFStream(fileformatstring, formatstringlength);
		if(memcmp(fileformatstring, "HSF-OCT-", formatstringlength) != 0) // 0 = strings are equal
		{
			BOOST_LOG_TRIVIAL(error) << filename << " Wrong fileformat (not HSF-OCT)";
			return false;
		}

		VolHeader volHeader;

		filereader.readFStream(&(volHeader.data));
// 		volHeader.printData(std::cout);
		BOOST_LOG_TRIVIAL(info) << "HSF file version: " << volHeader.data.version;
		filereader.seekg(VolHeader::getHeaderSize());

		Patient& pat    = oct.getPatient(volHeader.data.pid);
		Study&   study  = pat.getStudy(volHeader.data.vid);
		Series&  series = study.getSeries(1); // TODO


		copyMetaData(volHeader, pat, study, series);


		// Read SLO
		cv::Mat sloImage;
		filereader.readCVImage<uint8_t>(sloImage, volHeader.data.sizeXSlo, volHeader.data.sizeYSlo);

		{
			std::unique_ptr<SloImage> slo = std::make_unique<SloImage>();
			slo->setImage(sloImage);
			slo->setScaleFactor(ScaleFactor(volHeader.data.scaleXSlo, volHeader.data.scaleYSlo));
			series.takeSloImage(std::move(slo));
		}


		const std::size_t numBScans = op.readBScans?volHeader.data.numBScans:1;
		// Read BScann
		for(std::size_t numBscan = 0; numBscan<numBScans; ++numBscan)
		{
// 			std::this_thread::sleep_for(std::chrono::milliseconds(50));
			if(callback)
			{
				if(!callback->callback(static_cast<double>(numBscan)/static_cast<double>(numBScans)))
				{
					BOOST_LOG_TRIVIAL(info) << "loading canceled by user";
					return false;
				}
			}

			BScanHeader bscanHeader;
			BScan::Data bscanData;

			std::size_t bscanPos = VolHeader::getHeaderSize() + volHeader.getSLOPixelSize() + numBscan*volHeader.getBScanSize();

// 			std::cout << "bscanPos: " << bscanPos << std::endl;

			filereader.seekg(bscanPos);
			filereader.readFStream(&(bscanHeader.data));

			if(memcmp(bscanHeader.data.hsfOctRawStr, "HSF-BS-", BScanHeader::identiferSize) != 0) // 0 = strings are equal
			{
				std::string hsfStr(bscanHeader.data.hsfOctRawStr, bscanHeader.data.hsfOctRawStr+7);
				BOOST_LOG_TRIVIAL(error) << filename << ": Error in B-scan " << numBscan << " ; Wrong bscan header (not HSF-BS-) -> " << hsfStr;
				return false;
			}
// 			BOOST_LOG_TRIVIAL(info) << "HSF-BScan version: " << bscanHeader.data.version;

			// bscanHeader.printData();


			typedef std::optional<Segmentationlines::SegmentlineType> SegLineOpt;
			const SegLineOpt seglines[] =
			{
				Segmentationlines::SegmentlineType::ILM ,   // 0
				Segmentationlines::SegmentlineType::BM  ,   // 1
				Segmentationlines::SegmentlineType::RNFL,   // 2
				Segmentationlines::SegmentlineType::GCL ,   // 3
				Segmentationlines::SegmentlineType::IPL ,   // 4
				Segmentationlines::SegmentlineType::INL ,   // 5
				Segmentationlines::SegmentlineType::OPL ,   // 6
				SegLineOpt()                            ,   // 7
				Segmentationlines::SegmentlineType::ELM ,   // 8
				SegLineOpt()                            ,   // 9
				SegLineOpt()                            ,   // 10
				SegLineOpt()                            ,   // 11
				SegLineOpt()                            ,   // 12
				SegLineOpt()                            ,   // 13
				Segmentationlines::SegmentlineType::PR1 ,   // 14
				Segmentationlines::SegmentlineType::PR2 ,   // 15
				Segmentationlines::SegmentlineType::RPE     // 16
			};

			filereader.seekg(256+bscanPos);
			const int maxSeg = std::min(static_cast<int>(sizeof(seglines)/sizeof(seglines[0])), bscanHeader.data.numSeg);
			for(int segNum = 0; segNum < maxSeg; ++segNum)
			{
				Segmentationlines::Segmentline segVec;
				segVec.reserve(volHeader.data.sizeX);
				float value;
				for(std::size_t xpos = 0; xpos<volHeader.data.sizeX; ++xpos)
				{
					filereader.readFStream(&value);
					segVec.push_back(value);
				}

				std::transform(segVec.begin(), segVec.end(), segVec.begin()
				            , [](Segmentationlines::SegmentlineDataType value) { if(value > 1e20) return std::numeric_limits<double>::quiet_NaN(); return value; });


				if(seglines[segNum])
					bscanData.getSegmentLine(*(seglines[segNum])) = std::move(segVec);
			}

			filereader.seekg(volHeader.data.bScanHdrSize+bscanPos);
			cv::Mat bscanImage;
			cv::Mat bscanImagePow;
			cv::Mat bscanImageConv;
			filereader.readCVImage<float>(bscanImage, volHeader.data.sizeZ, volHeader.data.sizeX);

			if(op.fillEmptyPixelWhite)
				cv::threshold(bscanImage, bscanImage, 1.0, 1.0, cv::THRESH_TRUNC); // schneide hohe werte ab, sonst: bei der konvertierung werden sie auf 0 gesetzt
			// cv::pow(bscanImage, 0.25, bscanImagePow);
			simdQuadRoot(bscanImage, bscanImagePow);
			bscanImagePow.convertTo(bscanImageConv, CV_8U, 255, 0);

			bscanData.start       = CoordSLOmm(bscanHeader.data.startX, bscanHeader.data.startY);

			if(series.getScanPattern() == OctData::Series::ScanPattern::Circular
			|| (series.getScanPattern() == OctData::Series::ScanPattern::RadialCircles && numBscan >= numBScans-3)) // specific to the ScanPattern
			{
				bscanData.bscanType         = BScan::BScanType::Circle;
				bscanData.center            = CoordSLOmm(bscanHeader.data.endX  , bscanHeader.data.endY  );
				bscanData.clockwiseRotation = series.getLaterality() == OctData::Series::Laterality::OD;
			}
			else
			{
				bscanData.bscanType         = BScan::BScanType::Line;
				bscanData.end               = CoordSLOmm(bscanHeader.data.endX  , bscanHeader.data.endY  );
			}


			bscanData.scaleFactor = ScaleFactor(volHeader.data.scaleX, volHeader.data.distance, volHeader.data.scaleZ);
			bscanData.imageQuality = bscanHeader.data.quality;


			if(!filereader.good())
				break;

			std::shared_ptr<BScan> bscan = std::make_shared<BScan>(bscanImageConv, bscanData);
			if(op.holdRawData)
				bscan->setRawImage(bscanImage);
			series.addBScan(std::move(bscan));
		}

		if(volHeader.data.gridType > 0 && volHeader.data.gridOffset > 2000)
		{
			ThicknessGrid grid;
			filereader.seekg(volHeader.data.gridOffset);
			filereader.readFStream(&(grid.data));

			AnalyseGrid& analyseGrid = series.getAnalyseGrid();
			analyseGrid.addDiameterMM(grid.data.diameterA);
			analyseGrid.addDiameterMM(grid.data.diameterB);
			analyseGrid.addDiameterMM(grid.data.diameterC);

			analyseGrid.setCenter(CoordSLOmm(grid.data.centerPosXmm, grid.data.centerPosYmm));
		}


		BOOST_LOG_TRIVIAL(debug) << "read vol file \"" << filename << "\" finished";
		return true;
	}

}
