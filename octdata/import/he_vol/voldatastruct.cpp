#include"voldatastruct.h"

#include<octdata/datastruct/date.h>

#include<iostream>

namespace OctData
{
	
	void VolHeader::printData(std::ostream& stream)
	{
		stream << "version     : " << data.version      << '\n';
		stream << "sizeX       : " << data.sizeX        << '\n';
		stream << "numBScans   : " << data.numBScans    << '\n';
		stream << "sizeZ       : " << data.sizeZ        << '\n';
		stream << "scaleX      : " << data.scaleX       << '\n';
		stream << "distance    : " << data.distance     << '\n';
		stream << "scaleZ      : " << data.scaleZ       << '\n';
		stream << "sizeXSlo    : " << data.sizeXSlo     << '\n';
		stream << "sizeYSlo    : " << data.sizeYSlo     << '\n';
		stream << "scaleXSlo   : " << data.scaleXSlo    << '\n';
		stream << "scaleYSlo   : " << data.scaleYSlo    << '\n';
		stream << "fieldSizeSlo: " << data.fieldSizeSlo << '\n';
		stream << "scanFocus   : " << data.scanFocus    << '\n';
		stream << "scanPosition: " << data.scanPosition << '\n';
		stream << "examTime    : " << data.examTime     << '\t' << OctData::Date::fromWindowsTicks(data.examTime).timeDateStr() << '\n';
		stream << "scanPattern : " << data.scanPattern  << '\n';
		stream << "bScanHdrSize: " << data.bScanHdrSize << '\n';
		stream << "id          : " << data.id           << '\n';
		stream << "referenceID : " << data.referenceID  << '\n';
		stream << "pid         : " << data.pid          << '\n';
		stream << "patientID   : " << data.patientID    << '\n';
		stream << "padding     : " << data.padding      << '\n';
		stream << "dob         : " << data.dob          << '\t' << OctData::Date::fromWindowsTimeFormat(data.dob).timeDateStr() << '\n';
		stream << "vid         : " << data.vid          << '\n';
		stream << "visitID     : " << data.visitID      << '\n';
		stream << "visitDate   : " << data.visitDate    << '\t' << OctData::Date::fromWindowsTimeFormat(data.visitDate).timeDateStr() << '\n';
		stream << "gridType    : " << data.gridType     << '\n';
		stream << "gridOffset  : " << data.gridOffset   << '\n';
		stream << "progID      : ";
		for(std::size_t i = 0; i < sizeof(data.progID) && data.progID[i] != 0; ++i)
			stream << data.progID[i];
		stream << std::endl;
	}
	
	void BScanHeader::printData() const
	{
		std::cout << data.startX  << '\t'
					<< data.startY  << '\t'
					<< data.endX    << '\t'
					<< data.endY    << '\t'
					<< data.numSeg  << '\t'
					<< data.offSeg  << '\t'
					<< data.quality << '\t'
					<< data.shift   << std::endl;
	}
	
}


