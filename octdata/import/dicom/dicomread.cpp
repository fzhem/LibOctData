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

namespace
{
	/*
	cv::Mat decodeJPEG2k(const char* pixData, std::size_t pixDataLength)
	{
		const char* usedPixData = reinterpret_cast<const char*>(pixData);
		bool deleteUsedPixData = false;

		const unsigned char jpeg2kHeader[8] =
		{
		    0x00, 0x00, 0x00, 0x0c,
		    0x6a, 0x50, 0x20, 0x20
		};

		if(memcmp(pixData, jpeg2kHeader, sizeof(jpeg2kHeader)) != 0)	// encrypted cirrus
		{
			std::cout << "Anormal JPEG2K, versuche umsortierung" << std::endl;
			for(uint8_t* it = pixData; it < pixData+pixDataLength; it+=7)
				*it ^= 0x5a;

			// umsortieren
			char* newPixData = new char[pixDataLength];
			deleteUsedPixData = true;
			std::size_t headerpos = 3*pixDataLength/5;
			std::size_t blocksize = pixDataLength/5; //  512; //
			memcpy(newPixData, pixData+headerpos, blocksize);
			memcpy(newPixData+blocksize, pixData, headerpos);
			memcpy(newPixData+headerpos+blocksize, pixData+headerpos+blocksize, pixDataLength-headerpos-blocksize);

			usedPixData = newPixData;

/ *
			// test datei schreiben
			std::string number = boost::lexical_cast<std::string>(i);
			std::fstream outFileTest("/home/kay/Arbeitsfläche/jp2/" + number + ".jp2", std::ios::binary | std::ios::out);
			outFileTest.write(reinterpret_cast<const char*>(usedPixData), length);
			outFileTest.close();
* /
		}

		if(memcmp(pixData, jpeg2kHeader, sizeof(jpeg2kHeader)) != 0)	// decrypted cirrus data not successful
			return cv::Mat();


		// ausleseversuch
		ReadJPEG2K obj;
		if(!obj.openJpeg(usedPixData, pixDataLength))
			std::cerr << "Fehler beim JPEG2K-Einlesen" << std::endl;

		std::cout << "*" << std::flush;


		cv::Mat gray_image;

		bool flip = true; // for Cirrus
		obj.getImage(gray_image, flip);

		if(deleteUsedPixData)
			delete[] usedPixData;

		return gray_image;
	}
	*/
}


namespace OctData
{
	DicomRead::DicomRead()
	: OctFileReader({OctExtension{".dicom", ".dcm", "Dicom File"}, OctExtension("DICOMDIR", "DICOM DIR")})
	{
	}


#if false
	bool DicomRead::readFile(FileReader& filereader, OCT& oct, const FileReadOptions& /*op*/, CppFW::Callback* /*callback*/)
// 	bool readFile(const std::filesystem::path& file, OCT& oct, const FileReadOptions& /*op*/, CppFW::Callback* /*callback*/)
	{
		const std::filesystem::path& file = filereader.getFilepath();

		std::string ext = file.extension().generic_string();
		std::string filename = file.filename().generic_string();

		std::transform(ext.begin()     , ext.end()     , ext.begin()     , ::tolower);
		std::transform(filename.begin(), filename.end(), filename.begin(), ::tolower);

		if(filename == "dicomdir")
			return readDicomDir(file, oct);

		if(ext != ".dicom" && ext != ".dcm")
			return false;

		if(!bfs::exists(file))
			return false;

		std::cout << "ReadDICOM: " << filename << std::endl;

		OFCondition result = EC_Normal;
		/* Load file and get pixel data element */
		DcmFileFormat dfile;
		result = dfile.loadFile(file.c_str());
		std::cout << __LINE__ << std::endl;
		if(result.bad())
			return false;

		std::cout << __LINE__ << std::endl;

		DcmDataset *data = dfile.getDataset();
		if (data == NULL)
			return false;
		std::cout << __LINE__ << std::endl;

		data->print(std::cout);

		return false;
	}

