/*
 * Copyright © 2020  Stefano Marsili, <stemars@gmx.ch>
 *
 * This program is free software: you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public
 * License along with this program; if not, see <http://www.gnu.org/licenses/>
 */
/*
 * File:   config.h
 */

#ifndef SONO_CONFIG_H
#define SONO_CONFIG_H

#include <string>

namespace sono
{

namespace Config
{

const std::string& getVersionString() noexcept;

const std::string& getDataDir() noexcept;

} // namespace Config

} // namespace sono

#endif /* SONO_CONFIG_H */

