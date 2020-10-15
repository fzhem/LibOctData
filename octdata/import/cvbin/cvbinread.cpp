#include "cvbinread.h"

#include<locale>
#include<filesystem>

#include <boost/log/trivial.hpp>
#include <boost/lexical_cast.hpp>

#include <opencv2/opencv.hpp>

#include <datastruct/oct.h>
#include <datastruct/coordslo.h>
#include <datastruct/sloimage.h>
#include <datastruct/bscan.h>

#include <filereadoptions.h>


#include <oct_cpp_framework/cvmat/cvmattreestruct.h>
#include <oct_cpp_framework/cvmat/cvmattreestructextra.h>
#include <oct_cpp_framework/cvmat/cvmattreegetset.h>
#include <oct_cpp_framework/cvmat/treestructbin.h>
#include <oct_cpp_framework/callback.h>
#include <oct_cpp_framework/matcompress/simplematcompress.h>
#include <octfileread.h>

#include<filereader/filereader.h>

namespace bfs = std::filesystem;


namespace OctData
{

	namespace
	{

		const CppFW::CVMatTree* getDirNodeOptCamelCase(const CppFW::CVMatTree& node, const char* name)
		{
			const CppFW::CVMatTree* result = node.getDirNodeOpt(name);
			if(!result)
			{
				std::string nameFirstUpper(name);
				nameFirstUpper[0] = static_cast<std::string::value_type>(std::toupper(nameFirstUpper[0], std::locale()));
				result = node.getDirNodeOpt(nameFirstUpper.c_str());
			}
			return result;
		}


		// general export methods
		std::unique_ptr<SloImage> readSlo(const CppFW::CVMatTree* sloNode)
		{
			if(!sloNode)
				return nullptr;

			BOOST_LOG_TRIVIAL(trace) << "read slo data node";

			const CppFW::CVMatTree* imgNode = sloNode->getDirNodeOpt("img");
			if(imgNode && imgNode->type() == CppFW::CVMatTree::Type::Mat)
			{
				std::unique_ptr<SloImage> slo = std::make_unique<SloImage>();
				CppFW::GetFromCVMatTree sloWriter(*sloNode);
				slo->getSetParameter(sloWriter);
				slo->setImage(imgNode->getMat());
				return slo;
			}
			return nullptr;
		}

		void fillSegmentationsLines(const CppFW::CVMatTree* segNode, BScan::Data& bscanData)
		{
			if(segNode)
			{
				for(OctData::Segmentationlines::SegmentlineType type : OctData::Segmentationlines::getSegmentlineTypes())
				{
					const char* segLineName = Segmentationlines::getSegmentlineName(type);

					const CppFW::CVMatTree* seriesSegILMNode = segNode->getDirNodeOpt(segLineName);
					if(seriesSegILMNode && seriesSegILMNode->type() == CppFW::CVMatTree::Type::Mat)
					{
						const cv::Mat& segMat = seriesSegILMNode->getMat();
						cv::Mat convertedSegMat;
						segMat.convertTo(convertedSegMat, cv::DataType<double>::type);

						const double* p = convertedSegMat.ptr<double>(0);
						std::vector<double> segVec(p, p + convertedSegMat.rows*convertedSegMat.cols);

						bscanData.getSegmentLine(type) = std::move(segVec);
					}
				}
			}
		}

