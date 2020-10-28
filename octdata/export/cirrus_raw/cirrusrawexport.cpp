/*
 * Copyright (c) 2018 Kay Gawlik <kaydev@amarunet.de> <kay.gawlik@beuth-hochschule.de> <kay.gawlik@charite.de>
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

#include "cirrusrawexport.h"

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


namespace bfs = std::filesystem;


namespace OctData
{
	namespace
	{
		void writeFliped(const cv::Mat& bscan, uint8_t* dest, std::size_t sizeX, std::size_t sizeY)
		{
			if(bscan.type() != cv::DataType<uint8_t>::type || bscan.channels() != 1)
				return;

			if(bscan.cols != static_cast<int>(sizeX)) BOOST_LOG_TRIVIAL(warning) << "CirrusRawExport: cols != sizeX " << bscan.cols << " != " << sizeX;
			if(bscan.rows != static_cast<int>(sizeY)) BOOST_LOG_TRIVIAL(warning) << "CirrusRawExport: cols != sizeX " << bscan.rows << " != " << sizeY;

			const int cols = std::min(bscan.cols, static_cast<int>(sizeX));
			const int rows = std::min(bscan.rows, static_cast<int>(sizeY));

			for(int r = 0; r < rows; ++r)
			{
				const uint8_t* sourceIt = bscan.ptr<uint8_t>(r);
				      uint8_t* destIt   = dest+(rows-r)*sizeX;
				for(int c = 0; c < cols; ++c)
				{
					--destIt;
					*destIt = *sourceIt;
					++sourceIt;
				}
			}
		}

		inline std::string avoidEmptyString(const std::string& str)
		{
			if(str.empty())
				return "undef";
			return str;
		}
	}

	bool CirrusRawExport::writeFile(const std::filesystem::path& file, const OctData::OCT& oct, const OctData::FileWriteOptions& opt)
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


	bool CirrusRawExport::writeFile(const std::filesystem::path& file
	                              , const OCT&              /*oct*/
	                              , const Patient&          pat
	                              , const Study&            study
	                              , const Series&           series
	                              , const FileWriteOptions& /*opt*/)
	{
		const std::shared_ptr<const BScan> bscan = series.getBScan(0);
		if(!bscan)
			return false;

		const std::size_t cubeSizeX = bscan->getImage().cols;
		const std::size_t cubeSizeY = bscan->getImage().rows;
		const std::size_t cubeSizeZ = series.bscanCount();

		Series::Laterality eyeSide = series.getLaterality();
		Series::LateralityEnumWrapper lateralityWrapper(eyeSide);

		Date date = series.getScanDate();
		if(date.isEmpty())
			date = study.getStudyDate();
		if(date.isEmpty())
			date = Date::fromUnixTime(0);

		const std::string patient_id = avoidEmptyString(pat.getId());
		      std::string scantype   = "";
		const std::string scan_date1 = avoidEmptyString(date.strUsDate('-'));
		const std::string scan_date2 = avoidEmptyString(date.strTime('-')  );
		      std::string eye_side   = avoidEmptyString(static_cast<std::string&>(lateralityWrapper));
		const std::string sn         = "sn1234";
		      std::string cube       = "";
		const std::string filetype   = "raw";

		switch(series.getExaminedStructure())
		{
			case Series::ExaminedStructure::ONH:
				scantype += "Optic Disc";
				break;
			case Series::ExaminedStructure::Retina:
				scantype += "Macular";
				break;
			case Series::ExaminedStructure::Text:
				scantype += series.getExaminedStructureText();
				break;
			case Series::ExaminedStructure::Unknown:
				scantype += "Unknown";
				break;
		}

		scantype += " ";

		Series::ScanPattern scanPattern = series.getScanPattern();
		switch(series.getScanPattern())
		{
			case Series::ScanPattern::Volume:
			case Series::ScanPattern::FastVolume:
				scantype += "Cube";
				cube      = "cube";
				break;
			case Series::ScanPattern::Text:
				scantype += series.getScanPatternText();
				cube      = series.getScanPatternText();
				break;
			default:
			{
				Series::ScanPatternEnumWrapper scanpatternWrapper(scanPattern);
				scantype += static_cast<std::string&>(scanpatternWrapper);
				cube      = static_cast<std::string&>(scanpatternWrapper);
			}
		}

		scantype += " ";
		scantype += boost::lexical_cast<std::string>(cubeSizeX) + "x" + boost::lexical_cast<std::string>(cubeSizeZ);

		std::string filename = patient_id + '_'
		                     + scantype   + '_'
		                     + scan_date1 + '_'
		                     + scan_date2 + '_'
		                     + eye_side   + '_'
		                     + sn         + '_'
		                     + cube       + '_'
		                     + filetype   + ".img";

		bfs::path exportpath = file.parent_path() / filename;

		std::ofstream stream(exportpath.generic_string(), std::ios_base::binary);

		std::unique_ptr<uint8_t> cache(new uint8_t[cubeSizeX*cubeSizeY*cubeSizeZ]);
		uint8_t* cacheIt = cache.get();

		const OctData::Series::BScanList& bscans = series.getBScans();
		      OctData::Series::BScanList::const_reverse_iterator beginIt = bscans.crbegin();
		const OctData::Series::BScanList::const_reverse_iterator endIt   = bscans.crend();

// 		      OctData::Series::BScanList::const_iterator beginIt = bscans.cbegin();
// 		const OctData::Series::BScanList::const_iterator endIt   = bscans.cend();

		while(endIt != beginIt)
		{
			if(*beginIt)
				writeFliped((*beginIt)->getImage(), cacheIt, cubeSizeX, cubeSizeY);

			cacheIt += cubeSizeX*cubeSizeY;
			++beginIt;
		}

		stream.write(reinterpret_cast<char*>(cache.get()), cubeSizeX*cubeSizeY*cubeSizeZ);


		return true;
	}
}
