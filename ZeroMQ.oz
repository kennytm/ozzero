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

functor
import
    ZN at 'z14.so{native}'

export
    getVersion: GetVersion

define
    fun {GetVersion}
        Major Minor Patch
    in
        {ZN.version Major Minor Patch}
        version(major:Major  minor:Minor  patch:Patch)
    end
end

