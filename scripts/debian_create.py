#!/usr/bin/env python3

# Copyright © 2020  Stefano Marsili, <stemars@gmx.ch>
#
# This program is free software: you can redistribute it and/or modify
# it under the terms of the GNU General Public License as published by
# the Free Software Foundation, either version 3 of the License, or
# (at your option) any later version.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public
# License along with this program; if not, see <http://www.gnu.org/licenses/>

# File:   debian_create.py

# Creates debian source package, binary packages and binary dist zip.

g_sSourceProjectName = "sonorem"

# The subprojects: 
g_oSubPrjs = []
# The binaries containing shared object libraries for runtime
g_oBinLibs = []
# The binaries containing headers, documents and shared object libraries for building
g_oDevLibs = []
g_oAllLibs = g_oBinLibs + g_oDevLibs
# The binaries containing executables
g_oBinExes = ["sonorem"]
g_oAllPkgs = g_oAllLibs + g_oBinExes

import os

g_sScriptDirPath = os.path.dirname(os.path.abspath(__file__))

sTempX1X2X3 = g_sScriptDirPath + "/../share/python/frag_debian_create.py"

exec(compile(source=open(sTempX1X2X3).read(), filename=sTempX1X2X3, mode="exec"))
