#include "dicomread.h"

#ifdef USE_DCMTK

#include "../topcon/readjpeg2k.h"

#include <iostream>
#include <fstream>
#include <iomanip>
#include <sstream>
#include <memory>
#include<filesystem>

#include <algorithm>
#include <string>

#include <opencv2/opencv.hpp>

#include <boost/lexical_cast.hpp>

#include <dcmtk/config/osconfig.h>
#include <dcmtk/dcmimgle/dcmimage.h>

#include "dcmtk/dcmdata/dctk.h" 
#include "dcmtk/dcmdata/dcpxitem.h"


#include <datastruct/oct.h>
#include <datastruct/coordslo.h>
// #include <datastruct/sloimage.h>
#include <datastruct/bscan.h>


#include<filereader/filereader.h>

#include<omp.h>
#include <oct_cpp_framework/callback.h>

namespace bfs = std::filesystem;

#include <filereadoptions.h>

#include <boost/log/trivial.hpp>
#include<boost/lexical_cast.hpp>


#include<string.h>

namespace OctData
{
	DicomRead::DicomRead()
	: OctFileReader({OctExtension{".dicom", ".dcm", "Dicom File"}, OctExtension("DICOMDIR", "DICOM DIR")})
	{
	}

	namespace
	{
		std::string getStdString(DcmItem& item, const DcmTagKey& tagKey, const long unsigned int pos = 0)
		{
			OFString string;
			item.findAndGetOFString(tagKey, string, pos);
			return std::string(string.begin(), string.end());
		}

		Date convertStr2Date(const std::string& str)
		{
			Date d;
			d.setYear (boost::lexical_cast<int>(str.substr(0,4)));
			d.setMonth(boost::lexical_cast<int>(str.substr(4,2)));
			d.setDay  (boost::lexical_cast<int>(str.substr(6,2)));
			d.setDateAsValid();

			return d;
		}
	}

	bool DicomRead::readFile(FileReader& filereader, OCT& oct, const FileReadOptions& op, CppFW::Callback* callback)
	{
		const std::string filename = filereader.getFilepath().generic_string();

		std::string ext = filereader.getFilepath().extension().generic_string();
		std::transform(ext.begin(), ext.end(), ext.begin(), ::tolower);
		if(ext != ".dicom"
		&& ext != ".dcm")
			return false;

// 		BOOST_LOG_TRIVIAL(info) << "ReadDICOM: " << filename;

		/* Load file and get pixel data element */
		DcmFileFormat dfile;
		OFCondition result = dfile.loadFile(filename.c_str());
		if(result.bad())
			return false;

		DcmDataset* data = dfile.getDataset();
		if(data == nullptr)
			return false;


		result = data->findAndGetSint32Array(DcmTagKey(0x0073, 0x1125), registerArray, &numRegisterElements);
		if(result.bad())
		{
			registerArray       = nullptr;
			numRegisterElements = 0;
		}

		// data->print(std::cout);
		
		bscans = 0;

		std::string pixelSpacingStr = getStdString(*data, DCM_PixelSpacing);
		if(!pixelSpacingStr.empty())
		{
			std::istringstream pixelSpaceingStream(pixelSpacingStr);
			char c;
			pixelSpaceingStream >> pixelSpaceingX >> c >> pixelSpaceingZ;
		}
		/*
		DcmElement* pixelSpaceingElement = nullptr;
		result = data->findAndGetElement(DCM_PixelSpacing, pixelSpaceingElement);
		if(!result.bad() || pixelSpaceingElement)
		{
			std::cerr << "pixelSpaceingElement->getLength(): " << pixelSpaceingElement->getLength() << std::endl;
			pixelSpaceingElement->getFloat64(pixelSpaceingX, 0);
			pixelSpaceingElement->getFloat64(pixelSpaceingZ, 1);
		}*/

		Patient& pat    = oct.getPatient(1);
		Study&   study  = pat.getStudy(1);
		Series&  series = study.getSeries(1); // TODO

		DcmElement* element = nullptr;
		result = data->findAndGetElement(DCM_PixelData, element);
		if(!result.bad() && element != nullptr)
			readPixelData(element, series, op, callback);
		else
		{
			DcmSequenceOfItems* items = nullptr;
			result = data->findAndGetSequence(DcmTagKey(0x0407, 0x10a1), items);
// 			result = data->findAndGetElement(DcmTagKey(0x0407, 0x10a1), element);
			if(!result.bad() && items != nullptr)
				readDict(items, series, op, callback);
			else
			{
				BOOST_LOG_TRIVIAL(error) << "cant find PixelData";
				return false;
			}
		}


		pat.setId     (getStdString(*data, DCM_PatientID  ));
		pat.setSurname(getStdString(*data, DCM_PatientName));

		std::string laterality = getStdString(*data, DCM_Laterality);
		if(laterality == "OD")
			series.setLaterality(Series::Laterality::OD);
		else if(laterality == "OS")
			series.setLaterality(Series::Laterality::OS);

		std::string patSex = getStdString(*data, DCM_PatientSex);
		if(patSex == "M")
			pat.setSex(Patient::Sex::Male);
		else if(patSex == "F")
			pat.setSex(Patient::Sex::Female);

		study.setStudyOperator(getStdString(*data, DCM_OperatorsName));
		study.setStudyUID(getStdString(*data, DCM_StudyInstanceUID));

		pat.setBirthdate(convertStr2Date(getStdString(*data, DCM_PatientBirthDate)));
		study.setStudyDate(convertStr2Date(getStdString(*data, DCM_StudyDate)));
		series.setScanDate(convertStr2Date(getStdString(*data, DCM_AcquisitionDateTime)));


		series.setSeriesUID(getStdString(*data, DCM_SeriesInstanceUID));

		data->findAndGetFloat64(DCM_SpacingBetweenSlices, spacingBetweenSlices);


		return true;
	}

