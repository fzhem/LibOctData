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

#include<ostream>
#include<type_traits>

#include "substructure_template.h"
#include "study.h"
#include "date.h"

#include"objectwrapper.h"

namespace OctData
{
	class Patient : public SubstructureTemplate<Study>
	{
	public:
		explicit Patient(int internalId) : internalId(internalId) {}

		enum class Sex { Unknown, Female, Male};
		typedef ObjectWrapper<Sex> SexEnumWrapper;
		
		struct PatientData
		{
			std::string forename;
			std::string surname ;
			std::string title   ;
			std::string id      ;
			std::string uid     ;
			std::string ancestry;

			std::u16string diagnose;

			Date birthdate      ;

			Sex sex = Sex::Unknown;
		};

		static const char* getSexName(Sex sex);
		const char* getSexName()          const                  { return getSexName(data.sex); }

		const std::string& getForename () const                  { return data.forename ; }
		const std::string& getSurname  () const                  { return data.surname  ; }
		const std::string& getTitle    () const                  { return data.title    ; }
		const std::string& getId       () const                  { return data.id       ; }
		Sex                getSex      () const                  { return data.sex      ; }
		const Date&        getBirthdate() const                  { return data.birthdate; }

		const std::u16string& getDiagnose() const                { return data.diagnose;  }


		void setForename (const std::string& v)                  { data.forename  = v ; }
		void setSurname  (const std::string& v)                  { data.surname   = v ; }
		void setTitle    (const std::string& v)                  { data.title     = v ; }
		void setId       (const std::string& v)                  { data.id        = v ; }
		void setSex      (const Sex          v)                  { data.sex       = v ; }
		void setBirthdate(const Date&       bd)                  { data.birthdate = bd; }

		void setDiagnose (const std::u16string& v)               { data.diagnose  = v ; }

		const std::string& getPatientUID() const                 { return data.uid; }
		void setPatientUID(const std::string& id)                { data.uid = id  ; }
		const std::string& getAncestry() const                   { return data.ancestry; }
		void setAncestry (const std::string& v)                  { data.ancestry  = v ; }


		      Study& getInsertId(int id)                               { return getAndInsert(id) ; }

		      Study& getStudy(int seriesId)                      { return getAndInsert(seriesId)         ; }
		const Study& getStudy(int seriesId) const                { return *(substructureMap.at(seriesId)); }

		int getInternalId() const                                      { return internalId; }

		void clear()                                              { clearSubstructure(); }

		template<typename T> void getSetParameter(T& getSet)           { getSetParameter(getSet, *this); }
		template<typename T> void getSetParameter(T& getSet)     const { getSetParameter(getSet, *this); }

		const PatientData& getPatientData() const { return data; }
		void setPatientData(const PatientData& patData) { data = patData; }

		struct PrintOptions
		{
			std::ostream& stream;

			PrintOptions(std::ostream& stream) : stream(stream) {}

			template<typename T>
			void operator()(const char* name, const T& value)
			{
				stream << std::setw(26) << name << " : " << value << '\n';
			}
		};

		void print(std::ostream& stream) const
		{
			PrintOptions printer(stream);
			this->getSetParameter(printer);
		}

	private:
		const int internalId;

		
		PatientData data;

		template<typename T, typename ParameterSet>
		static void getSetParameter(T& getSet, ParameterSet& pat)
		{
			auto& p = pat.data;
			SexEnumWrapper sexWrapper(p.sex);
			DateWrapper    birthDateWrapper(p.birthdate);

// 			getSet("internalId", p.internalId                               );
			getSet("forename"  , p.forename                                 );
			getSet("surname"   , p.surname                                  );
			getSet("title"     , p.title                                    );
			getSet("id"        , p.id                                       );
			getSet("uid"       , p.uid                                      );
			getSet("ancestry"  , p.ancestry                                 );
			getSet("birthdate" , static_cast<std::string&>(birthDateWrapper));
			getSet("sex"       , static_cast<std::string&>(sexWrapper)      );
		}

	};



}
