#include "he_e2eread.h"

#define _USE_MATH_DEFINES
#include<cmath>
#include<cctype>

#include <datastruct/oct.h>
#include <datastruct/coordslo.h>
#include <datastruct/sloimage.h>
#include <datastruct/bscan.h>
#include <filereadoptions.h>

#include <iostream>
#include <fstream>
#include <iomanip>
#include<filesystem>

#include <opencv2/opencv.hpp>

#include <E2E/e2edata.h>
#include <E2E/dataelements/patientdataelement.h>
#include <E2E/dataelements/image.h>
#include <E2E/dataelements/segmentationdata.h>
#include <E2E/dataelements/bscanmetadataelement.h>
#include <E2E/dataelements/imageregistration.h>
#include <E2E/dataelements/slodataelement.h>
#include <E2E/dataelements/textelement.h>
#include <E2E/dataelements/textelement16.h>
#include <E2E/dataelements/stringlistelement.h>

#include <E2E/dataelements/studydata.h>

#include "he_gray_transform.h"

#include <boost/locale.hpp>
#include <boost/log/trivial.hpp>

#include <oct_cpp_framework/callback.h>

#include"../platform_helper.h"

#include<filereader/filereader.h>


namespace bfs = std::filesystem;
namespace loc = boost::locale;


namespace OctData
{

	namespace
	{
		constexpr const double factorAngle2mm = 4.4/15.; // TODO 15 grad = 4.4mm => 1grad = 0.29333333

		inline double pow2(double v) { return v*v; }

		Patient::Sex convertSex(E2E::PatientDataElement::Sex e2eSex)
		{
			switch(e2eSex)
			{
				case E2E::PatientDataElement::Sex::Female : return Patient::Sex::Female ;
				case E2E::PatientDataElement::Sex::Male   : return Patient::Sex::Male   ;
				case E2E::PatientDataElement::Sex::Unknown: return Patient::Sex::Unknown;
			}
			return Patient::Sex::Unknown;
		}
		
		void copyPatData(Patient& pat, const E2E::Patient& e2ePat)
		{
			if(e2ePat.getPatientUID())
				pat.setPatientUID(e2ePat.getPatientUID()->getText());

			const E2E::PatientDataElement* e2ePatData = e2ePat.getPatientData();
			if(e2ePatData)
			{
				pat.setForename (                            e2ePatData->getForename() );
				pat.setSurname  (                            e2ePatData->getSurname () );
				pat.setId       (                            e2ePatData->getId      () );
				pat.setSex      (                 convertSex(e2ePatData->getSex     ()));
				pat.setTitle    (                            e2ePatData->getTitle   () );
				pat.setBirthdate(Date::fromWindowsTimeFormat(e2ePatData->getWinBDate()));
			}

			const E2E::TextElement16* e2eDiagnose = e2ePat.getDiagnose();
			if(e2eDiagnose)
				pat.setDiagnose(e2eDiagnose->getText());

			E2E::StringListElement* ancestry = e2ePat.getAncestry();
			if(ancestry && ancestry->size() > 0)
				pat.setAncestry(convertUTF16StringToUTF8(ancestry->getString(0)));
		}
		
		void copyStudyData(Study& study, const E2E::Study& e2eStudy)
		{
			if(e2eStudy.getStudyUID())
				study.setStudyUID(e2eStudy.getStudyUID()->getText());
			
			const E2E::StudyData* e2eStudyData = e2eStudy.getStudyData();
			if(e2eStudyData)
			{
				study.setStudyDate(Date::fromWindowsTimeFormat(e2eStudyData->getWindowsStudyDate()));
				study.setStudyOperator(loc::conv::to_utf<char>(e2eStudyData->getOperator(), "ISO-8859-15"));
			}

			E2E::StringListElement* e2eStudyName = e2eStudy.getStudyName();
			if(e2eStudyName && e2eStudyName->size() > 0)
				study.setStudyName(convertUTF16StringToUTF8(e2eStudyName->getString(0)));
		}
		