		std::shared_ptr<BScan> readBScan(const CppFW::CVMatTree* bscanNode)
		{
			if(!bscanNode)
				return nullptr;

			std::shared_ptr<BScan> bscan;
			const CppFW::CVMatTree* imgNode = bscanNode->getDirNodeOpt("img");
			if(imgNode)
			{
				cv::Mat img;

				if(imgNode->type() == CppFW::CVMatTree::Type::Dir)
				{
					CppFW::SimpleMatCompress matcompress;
					matcompress.fromCVMatTree(*imgNode);
					img = cv::Mat(matcompress.getRows(), matcompress.getCols(), cv::DataType<uint8_t>::type);
					matcompress.writeToMat(img.ptr<uint8_t>(), img.rows, img.cols);
				}

				if(imgNode->type() == CppFW::CVMatTree::Type::Mat)
					img = imgNode->getMat();

				if(!img.empty())
				{
					BScan::Data bscanData;

					const CppFW::CVMatTree* seriesSegNode = getDirNodeOptCamelCase(*bscanNode, "segmentations");
					fillSegmentationsLines(seriesSegNode, bscanData);

					bscan = std::make_shared<BScan>(img, bscanData);

					CppFW::GetFromCVMatTree bscanReader(bscanNode->getDirNodeOpt("data"));
					bscan->getSetParameter(bscanReader);


					const CppFW::CVMatTree* angioNode = bscanNode->getDirNodeOpt("angioImg");
					if(angioNode && angioNode->type() == CppFW::CVMatTree::Type::Mat)
						bscan->setAngioImage(angioNode->getMat());
				}
			}

			return bscan;
		}

		bool readBScanList(const CppFW::CVMatTree::NodeList& seriesList, Series& series, CppFW::Callback* callback)
		{
			BOOST_LOG_TRIVIAL(trace) << "read bscan list";

			CppFW::CallbackStepper bscanCallbackStepper(callback, seriesList.size());
			for(const CppFW::CVMatTree* bscanNode : seriesList)
			{
				if(++bscanCallbackStepper == false)
					return false;

				std::shared_ptr<BScan> bscan = readBScan(bscanNode);
				if(bscan)
					series.addBScan(std::move(bscan));
			}
			return true;
		}




		// deep file format (support many scans per file, tree structure)
		template<typename S>
		bool readStructure(const CppFW::CVMatTree& tree, S& structure, CppFW::Callback* callback)
		{
			bool result = true;
			const CppFW::CVMatTree* dataNode = tree.getDirNodeOpt("data");
			if(dataNode)
			{
				CppFW::GetFromCVMatTree structureReader(dataNode);
				structure.getSetParameter(structureReader);
			}


			for(const CppFW::CVMatTree::NodePair& subNodePair : tree.getNodeDir())
			{
				if(!subNodePair.second)
					continue;

				const std::string& nodeName = subNodePair.first;
				if(nodeName.substr(0,3) == "id_")
				{
					const std::string& nodeIdStr = nodeName.substr(3, 50);

					try
					{
						int id = boost::lexical_cast<int>(nodeIdStr);
						readStructure(*(subNodePair.second), structure.getInsertId(id), callback);
					}
					catch(const boost::bad_lexical_cast&)
					{
					}
				}
			}
			return result;
		}


		template<>
		bool readStructure<Series>(const CppFW::CVMatTree& tree, Series& series, CppFW::Callback* callback)
		{
			const CppFW::CVMatTree* dataNode = tree.getDirNodeOpt("data");
			if(dataNode)
			{
				CppFW::GetFromCVMatTree seriesReader(dataNode);
				series.getSetParameter(seriesReader);
			}


			const CppFW::CVMatTree* sloNode = tree.getDirNodeOpt("slo");
			series.takeSloImage(readSlo(sloNode));

			const CppFW::CVMatTree* bscansNode = getDirNodeOptCamelCase(tree, "bscans");
			if(!bscansNode)
				return false;

			const CppFW::CVMatTree::NodeList& seriesList = bscansNode->getNodeList();
			return readBScanList(seriesList, series, callback);
		}

		bool readTreeData(OCT& oct, const CppFW::CVMatTree& octtree, CppFW::Callback* callback)
		{
			return readStructure(octtree, oct, callback);
		}




