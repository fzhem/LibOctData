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

#pragma once

#include "substructure_template.h"
#include "patient.h"

#include <tuple>

namespace OctData
{
	class OCT : public SubstructureTemplate<Patient>
	{
	public:
		Octdata_EXPORTS       Patient& getInsertId(int id)                        { return getAndInsert        (id) ; }

		Octdata_EXPORTS       Patient& getPatient(int patientId)                  { return getAndInsert        (patientId) ; }
		Octdata_EXPORTS const Patient& getPatient(int patientId) const            { return *(substructureMap.at(patientId)); }
		Octdata_EXPORTS void clear()                                              { clearSubstructure(); }

		Octdata_EXPORTS std::tuple<std::shared_ptr<const Patient>, std::shared_ptr<const Study>> findSeries(const std::shared_ptr<const Series>& seriesReq) const;


		template<typename T> void getSetParameter(T& /*getSet*/)       { }
		template<typename T> void getSetParameter(T& /*getSet*/) const { }
	};

}