		void copySeriesData(Series& series, const E2E::Series& e2eSeries)
		{
			if(e2eSeries.getSeriesUID())
				series.setSeriesUID(e2eSeries.getSeriesUID()->getText());

			if(e2eSeries.getExaminedStructure())
			{
				if(e2eSeries.getExaminedStructure()->size() > 0)
				{
					const std::u16string& examinedStructure = e2eSeries.getExaminedStructure()->getString(0);
					if(examinedStructure == u"ONH")
						series.setExaminedStructure(Series::ExaminedStructure::ONH);
					else if(examinedStructure == u"Retina")
						series.setExaminedStructure(Series::ExaminedStructure::Retina);
					else
					{
						series.setExaminedStructureText(convertUTF16StringToUTF8(examinedStructure));
						series.setExaminedStructure(Series::ExaminedStructure::Unknown);
					}
				}
			}

			if(e2eSeries.getScanPattern())
			{
				if(e2eSeries.getScanPattern()->size() > 0)
				{
					const std::u16string& scanPattern = e2eSeries.getScanPattern()->getString(0);
					if(scanPattern == u"OCT ART Volume")
						series.setScanPattern(Series::ScanPattern::Volume);
					else if(scanPattern == u"OCT Radial+Circles")
						series.setScanPattern(Series::ScanPattern::RadialCircles);
					else
					{
						series.setScanPatternText(convertUTF16StringToUTF8(scanPattern));
						series.setScanPattern(Series::ScanPattern::Unknown);
//						series.setScanPatternText(convert.to_bytes(scanPattern));
					}
				}
			}
			// e2eSeries.
		}

		void copySlo(Series& series, const E2E::Series& e2eSeries, const FileReadOptions& op)
		{
			const E2E::Image* e2eSlo = e2eSeries.getSloImage();
			if(!e2eSlo)
				return;

			std::unique_ptr<SloImage> slo = std::make_unique<SloImage>();

			const cv::Mat e2eSloImage = e2eSlo->getImage();

			bool imageIsSet = false;
			if(op.rotateSlo)
			{
				E2E::SloDataElement* e2eSloData = e2eSeries.getSloDataElement();
				if(e2eSloData)
				{
					const float* sloTrans = e2eSloData->getTransformData();

					const double a11 = 1./sloTrans[0];
					const double a12 =   -sloTrans[1];
					const double a21 =   -sloTrans[3];
					const double a22 = 1./sloTrans[4];

					const double b1  = -sloTrans[2] - a12*e2eSloImage.rows/2;
					const double b2  = -sloTrans[5] + a21*e2eSloImage.cols/2;

					CoordTransform sloTransform(a11, a12, a21, a22, b1, b2);

					cv::Mat trans_mat = (cv::Mat_<double>(2,3) << a11, a12, b1, a21, a22, b2);
					// cv::Mat trans_mat = (cv::Mat_<double>(2,3) << 1, 0, shiftY, degree, 1, shiftX - degree*bscanImageConv.cols/2.);

					uint8_t fillValue = 0;
					if(op.fillEmptyPixelWhite)
						fillValue = 255;

					cv::Mat affineResult;
					cv::warpAffine(e2eSloImage, affineResult, trans_mat, e2eSloImage.size(), cv::INTER_LINEAR, cv::BORDER_CONSTANT, cv::Scalar(fillValue));
					slo->setImage(affineResult);
					imageIsSet = true;
					slo->setTransform(sloTransform);
				}
			}

			if(!imageIsSet)
				slo->setImage(e2eSloImage);

			double sizeX = static_cast<double>(e2eSlo->getImageCols());
			double sizeY = static_cast<double>(e2eSlo->getImageRows());

			double sloScanAngle = 30; // TODO
			double factor = factorAngle2mm*sloScanAngle;
			slo->setScaleFactor(ScaleFactor(factor/sizeX, factor/sizeY));
			slo->setShift(CoordSLOpx(sizeX/2.,
			                         sizeY/2.));
			series.takeSloImage(std::move(slo));
		}