	bool DicomRead::readDicomDir(const std::filesystem::path& file, OCT& /*oct*/)
	{
		DcmDicomDir dicomdir(file.c_str());

		// dicomdir.print(std::cout);
	//Retrieve root node
		DcmDirectoryRecord *root = &dicomdir.getRootRecord();
		//Prepare child elements
		DcmDirectoryRecord* rootTest      = new DcmDirectoryRecord(*root);
		DcmDirectoryRecord* patientRecord = nullptr;
		DcmDirectoryRecord* studyRecord   = nullptr;
		DcmDirectoryRecord* seriesRecord  = nullptr;
		DcmDirectoryRecord* image         = nullptr;

		if(rootTest == nullptr || rootTest->nextSub(patientRecord) == nullptr)
		{
			std::cout << "It looks like the selected file does not have the expected format." << std::endl;
			return false;
		}
		else
		{
			while((patientRecord = root->nextSub(patientRecord)) != nullptr)
			{
#define ReadRecord(XX) const char* str##XX; patientRecord->findAndGetString(XX, str##XX); if(str##XX) std::cout << #XX" : " << str##XX << std::endl;
// 				const char* patName;
// 				const char* patId;
// 				patientRecord->findAndGetString(DCM_PatientName, patName);
// 				patientRecord->findAndGetString(DCM_PatientID  , patId);

				ReadRecord(DCM_PatientName);
				ReadRecord(DCM_PatientID);
				ReadRecord(DCM_IssuerOfPatientID);
				ReadRecord(DCM_TypeOfPatientID);
				ReadRecord(DCM_IssuerOfPatientIDQualifiersSequence);
				ReadRecord(DCM_PatientSex);
				ReadRecord(DCM_PatientBirthDate);
				ReadRecord(DCM_PatientBirthTime);

// 				if(patName)
// 					std::cout << "patName: " << patName << std::endl;
// 				if(patId)
// 					std::cout << "patId: " << patId << std::endl;
//           if (sqi->GetItem(itemused).FindDataElement(gdcm::Tag (0x0010, 0x0010)))
//             sqi->GetItem(itemused).GetDataElement(gdcm::Tag (0x0010, 0x0010)).GetValue().Print(strm);
//           std::cout << "PATIENT NAME : " << strm.str() << std::endl;
//
				while((studyRecord = patientRecord->nextSub(studyRecord)) != nullptr)
				{
#undef ReadRecord
#define ReadRecord(XX) const char* str##XX; studyRecord->findAndGetString(XX, str##XX); if(str##XX) std::cout << #XX" : " << str##XX << std::endl;
					ReadRecord(DCM_StudyID);
					while((seriesRecord = studyRecord->nextSub(seriesRecord)) != nullptr)
					{
						while((image = seriesRecord->nextSub(image)) != nullptr)
						{
							const char *sName;
							//Retrieve the file name
							image->findAndGetString(DCM_ReferencedFileID, sName);

							//If a file is selected
							if(sName != nullptr)
							{
							//sName is the path for the file from the DICOMDIR file
							//You need to create the absolute path to use the DICOM file

							//Here you can do different tests (does the file exists ? for example)

							//Treat the dicom file
								std::cout << "sName: " << sName << std::endl;


							}
						}
					}
				}
			}
		}

		return false;
	}

#endif

	#if false
	void ReadDICOM::readFile(const std::string& filename, CScan* cscan)
	{

		if(!cscan)
			return;

		bfs::path xmlfile(filename);
		if(!bfs::exists(xmlfile))
			return;


		/*
		DcmDicomDir dicomdir(filename.c_str());
		dicomdir.print(std::cout);
		dicomdir.getRootRecord();
		*/



		//Open the DICOMDIR File
		// QString DICOMDIR_folder = "C:/Folder1/Folder2";
		const char *fileName = filename.c_str();
		DcmDicomDir dicomdir(fileName);


		dicomdir.print(std::cout);

		//Retrieve root node
		DcmDirectoryRecord *root = &dicomdir.getRootRecord();
		//Prepare child elements
		DcmDirectoryRecord *rootTest = new DcmDirectoryRecord(*root);
		DcmDirectoryRecord *PatientRecord = NULL;
		DcmDirectoryRecord *StudyRecord = NULL;
		DcmDirectoryRecord *SeriesRecord = NULL;
		DcmDirectoryRecord *image = NULL;

		if(rootTest == NULL || rootTest->nextSub(PatientRecord) == NULL)
			std::cout << "It looks like the selected file does not have the expected format." << std::endl;
		else
		{
			while((PatientRecord = root->nextSub(PatientRecord)) != NULL)
			{
				while((StudyRecord = PatientRecord->nextSub(StudyRecord)) != NULL)
				{
					while((SeriesRecord = StudyRecord->nextSub(SeriesRecord)) != NULL)
					{
						while((image = SeriesRecord->nextSub(image)) != NULL)
						{
							const char *sName;
							//Retrieve the file name
							image->findAndGetString(DCM_ReferencedFileID, sName);

							//If a file is selected
							if(sName != "")
							{
							//sName is the path for the file from the DICOMDIR file
							//You need to create the absolute path to use the DICOM file

							//Here you can do different tests (does the file exists ? for example)

							//Treat the dicom file
								std::cout << sName << std::endl;


							}
						}
					}
				}
			}
		}

		return;

		// http://support.dcmtk.org/redmine/projects/dcmtk/wiki/Howto_AccessingCompressedData
		// https://stackoverflow.com/questions/28219632/dicom-accessing-compressed-data-dcmtkt

		DicomImage DCM_image(filename.c_str());

		int depth      = DCM_image.getDepth();
		int n_channels = 1;

		std::cout << "depth: " << depth << std::endl;


		for(std::size_t i = 0; i<DCM_image.getFrameCount(); ++i)
		{
			cv::Mat image(DCM_image.getHeight(), DCM_image.getWidth(), CV_MAKETYPE(depth, n_channels), const_cast<void*>(DCM_image.getOutputData(0, i)));

			cv::Mat dest;
			image.convertTo(dest, CV_32FC1, 1/static_cast<double>(1 << 16), 0);

			cscan->takeBScan(new BScan(dest, BScan::Data()));
		}

	}
	#endif