	void DicomRead::readDict(DcmSequenceOfItems* sequence, Series& series, const FileReadOptions& op, CppFW::Callback* /*callback*/)
	{
		DcmStack stack;
		DcmObject* object = nullptr;
		/* iterate over all elements */
		while(sequence->nextObject(stack, OFTrue).good())
		{
			object = stack.top();
			if(object->getTag() == DcmTagKey(0x0407, 0x1006))
			{
				DcmOtherByteOtherWord* element = dynamic_cast<DcmOtherByteOtherWord*>(object);
				if(!element)
					continue;

				Uint8*      data   = nullptr;
				std::size_t length = element->getNumberOfValues();
				element->getUint8Array(data);

				decodeImage(series, op, reinterpret_cast<char*>(data), length);
			}
		}
	}
	
	
	void DicomRead::readPixelItem(DcmPixelSequence* dseq, Series& series, const FileReadOptions& op, unsigned long i)
	{
		OFCondition result;
		Uint8* pixData = nullptr;
		DcmPixelItem* pixitem = nullptr;
		
		dseq->getItem(pixitem, i);
		if(pixitem != NULL)
		{
			Uint32 length = pixitem->getLength();
			if(length == 0)
			{
				std::cerr << "unexpected pixitem lengt 0, ignore item\n";
			}
			else
			{
				result = pixitem->getUint8Array(pixData);
				if(result != EC_Normal)
				{
					std::cout << "defect Pixdata" << std::endl;
				}
			}
		}

		// Get the length of this pixel item (i.e. fragment, i.e. most of the time, the lenght of the frame)
		Uint32 length = pixitem->getLength();
		decodeImage(series, op, reinterpret_cast<char*>(pixData), length);
	}