		void addSegData(BScan::Data& bscanData, Segmentationlines::SegmentlineType segType, const E2E::BScan::SegmentationMap& e2eSegMap, int index, int type, const E2E::ImageRegistration* reg, std::size_t imagecols)
		{
			const E2E::BScan::SegmentationMap::const_iterator segPair = e2eSegMap.find(E2E::BScan::SegPair(index, type));
			if(segPair != e2eSegMap.end())
			{
				E2E::SegmentationData* segData = segPair->second;
				if(segData)
				{
					std::size_t numSegData = segData->size();
					Segmentationlines::Segmentline segVec(numSegData);
					if(reg)
					{
						double shiftY    = -reg->values[3];
						double degree    = -reg->values[7];
						double shiftX    = -reg->values[9] - degree*static_cast<double>(imagecols)/2.;
						int    shiftXVec = static_cast<int>(std::round(shiftY));
						double pos       = shiftXVec;

						Segmentationlines::Segmentline::iterator segVecBegin = segVec.begin();
						E2E::SegmentationData::pointer segDataBegin = segData->begin();

						std::size_t absShiftX = static_cast<std::size_t>(abs(shiftXVec));
						if(numSegData < absShiftX)
							return;

						std::size_t numAssign = numSegData-absShiftX;

						if(shiftXVec < 0)
							segDataBegin -= shiftXVec;
						if(shiftXVec > 0)
							segVecBegin += shiftXVec;

						std::transform(segDataBegin, segDataBegin+numAssign, segVecBegin, [&pos, shiftX, degree](double value) { return value + shiftX + (++pos)*degree; });
					}
					else
						segVec.assign(segData->begin(), segData->end());

					std::transform(segVec.begin(), segVec.end(), segVec.begin()
					            , [](Segmentationlines::SegmentlineDataType value) { if(value > 1e20) return std::numeric_limits<double>::quiet_NaN(); return value; });


					bscanData.getSegmentLine(segType) = std::move(segVec);
				}
			}
		}

		template<typename T>
		void fillEmptyBroderCols(cv::Mat& image, T broderValue, T fillValue)
		{
			const int cols = image.cols;
			const int rows = image.rows;

			// find left Broder
			int leftEnd = cols;
			for(int row = 0; row<rows; ++row)
			{
				T* it = image.ptr<T>(row);
				for(int col = 0; col < leftEnd; ++col)
				{
					if(*it != broderValue)
					{
						leftEnd = col;
						break;
					}
					++it;
				}
				if(leftEnd == 0)
					break;
			}

			// fill left Broder
			if(leftEnd > 0)
			{
				for(int row = 0; row<rows; ++row)
				{
					T* it = image.ptr<T>(row);
					for(int col = 0; col < leftEnd; ++col)
					{
						*it = fillValue;
						++it;
					}
				}
			}
			else
				if(leftEnd == cols) // empty Image
					return;

			// find right Broder
			int rightEnd = leftEnd;
			for(int row = 0; row<rows; ++row)
			{
				T* it = image.ptr<T>(row, cols-1);
				for(int col = cols-1; col >= rightEnd; --col)
				{
					if(*it != broderValue)
					{
						rightEnd = col;
						break;
					}
					--it;
				}
				if(rightEnd == cols-1)
					break;
			}

			// fill right Broder
			if(rightEnd < cols)
			{
				for(int row = 0; row<rows; ++row)
				{
					T* it = image.ptr<T>(row, rightEnd);
					for(int col = rightEnd; col < cols; ++col)
					{
						*it = fillValue;
						++it;
					}
				}
			}
		}
		
		template<typename SourceType, typename DestType, typename TransformType>
		void useLUTBScan(const cv::Mat& source, cv::Mat& dest)
		{
			dest.create(source.rows, source.cols, cv::DataType<DestType>::type);

			TransformType& lut = TransformType::getInstance();
			
			const SourceType* sPtr = source.ptr<SourceType>();
			DestType* dPtr = dest.ptr<DestType>();
			
			const std::size_t size = source.cols * source.rows;
			for(size_t i = 0; i < size; ++i)
			{
				*dPtr = lut.getValue(*sPtr);
				++dPtr;
				++sPtr;
			}
		}

		void transformImage(const E2E::ImageRegistration* reg, cv::Mat& image, bool fillWhite, int interpolMethod = cv::INTER_LINEAR)
		{
			if(!reg)
				return;

			// std::cout << "shift X: " << reg->values[9] << std::endl;
			double shiftY = -reg->values[3];
			double degree = -reg->values[7];
			double shiftX = -reg->values[9];
			// std::cout << "shift X: " << shiftX << "\tdegree: " << degree << "\t" << (degree*bscanImageConv.cols/2) << std::endl;
			cv::Mat trans_mat = (cv::Mat_<double>(2,3) << 1, 0, shiftY, degree, 1, shiftX - degree*image.cols/2.);

			uint8_t fillValue = 0;
			if(fillWhite)
				fillValue = 255;
			cv::warpAffine(image, image, trans_mat, image.size(), interpolMethod, cv::BORDER_CONSTANT, cv::Scalar(fillValue));
		}

