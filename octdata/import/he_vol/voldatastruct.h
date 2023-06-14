#pragma once

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


#include "../../octdata_packhelper.h"

#include<iosfwd>
#include<cstdint>

namespace OctData
{

	struct VolHeader
	{
		PACKSTRUCT(struct RawData
		{
			// char HSF-OCT-        [ 8]; // check first in function
			char     version     [ 4];
			uint32_t sizeX           ;
			uint32_t numBScans       ;
			uint32_t sizeZ           ;
			double   scaleX          ;
			double   distance        ;
			double   scaleZ          ;
			uint32_t sizeXSlo        ;
			uint32_t sizeYSlo        ;
			double   scaleXSlo       ;
			double   scaleYSlo       ;
			uint32_t fieldSizeSlo    ;
			double   scanFocus       ;
			char     scanPosition[ 4];
			uint64_t examTime        ;
			uint32_t scanPattern     ;
			uint32_t bScanHdrSize    ;
			char     id          [16];
			char     referenceID [16];
			uint32_t pid             ;
			char     patientID   [21];
			char     padding     [ 3];
			double   dob             ; // Patient date of birth
			uint32_t vid             ;
			char     visitID     [24];
			double   visitDate       ;
			int32_t  gridType        ;
			int32_t  gridOffset      ;
			char     spare       [ 8];
			char     progID      [32];
		});
		RawData data;

		void printData(std::ostream& stream);

		std::size_t getSLOPixelSize() const   {
			return data.sizeXSlo*data.sizeYSlo;
		}
		std::size_t getBScanPixelSize() const {
			return data.sizeX   *data.sizeZ   *sizeof(float);
		}
		std::size_t getBScanSize() const      {
			return getBScanPixelSize() + data.bScanHdrSize;
		}

		constexpr static std::size_t getHeaderSize() {
			return 2048;
		}

	};

	struct BScanHeader
	{
		PACKSTRUCT(struct Data
		{
			char     hsfOctRawStr[ 7];
			char     version     [ 5];
			uint32_t bscanHdrSize    ;
			double   startX          ;
			double   startY          ;
			double   endX            ;
			double   endY            ;
			int32_t  numSeg          ;
			int32_t  offSeg          ;
			float    quality         ;
			int32_t  shift           ;
		});

		constexpr static const std::size_t identiferSize = sizeof(Data::hsfOctRawStr)/sizeof(Data::hsfOctRawStr[0]);

		Data data;

		void printData() const;
	};

	struct ThicknessGrid
	{
		PACKSTRUCT(struct Data
		{
			int    gridType     ;
			double diameterA    ;
			double diameterB    ;
			double diameterC    ;
			double centerPosXmm ;
			double centerPosYmm ;
			float  centralThk   ;
			float  minCentralThk;
			float  maxCentralThk;
			float  totalVolume  ;
			// sectors
		});

		Data data;
	};

}
