/*
    This file is part of ozzero.

    ozzero is free software: you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation, either version 3 of the License, or
    (at your option) any later version.

    ozzero is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Foobar.  If not, see <http://www.gnu.org/licenses/>.
*/

// z14.c -- ZeroMQ Oz bridge for Mozart/Oz 1.4

#include <mozart.h>
#include <zmq.h>

#if __cplusplus
extern "C" {
#endif

OZ_BI_define(ozzero_version, 0, 3)
{
    int major, minor, patch;
    zmq_version(&major, &minor, &patch);
    return
	(OZ_out(0) = OZ_int(major)) &&
	(OZ_out(1) = OZ_int(minor)) &&
	(OZ_out(2) = OZ_int(patch));
}
OZ_BI_end

//------------------------------------------------------------------------------

OZ_C_proc_interface* oz_init_module(void)
{
    static OZ_C_proc_interface interfaces[] = {
        {"version", 0, 3, ozzero_version},
        {NULL}
    };
    return interfaces;
}

#if __cplusplus
}
#endif