		void copyBScan(Series& series, const E2E::BScan& e2eBScan, const FileReadOptions& op)
		{
			const E2E::Image* e2eAngioImg = e2eBScan.getAngioImage();
			const E2E::Image* e2eBScanImg = e2eBScan.getImage();
			if(!e2eBScanImg)
				return;

			const cv::Mat& e2eImage = e2eBScanImg->getImage();


			BScan::Data bscanData;

			const E2E::BScanMetaDataElement* e2eMeta = e2eBScan.getBScanMetaDataElement();
			if(e2eMeta)
			{
				double scanLengthAngle;
				bscanData.start = CoordSLOmm(e2eMeta->getX1()*factorAngle2mm, e2eMeta->getY1()*factorAngle2mm);
				bscanData.end   = CoordSLOmm(e2eMeta->getX2()*factorAngle2mm, e2eMeta->getY2()*factorAngle2mm);

				bscanData.numAverage      = e2eMeta->getNumAve();
				bscanData.imageQuality    = e2eMeta->getImageQuality();
				bscanData.acquisitionTime = Date::fromWindowsTicks(e2eMeta->getAcquisitionTime());

				if(e2eMeta->getScanType() == E2E::BScanMetaDataElement::ScanType::Circle)
				{
					double scanAngle            = sqrt(pow2(e2eMeta->getX1()-e2eMeta->getCenterX()) + pow2(e2eMeta->getY1()-e2eMeta->getCenterY()))*2.;
					scanLengthAngle             = scanAngle*M_PI;
					bscanData.scanAngle         = scanAngle;
					bscanData.clockwiseRotation = series.getLaterality() == OctData::Series::Laterality::OD; // TODO: inoperable because not read Laterality from e2e
					bscanData.center            = CoordSLOmm(e2eMeta->getCenterX()*factorAngle2mm, e2eMeta->getCenterY()*factorAngle2mm);
					bscanData.bscanType         = BScan::BScanType::Circle;
				}
				else
				{
					scanLengthAngle             = sqrt(pow2(e2eMeta->getX1()-e2eMeta->getX2()) + pow2(e2eMeta->getY1()-e2eMeta->getY2()));
					bscanData.scanAngle         = scanLengthAngle;
					bscanData.bscanType         = BScan::BScanType::Line;
				}
				bscanData.scaleFactor = ScaleFactor(scanLengthAngle*factorAngle2mm/static_cast<double>(e2eImage.cols), 0, e2eMeta->getScaleY());
			}


			const E2E::ImageRegistration* reg = nullptr;
			if(op.registerBScanns)
				reg = e2eBScan.getImageRegistrationData();

			// segmenation lines
			const E2E::BScan::SegmentationMap& e2eSegMap = e2eBScan.getSegmentationMap();
			const std::size_t imgCols = static_cast<std::size_t>(e2eImage.cols);
			addSegData(bscanData, Segmentationlines::SegmentlineType::ILM , e2eSegMap,  0, 5, reg, imgCols);
			addSegData(bscanData, Segmentationlines::SegmentlineType::BM  , e2eSegMap,  1, 2, reg, imgCols);
			addSegData(bscanData, Segmentationlines::SegmentlineType::RNFL, e2eSegMap,  2, 7, reg, imgCols);
			addSegData(bscanData, Segmentationlines::SegmentlineType::GCL , e2eSegMap,  3, 1, reg, imgCols);
			addSegData(bscanData, Segmentationlines::SegmentlineType::IPL , e2eSegMap,  4, 1, reg, imgCols);
			addSegData(bscanData, Segmentationlines::SegmentlineType::INL , e2eSegMap,  5, 1, reg, imgCols);
			addSegData(bscanData, Segmentationlines::SegmentlineType::OPL , e2eSegMap,  6, 1, reg, imgCols);
			//                                                                          7
			addSegData(bscanData, Segmentationlines::SegmentlineType::ELM , e2eSegMap,  8, 3, reg, imgCols);
			//                                                                          9
			//                                                                         10
			//                                                                         11
			//                                                                         12
			//                                                                         13
			addSegData(bscanData, Segmentationlines::SegmentlineType::PR1 , e2eSegMap, 14, 1, reg, imgCols);
			addSegData(bscanData, Segmentationlines::SegmentlineType::PR2 , e2eSegMap, 15, 1, reg, imgCols);
			addSegData(bscanData, Segmentationlines::SegmentlineType::RPE , e2eSegMap, 16, 1, reg, imgCols);

			cv::Mat bscanImageConv;
			if(e2eImage.type() == cv::DataType<float>::type)
			{
				cv::Mat bscanImagePow;
				cv::pow(e2eImage, 0.25, bscanImagePow);
				bscanImagePow.convertTo(bscanImageConv, CV_8U, 255, 0);
			}
			else
			{
				cv::Mat dest;
				// convert image
				switch(op.e2eGray)
				{
				case FileReadOptions::E2eGrayTransform::nativ:
					e2eImage.convertTo(dest, CV_32FC1, 1/static_cast<double>(1 << 16), 0);
					cv::pow(dest, 8, dest);
					dest.convertTo(bscanImageConv, CV_8U, 255, 0);
					break;
				case FileReadOptions::E2eGrayTransform::xml:
					useLUTBScan<uint16_t, uint8_t, HeGrayTransformXml>(e2eImage, bscanImageConv);
					break;
				case FileReadOptions::E2eGrayTransform::vol:
					useLUTBScan<uint16_t, uint8_t, HeGrayTransformVol>(e2eImage, bscanImageConv);
					break;
				case FileReadOptions::E2eGrayTransform::u16:
					useLUTBScan<uint16_t, uint8_t, HeGrayTransformUFloat16>(e2eImage, bscanImageConv);
					break;
				}
				if(bscanImageConv.empty())
				{
					BOOST_LOG_TRIVIAL(error) << "E2E::copyBScan: Error: Converted Matrix empty, valid E2eGrayTransform option?";
					useLUTBScan<uint16_t, uint8_t, HeGrayTransformXml>(e2eImage, bscanImageConv);
				}
			}

			if(!op.fillEmptyPixelWhite)
				fillEmptyBroderCols<uint8_t>(bscanImageConv, 255, 0);

			transformImage(reg, bscanImageConv, op.fillEmptyPixelWhite);

			std::shared_ptr<BScan> bscan = std::make_shared<BScan>(bscanImageConv, bscanData);
			if(op.holdRawData)
				bscan->setRawImage(e2eImage);
			if(e2eAngioImg)
			{
				cv::Mat angioImg = e2eAngioImg->getImage();
// 				std::transform(angioImg.begin<uint8_t>(), angioImg.end<uint8_t>(), angioImg.begin<uint8_t>(), [](uint8_t v){ return v==255?0:v; });
				if(reg)
					transformImage(reg, angioImg, false, cv::INTER_NEAREST);
				bscan->setAngioImage(angioImg);
			}
			series.addBScan(std::move(bscan));
		}
		