		bool readFlatData(OCT& oct, const CppFW::CVMatTree& octtree, const CppFW::CVMatTree* seriesNode, CppFW::Callback* callback)
		{
			BOOST_LOG_TRIVIAL(trace) << "open flat octbin structure";
			if(seriesNode->type() != CppFW::CVMatTree::Type::List)
			{
				BOOST_LOG_TRIVIAL(debug) << "Serie node not found or false datatype";
				return false;
			}

			const CppFW::CVMatTree* patDataNode    = getDirNodeOptCamelCase(octtree, "patientData");
			const CppFW::CVMatTree* studyDataNode  = getDirNodeOptCamelCase(octtree, "studyData"  );
			const CppFW::CVMatTree* seriesDataNode = getDirNodeOptCamelCase(octtree, "seriesData" );

			int patId    = CppFW::CVMatTreeExtra::getCvScalar(patDataNode   , "ID", 1);
			int studyId  = CppFW::CVMatTreeExtra::getCvScalar(studyDataNode , "ID", 1);
			int seriesId = CppFW::CVMatTreeExtra::getCvScalar(seriesDataNode, "ID", 1);

			Patient& pat    = oct  .getPatient(patId   );
			Study&   study  = pat  .getStudy  (studyId );
			Series&  series = study.getSeries (seriesId);

			if(patDataNode)
			{
				BOOST_LOG_TRIVIAL(trace) << "read patient data node";
				CppFW::GetFromCVMatTree patientReader(*patDataNode);
				pat.getSetParameter(patientReader);
			}

			if(studyDataNode)
			{
				BOOST_LOG_TRIVIAL(trace) << "read study data node";
				CppFW::GetFromCVMatTree studyReader(*studyDataNode);
				study.getSetParameter(studyReader);
			}

			if(seriesDataNode)
			{
				BOOST_LOG_TRIVIAL(trace) << "read series data node";
				CppFW::GetFromCVMatTree seriesReader(*seriesDataNode);
				series.getSetParameter(seriesReader);
			}

			const CppFW::CVMatTree* sloNode = octtree.getDirNodeOpt("slo");
			series.takeSloImage(readSlo(sloNode));

			const CppFW::CVMatTree::NodeList& seriesList = seriesNode->getNodeList();
			return readBScanList(seriesList, series, callback);
		}

	}

	CvBinRead::CvBinRead()
	: OctFileReader(OctExtension(".octbin", "CvBin format"))
	{
	}

	bool CvBinRead::readFile(FileReader& filereader, OCT& oct, const FileReadOptions& /*op*/, CppFW::Callback* callback)
	{
		const std::filesystem::path& file = filereader.getFilepath();
//
//     BOOST_LOG_TRIVIAL(trace)   << "A trace severity message";
//     BOOST_LOG_TRIVIAL(debug)   << "A debug severity message";
//     BOOST_LOG_TRIVIAL(info)    << "An informational severity message";
//     BOOST_LOG_TRIVIAL(warning) << "A warning severity message";
//     BOOST_LOG_TRIVIAL(error)   << "An error severity message";
//     BOOST_LOG_TRIVIAL(fatal)   << "A fatal severity message";

		if(file.extension() != ".octbin" && file.extension() != ".bin")
			return false;

		BOOST_LOG_TRIVIAL(trace) << "Try to open OCT file as bins";

		CppFW::CallbackSubTaskCreator callbackBasisTasks(callback, 4);

		CppFW::Callback loadTask    = callbackBasisTasks.getSubTaskCallback(3);
		CppFW::Callback convertTask = callbackBasisTasks.getSubTaskCallback(1);

		CppFW::CVMatTree octtree = CppFW::CVMatTreeStructBin::readBin(file.generic_string(), &loadTask);

		if(octtree.type() != CppFW::CVMatTree::Type::Dir)
		{
			BOOST_LOG_TRIVIAL(trace) << "false internal structure of bin";
			return false;
		}

		bool fillStatus;
		const CppFW::CVMatTree* seriesNode = getDirNodeOptCamelCase(octtree, "serie");
		if(seriesNode)
			fillStatus = readFlatData(oct, octtree, seriesNode, &convertTask);
		else
			fillStatus = readTreeData(oct, octtree, &convertTask);



		if(fillStatus)
			BOOST_LOG_TRIVIAL(debug) << "read bin file \"" << file.generic_string() << "\" finished";
		else
			BOOST_LOG_TRIVIAL(debug) << "read bin file \"" << file.generic_string() << "\" failed";

		return fillStatus;

	}


}
