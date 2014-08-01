/*  Copyright 2014 Aaron Boxer

    This program is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    This program is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with this program.  If not, see <http://www.gnu.org/licenses/>. */

#pragma once

#define CONSTANT constant
#define KERNEL kernel
#define LOCAL local
#define GLOBAL global


size_t getGlobalId(	const uint dimindx) {
  return get_global_id(dimindx);
}
size_t getGroupId(	const uint dimindx) {
  return get_group_id(dimindx);
}
size_t getLocalId(	const uint dimindx) {
  return get_local_id(dimindx);
}

inline void localMemoryFence() {
	barrier(CLK_LOCAL_MEM_FENCE);
}