	void DicomRead::readPixelData(DcmElement* element, Series& series, const FileReadOptions& op, CppFW::Callback* callback)
	{
		DcmPixelData* dpix = OFstatic_cast(DcmPixelData*, element);
		/* Since we have compressed data, we must utilize DcmPixelSequence
			in order to access it in raw format, e. g. for decompressing it
			with an external library.
		*/
		
		E_TransferSyntax xferSyntax = EXS_Unknown;
		const DcmRepresentationParameter* rep = nullptr;

		// Find the key that is needed to access the right representation of the data within DCMTK
		dpix->getOriginalRepresentationKey(xferSyntax, rep);
		if(!rep)
		{
			dpix->getCurrentRepresentationKey(xferSyntax, rep);
		}

		DcmPixelSequence* dseq = nullptr;
		// Access original data representation and get result within pixel sequence
		OFCondition result = dpix->getEncapsulatedRepresentation(xferSyntax, rep, dseq);

		if(result == EC_Normal)
		{

			// Access first frame (skipping offset table)
			// #pragma omp parallel for ordered schedule(dynamic) private(pixitem)
// 			#pragma omp parallel for ordered schedule(dynamic)
			if(op.readBScanNum >=0)
			{
				readPixelItem(dseq, series, op, op.readBScanNum);
				return;
			}
			
			const unsigned long maxEle = dseq->card();
			for(unsigned long k = 1; k<maxEle; ++k)
			{
				unsigned long i = k-1;
				if(callback)
				{
					if(!callback->callback(static_cast<double>(i)/static_cast<double>(maxEle)))
						break;
				}
				readPixelItem(dseq, series, op, k);
			}
		}
	}
	
	
	std::vector<char> decodeCirrusData(const char* pixDataPtr, std::size_t length)
	{
		std::vector<char> copyData(pixDataPtr, pixDataPtr + length);
		char* pixDataCopy = copyData.data();
		
		for(char* it = pixDataCopy; it < pixDataCopy+length; it+=7)
			*it ^= 0x5a;

		const std::size_t headerpos = 3*length/5;
		constexpr int headerlength = 253;
		
				
		std::vector<char> pixData;
		pixData.reserve(length);
		
		auto appendData = [&](std::size_t pos, std::size_t end){
			pixData.insert(pixData.end(), pixDataCopy + pos, pixDataCopy + end);
		};
		
		appendData(headerpos, headerpos+headerlength);
		appendData(993 , 1016     );
		appendData(276 , 763      );
		appendData(23  , 276      );
		appendData(1016, headerpos);
		appendData(0   , 23       );
		appendData(763 , 993      );
		appendData(headerpos+headerlength, length);
	
		
		return pixData;
	}

	
	void DicomRead::decodeImage(Series& series, const FileReadOptions& op, const char* pixData, std::size_t length)
	{
		std::size_t actBScan = bscans;
		++bscans;

		static const unsigned char jpeg2kHeader[8] = { 0x00, 0x00, 0x00, 0x0c, 0x6a, 0x50, 0x20, 0x20 };

		std::vector<char> usedPixData;
		if(memcmp(pixData, jpeg2kHeader, sizeof(jpeg2kHeader)) != 0) // encrypted cirrus data
		{
// 			BOOST_LOG_TRIVIAL(warning) << "Anormal JPEG2K, try reoder data";
// 			BOOST_LOG_TRIVIAL(info) << "Number: " << actBScan << "\tLength:" << length << "\t" << 932+actBScan;
			
			usedPixData = decodeCirrusData(pixData, length);
			pixData = usedPixData.data();
		}

		// ausleseversuch
		ReadJPEG2K obj;
		if(!obj.openJpeg(pixData, length))
			BOOST_LOG_TRIVIAL(error) << "Fehler beim JPEG2K-Einlesen";


		cv::Mat gray_image;

		bool flip = true; // for Cirrus
		obj.getImage(gray_image, flip);

		if(op.registerBScanns && numRegisterElements > actBScan)
		{
			// std::cout << "shift X: " << reg->values[9] << std::endl;
			double shiftY = -registerArray[actBScan];
			double shiftX = 0;
			// std::cout << "shift X: " << shiftX << "\tdegree: " << degree << "\t" << (degree*bscanImageConv.cols/2) << std::endl;
			cv::Mat trans_mat = (cv::Mat_<double>(2,3) << 1, 0, shiftX, 0, 1, shiftY);

			uint8_t fillValue = 0;
			if(op.fillEmptyPixelWhite)
				fillValue = 255;
			cv::warpAffine(gray_image, gray_image, trans_mat, gray_image.size(), cv::INTER_LINEAR, cv::BORDER_CONSTANT, cv::Scalar(fillValue));
		}

// 		#pragma omp ordered
		{
			if(!gray_image.empty())
			{
				BScan::Data bscanData;
				bscanData.scaleFactor = ScaleFactor(pixelSpaceingX, spacingBetweenSlices, pixelSpaceingZ);
				series.addBScan(std::make_shared<BScan>(gray_image, bscanData));
			}
			else
				BOOST_LOG_TRIVIAL(error) << "Empty openCV image\n";
		}
	}


}
#pragma message("build with dicom support")

#else
#pragma message("build withhout dicom support")

namespace OctData
{
	DicomRead::DicomRead()
	: OctFileReader()
	{ }

	bool DicomRead::readFile(OctData::FileReader& /*filereader*/, OctData::OCT& /*oct*/, const OctData::FileReadOptions& /*op*/, CppFW::Callback* /*callback*/)
	{
		return false;
	}

}

#endif // USE_DCMTK
