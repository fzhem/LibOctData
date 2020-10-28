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

#include "filestreamdircet.h"

#include<iomanip>
#include<exception>
#include<filesystem>

#include<boost/log/trivial.hpp>

#include<import/platform_helper.h>

namespace bfs = std::filesystem;

namespace OctData
{

	FileStreamDircet::FileStreamDircet(const std::filesystem::path& filepath)
	{
		stream.open(filepath, std::ios::binary | std::ios::in);
		if(!stream.good())
		{
			BOOST_LOG_TRIVIAL(error) << "Can't open file stream " << filepath;
			throw("Can't open file stream"); // TODO
		}
	}
}
