/*
 * Copyright (c) 2023 Kay Gawlik <kaydev@amarunet.de>
 *
 * This program is free software: you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation, either
 * version 3 of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#include "volwrite.h"


#include <import/he_vol/voldatastruct.h>

#include<iostream>
#include<fstream>
#include<iomanip>
#include<memory>
#include<filesystem>

#include<opencv2/opencv.hpp>

#include<boost/algorithm/string/classification.hpp>
#include<boost/algorithm/string/split.hpp>
#include<boost/lexical_cast.hpp>
#include<boost/log/trivial.hpp>

#include<oct_cpp_framework/callback.h>

#include<filereader/filereader.h>

#include<datastruct/oct.h>
#include<datastruct/coordslo.h>
#include<datastruct/sloimage.h>
#include<datastruct/bscan.h>

#include<algorithm>

namespace bfs = std::filesystem;


namespace OctData
{
	namespace
	{
		template<std::size_t n>
		void copyString(char (&dest)[n] , const std::string& source)
		{
			std::size_t maxLen = std::min(n-1, source.size());
			std::memcpy(dest, source.data(), maxLen);
			dest[maxLen] = '\0';
		}
		
		template<std::size_t n>
		void copyChars(char (&dest)[n] , const char (&source)[n+1])
		{
			std::memcpy(dest, source, n);
		}
		
		void copyMetaData(OctData::VolHeader& header, const OctData::Patient& pat, const OctData::Study& /*study*/, const OctData::Series& series)
		{
			// Patient data
			copyString(header.data.patientID, pat.getId());
			// pat.setBirthdate(OctData::Date::fromWindowsTimeFormat(header.data.dob));

			// Study data
			// study.setStudyDate(OctData::Date::fromWindowsTicks(header.data.examTime));
			
			// Series data
			// series.setScanDate(OctData::Date::fromWindowsTimeFormat(header.data.visitDate));
			switch(series.getLaterality())
			{
				case Series::Laterality::OD: copyString(header.data.scanPosition, "OD"); break;
				case Series::Laterality::OS: copyString(header.data.scanPosition, "OS"); break;
				case Series::Laterality::undef: break;
			}
			copyString(header.data.id, series.getSeriesUID());
			copyString(header.data.referenceID, series.getRefSeriesUID());
						
			// series.setScanDate(OctData::Date::fromWindowsTicks(data.examTime));

			switch(series.getScanPattern())
			{
				case OctData::Series::ScanPattern::SingleLine   : header.data.scanPattern = 1; break;
				case OctData::Series::ScanPattern::Circular     : header.data.scanPattern = 2; break;
				case OctData::Series::ScanPattern::Volume       : header.data.scanPattern = 3; break;
				case OctData::Series::ScanPattern::FastVolume   : header.data.scanPattern = 4; break;
				case OctData::Series::ScanPattern::Radial       : header.data.scanPattern = 5; break;
				case OctData::Series::ScanPattern::RadialCircles: header.data.scanPattern = 6; break;
				default:
					header.data.scanPattern = 0;
					BOOST_LOG_TRIVIAL(warning) << "Unknown scan pattern: " << header.data.scanPattern;
					break;
			}
			
			auto bscan = series.getBScan(0);
			const SloImage& sloImage = series.getSloImage();

			header.data.scanFocus = series.getScanFocus();
			
			header.data.bScanHdrSize = 256;
			header.data.numBScans = static_cast<uint32_t>(series.bscanCount());
			header.data.sizeX     = static_cast<uint32_t>(bscan->getWidth());
			header.data.sizeZ     = static_cast<uint32_t>(bscan->getHeight());
			if(sloImage.hasImage())
			{
				header.data.sizeXSlo  = static_cast<uint32_t>(sloImage.getWidth());
				header.data.sizeYSlo  = static_cast<uint32_t>(sloImage.getHeight());
				header.data.scaleXSlo = sloImage.getScaleFactor().getX();
				header.data.scaleYSlo = sloImage.getScaleFactor().getY();
			}
			else
			{
				header.data.sizeXSlo  = 1;
				header.data.sizeYSlo  = 1;
			}
		}
	}

	bool VolWrite::writeFile(const std::filesystem::path& file, const OctData::OCT& oct, const OctData::FileWriteOptions& opt)
	{
		OCT::SubstructureCIterator pat = oct.begin();
		const std::shared_ptr<Patient>& p = pat->second;
		if(!p)
			return false;

		Patient::SubstructureCIterator study = p->begin();
		const std::shared_ptr<Study>& s = study->second;
		if(!s)
			return false;

		Study::SubstructureCIterator series = s->begin();
		const std::shared_ptr<Series>& ser = series->second;

		if(!ser)
			return false;

		return writeFile(file, oct, *p, *s, *ser, opt);
	}


	bool VolWrite::writeFile(const std::filesystem::path& file
	                       , const OCT&              /*oct*/
	                       , const Patient&          pat
	                       , const Study&            study
	                       , const Series&           series
	                       , const FileWriteOptions& /*opt*/)
	{
		const std::shared_ptr<const BScan> bscan = series.getBScan(0);
		if(!bscan)
			return false;
		
		if(file.empty())
			return false;

		
		std::ofstream stream(file, std::ios::binary | std::ios::out | std::ios::trunc);
		
		auto fillStreamTo = [&stream](std::size_t pos){
			std::size_t currentPos = static_cast<std::size_t>(stream.tellp());
			assert(currentPos <= pos);
			for(std::size_t p = currentPos; p < pos; ++p)
				stream.put('\0');
		};
		
		auto writeStruct = [&stream](const auto& s){
			stream.write(reinterpret_cast<const char*>(&s), sizeof(s));
		};
		
		auto writeImage = [&stream](const cv::Mat& mat){
			assert(mat.dataend > mat.data);
			std::streamsize size = mat.dataend - mat.data;
			auto data = reinterpret_cast<const char*>(mat.data);
			stream.write(data, size);
		};
		
		
		stream << "HSF-OCT-";
		VolHeader volHeader{};
		copyChars(volHeader.data.version, "100\0");
		copyMetaData(volHeader, pat, study, series);
		
		writeStruct(volHeader.data);
		
		fillStreamTo(VolHeader::getHeaderSize());
		
		
		
		// Write SLO
		const SloImage& sloImage = series.getSloImage();
		if(sloImage.hasImage())
			writeImage(sloImage.getImage());
		else
		{
			uint8_t t = 0;
			writeStruct(t);
		}

		const std::size_t numBScans = series.bscanCount();
		// Read BScann
		for(std::size_t numBscan = 0; numBscan<numBScans; ++numBscan)
		{
			const std::shared_ptr<const BScan>& bscan = series.getBScan(numBscan);
			if(!bscan)
				continue;
			
			BScanHeader bscanHeader{};
			copyChars(bscanHeader.data.hsfOctRawStr, "HSF-BS-");
			copyChars(bscanHeader.data.version, "100\0\0");
			bscanHeader.data.bscanHdrSize = volHeader.data.bScanHdrSize;
			bscanHeader.data.startX  = bscan->getStart().getX();
			bscanHeader.data.startY  = bscan->getStart().getY();
			bscanHeader.data.endX    = bscan->getEnd  ().getX();
			bscanHeader.data.endY    = bscan->getEnd  ().getY();
			bscanHeader.data.numSeg  = 0;
			bscanHeader.data.offSeg  = 0;
			bscanHeader.data.quality = static_cast<float>(bscan->getImageQuality());
			bscanHeader.data.shift   = 0;

			std::size_t bscanPos = VolHeader::getHeaderSize() + volHeader.getSLOPixelSize() + numBscan*volHeader.getBScanSize();
			fillStreamTo(bscanPos);
			writeStruct(bscanHeader.data);
			
			
			fillStreamTo(256+bscanPos);
			
			// TODO: write segmentation lines


			fillStreamTo(volHeader.data.bScanHdrSize+bscanPos);
			cv::Mat bscanImageConv;
			cv::Mat bscanImagePow;
			
			bscan->getImage().convertTo(bscanImageConv,  CV_32F/*cv::DataType<float>::type*/, 1./255, 0);
			cv::pow(bscanImageConv, 4, bscanImagePow);
			writeImage(bscanImagePow);

		}
		
		return true;
	}
}