	#if true

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
// 	void ReadDICOM::readFile(const std::string& filename, CScan* cscan)
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
	#endif

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

#if false
				static int i = 0;
				if(i++!=4)
					continue;
				for(int i = 0; i < 300; ++i)
					decodeImage(series, op, reinterpret_cast<char*>(data), length);
				break;
#else
				decodeImage(series, op, reinterpret_cast<char*>(data), length);
#endif
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

		DcmPixelSequence* dseq = nullptr;
		E_TransferSyntax xferSyntax = EXS_Unknown;
		const DcmRepresentationParameter* rep = nullptr;

		// Find the key that is needed to access the right representation of the data within DCMTK
		dpix->getOriginalRepresentationKey(xferSyntax, rep);

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
#if false	
			for(int i = 0; i < 1000; ++i)
				readPixelItem(dseq, series, op, 4);
			
			callback->callback(0.5);
#else
			
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

// 				std::cout << " ---- " << i << std::endl;

// 	#pragma omp critical
// 				if(!pixData)
				
			}
#endif
		}
	}
	
	
	std::unique_ptr<char[]> manipulateTest2(char* pixDataPtr, std::size_t length, std::vector<int> manipulator)
	{	
		for(char* it = pixDataPtr; it < pixDataPtr+length; it+=7)
			*it ^= 0x5a;

		static const std::vector<uint8_t> manipulator2 = {105 , 186,  249  ,179   , 0 , 203  ,232 , 100 , 134 , 156 ,  31  ,214 ,  46 ,204 , 250 ,  15 , 244  ,185 , 255 ,  83 , 255 ,  65 ,  44  ,218 , 255 , 201 ,  49   , 0 , 109  ,  0   , 0  ,  0  ,183  , 45 , 158 , 246,  255  , 50  , 93};
		const std::size_t headerpos = 3*length/5;
		constexpr int headerlength = 305;
		
		static int v = 300;
		pixDataPtr[v] ^= 0x7a;
		++v;
		
		
		std::unique_ptr<char[]> pixData(new char[length]);
		char* newPixData = pixData.get();

		memcpy(newPixData, pixDataPtr, length);
		memcpy(newPixData, pixDataPtr+headerpos, headerlength);
		memcpy(newPixData+headerpos, pixDataPtr, headerlength);
		
 		char* dataPtr = newPixData + 2*length/45 - 15;
		for(int val : manipulator2)
		{
// 			int val = manipulator[i];
			*dataPtr ^= static_cast<char>(val);
			++dataPtr;
// 			BOOST_LOG_TRIVIAL(info) << val;
		} 
		
		dataPtr = newPixData + 900;
		for(int val : manipulator)
		{
// 			int val = manipulator[i];
			*dataPtr ^= static_cast<char>(val);
			++dataPtr;
// 			BOOST_LOG_TRIVIAL(info) << val;
		}
		return pixData;
	}
	
	std::unique_ptr<char[]> manipulateTest(char* pixDataPtr, std::size_t length)
	{	
		for(char* it = pixDataPtr; it < pixDataPtr+length; it+=7)
			*it ^= 0x5a;

		const std::size_t headerpos = 3*length/5;
		constexpr int headerlength = 250;
		

// 		pixDataPtr[200] ^= static_cast<char>(124);
		
		
		std::unique_ptr<char[]> pixData(new char[length]);
		char* newPixData = pixData.get();

		memcpy(newPixData, pixDataPtr, length);
		memcpy(newPixData, pixDataPtr+headerpos, headerlength);
		memcpy(newPixData+headerpos, pixDataPtr, headerlength);
		
		
		newPixData[256] ^= static_cast<char>(-66);
		newPixData[915] ^= static_cast<char>(-25);
		
// 		newPixData[216] ^= static_cast<char>(51);
// 		newPixData[663] ^= static_cast<char>(126);
		
// 		newPixData[455] ^= static_cast<char>(134);
// 		newPixData[915] ^= static_cast<char>(231);
// 		
// 		static int v = 200;
// 		newPixData[v] ^= static_cast<char>(231);
// 		++v;
// 		static int v = 0;
// 		newPixData[455] = static_cast<char>(newPixData[455] ^ v);
// 		++v;
		
		return pixData;
	}

	std::unique_ptr<char[]> manipulate(char* pixDataPtr, std::size_t length, std::vector<int> manipulator)
	{	
		for(char* it = pixDataPtr; it < pixDataPtr+length; it+=7)
			*it ^= 0x5a;

		const std::size_t headerpos = 3*length/5;

// 		std::size_t headerlength = 200+actBScan;
		std::size_t headerlength = 305;
// 		std::size_t headerlength = 245;
		
		if(!manipulator.empty())
		{
			headerlength = 200+manipulator.front();
			manipulator.pop_back();
 			BOOST_LOG_TRIVIAL(info) << "headerlength: " << headerlength;
		}

		std::unique_ptr<char[]> pixData(new char[length]);
		char* newPixData = pixData.get();

		
		memcpy(newPixData, pixDataPtr, length);
		memcpy(newPixData, pixDataPtr+headerpos, headerlength);
		memcpy(newPixData+headerpos, pixDataPtr, headerlength);

// 		newPixData[actBScan+300+400] = static_cast<char>(~newPixData[actBScan+300+400]);
// 		newPixData[actBScan+300+600] ^= 0x5a;

// 		newPixData[932] ^= static_cast<char>(actBScan);
// 		newPixData[932] ^= static_cast<char>(0xc3);
// 		newPixData[938] ^= static_cast<char>(actBScan);

// 		newPixData[932] ^= static_cast<char>(0xc3);
// 		newPixData[946] ^= static_cast<char>(44);
		/*
		char* dataPtr = newPixData + length/45 - 10;
		for(std::size_t i = 0; i < manipulator.size()/2; ++i)
		{
			int val = manipulator[i];
			*dataPtr ^= static_cast<char>(val);
			++dataPtr;
		}
		*/
// 		dataPtr = newPixData + 2*length/45 - 10;
// 		for(std::size_t i = manipulator.size()/2; i < manipulator.size(); ++i)
 		char* dataPtr = newPixData + 2*length/45 - 15;
		for(int val : manipulator)
		{
// 			int val = manipulator[i];
			*dataPtr ^= static_cast<char>(val);
			++dataPtr;
// 			BOOST_LOG_TRIVIAL(info) << val;
		}
		
		newPixData[455] ^= static_cast<char>(134);
		newPixData[915] ^= static_cast<char>(231);
		
		return pixData;
	}
	
	
	void DicomRead::decodeImage(Series& series, const FileReadOptions& op, char* pixData, std::size_t length)
	{
		std::unique_ptr<char[]> copyPixData{new char[length]};
		memcpy(copyPixData.get(), pixData, length);

		std::size_t actBScan = bscans;
		++bscans;

		std::ofstream stream("img_" + std::to_string(actBScan) + ".bim", std::ios::binary);
		stream.write(pixData, length);
		stream.close();

		static const unsigned char jpeg2kHeader[8] = { 0x00, 0x00, 0x00, 0x0c, 0x6a, 0x50, 0x20, 0x20 };

		std::unique_ptr<char[]> usedPixData;
		if(memcmp(pixData, jpeg2kHeader, sizeof(jpeg2kHeader)) != 0) // non unencrypted cirrus
		{
// 			BOOST_LOG_TRIVIAL(warning) << "Anormal JPEG2K, try reoder data";
// 			BOOST_LOG_TRIVIAL(info) << "Number: " << actBScan << "\tLength:" << length << "\t" << 932+actBScan;
			char* pixDataPtr = copyPixData.get();
			
			if(op.xorTest.empty())
				usedPixData = manipulateTest(pixDataPtr, length);
			else
				usedPixData = manipulate(pixDataPtr, length, op.xorTest);
// 			usedPixData = manipulateTest2(pixDataPtr, length, op.xorTest);
		
// 			std::cout << i << "data: " << length << "\t" << headerpos << "\t" << std::endl;
		}

		// ausleseversuch
		ReadJPEG2K obj;
		if(usedPixData)
		{
			if(!obj.openJpeg(usedPixData.get(), length))
				BOOST_LOG_TRIVIAL(error) << "Fehler beim JPEG2K-Einlesen";
		}
		else
			if(!obj.openJpeg(copyPixData.get(), length))
				BOOST_LOG_TRIVIAL(error) << "Fehler beim JPEG2K-Einlesen";


		cv::Mat gray_image;

		bool flip = false; // for Cirrus
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

//  		putText(gray_image, boost::lexical_cast<std::string>(actBScan), cv::Point(5, 850), cv::FONT_HERSHEY_PLAIN, 5, cv::Scalar(255));
//  		putText(gray_image, boost::lexical_cast<std::string>(length), cv::Point(5, 880), cv::FONT_HERSHEY_PLAIN, 3, cv::Scalar(255));

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
