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

#include "oct.h"

namespace OctData
{
	std::tuple<std::shared_ptr<const Patient>, std::shared_ptr<const Study>> OCT::findSeries(const std::shared_ptr<const Series>& seriesReq) const
	{
		if(!seriesReq)
			return {nullptr, nullptr};

		// Search series
		for(const OCT::SubstructurePair& patientPair : *this)
		{
			const OCT::SubstructureTypePtr& actPatient = patientPair.second;
			for(const Patient::SubstructurePair& studyPair : *actPatient)
			{
				const Patient::SubstructureTypePtr& actStudy = studyPair.second;
				for(const Study::SubstructurePair& seriesPair : *actStudy)
				{
					const Study::SubstructureTypePtr& series = seriesPair.second;
					if(series == seriesReq)
						return {actPatient, actStudy};
				}
			}
		}
		return {nullptr, nullptr};
	}
}