		std::string toLower(std::string data)
		{
			std::transform(data.begin(), data.end(), data.begin(), [](char c){ return std::tolower(c); });
			return data;
		}
	}


	HeE2ERead::HeE2ERead()
	: OctFileReader({OctExtension(".E2E", "Heidelberg Engineering E2E File"), OctExtension(".sdb", "Heidelberg Engineering HEYEX File")})
	{
	}

	bool HeE2ERead::readFile(FileReader& filereader, OCT& oct, const FileReadOptions& op, CppFW::Callback* callback)
	{
		const std::filesystem::path& file = filereader.getFilepath();
		
		std::string fileExtLower = toLower(file.extension().generic_string());

		if(fileExtLower != ".e2e" && fileExtLower != ".sdb")
			return false;

		if(!bfs::exists(file))
			return false;

		BOOST_LOG_TRIVIAL(trace) << "Try to open OCT file as HEYEX file";

		CppFW::Callback loadCallback   ;
		CppFW::Callback convertCallback;

		if(callback)
		{
			loadCallback    = callback->createSubTask(0.5, 0.0);
			convertCallback = callback->createSubTask(0.5, 0.5);
		}

		E2E::E2EData e2eData;
		e2eData.options.readBScanImages = op.readBScans;
		e2eData.readE2EFile(file.generic_string(), &loadCallback);

		const E2E::DataRoot& e2eRoot = e2eData.getDataRoot();

		std::size_t basisFileId = e2eRoot.getCreateFromLoadedFileNum();


		// load extra Data from patient file (pdb) and study file (edb)
		if(file.extension() == ".sdb")
		{
			BOOST_LOG_TRIVIAL(debug) << "Try to load extra files";
			for(const E2E::DataRoot::SubstructurePair& e2ePatPair : e2eRoot)
			{
				const std::size_t bufferSize = 100;
				char buffer[bufferSize];
				const E2E::Patient& e2ePat = *(e2ePatPair.second);
				std::snprintf(buffer, bufferSize, "%08d.pdb", e2ePatPair.first);

				BOOST_LOG_TRIVIAL(debug) << "try to open patient informations file: " << buffer;
				// std::string filenname =
				bfs::path patientDataFile(file.parent_path() / buffer);
				if(bfs::exists(patientDataFile))
					e2eData.readE2EFile(patientDataFile.generic_string());

				for(const E2E::Patient::SubstructurePair& e2eStudyPair : e2ePat)
				{
					std::snprintf(buffer, bufferSize, "%08d.edb", e2eStudyPair.first);
					BOOST_LOG_TRIVIAL(debug) << "try to open series informations file: " << buffer;
					bfs::path studyDataFile(file.parent_path() / buffer);
					if(bfs::exists(studyDataFile))
						e2eData.readE2EFile(studyDataFile.generic_string());
				}
			}
		}


		BOOST_LOG_TRIVIAL(debug) << "convert HEYEX data to own data structure";
		CppFW::CallbackSubTaskCreator callbackCreatorPatients(&convertCallback, e2eRoot.size());
		// convert e2e structure in octdata structure
		for(const E2E::DataRoot::SubstructurePair& e2ePatPair : e2eRoot)
		{
			const E2E::Patient& e2ePat = *(e2ePatPair.second);

			CppFW::Callback callbackPatient = callbackCreatorPatients.getSubTaskCallback();
			CppFW::CallbackSubTaskCreator callbackCreatorStudys(&callbackPatient, e2ePat.size());

			if(e2ePat.getCreateFromLoadedFileNum() != basisFileId)
				continue;

			Patient& pat = oct.getPatient(e2ePatPair.first);
			copyPatData(pat, e2ePat);
			
			for(const E2E::Patient::SubstructurePair& e2eStudyPair : e2ePat)
			{
				const E2E::Study& e2eStudy = *(e2eStudyPair.second);

				CppFW::Callback callbackStudy = callbackCreatorStudys.getSubTaskCallback();
				CppFW::CallbackSubTaskCreator callbackCreatorSeries(&callbackPatient, e2eStudy.size());

				if(e2eStudy.getCreateFromLoadedFileNum() != basisFileId)
					continue;

// 				std::cout << "studyID: " << studyID << std::endl;
				Study& study = pat.getStudy(e2eStudyPair.first);

				copyStudyData(study, e2eStudy);

				
				for(const E2E::Study::SubstructurePair& e2eSeriesPair : e2eStudy)
				{
					CppFW::Callback callbackSeries = callbackCreatorSeries.getSubTaskCallback();

					const E2E::Series& e2eSeries = *(e2eSeriesPair.second);
					if(e2eSeries.getCreateFromLoadedFileNum() != basisFileId)
						continue;

// 					std::cout << "seriesID: " << seriesID << std::endl;
					Series& series = study.getSeries(e2eSeriesPair.first);
					copySlo(series, e2eSeries, op);
					
					copySeriesData(series, e2eSeries);

					CppFW::CallbackStepper bscanCallbackStepper(&callbackSeries, e2eSeries.size());
					for(const E2E::Series::SubstructurePair& e2eBScanPair : e2eSeries)
					{
						copyBScan(series, *(e2eBScanPair.second), op);
						++bscanCallbackStepper;
					}
				}
			}
		}

		BOOST_LOG_TRIVIAL(debug) << "read HEYEX file \"" << file.generic_string() << "\" finished";

		return true;
	}


}
