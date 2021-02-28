/*************************************************************************/
/* spa - simple plugin API                                               */
/* Copyright (C) 2018                                                    */
/* Johannes Lorenz (j.git$$$lorenz-ho.me, $$$=@)                         */
/*                                                                       */
/* This program is free software; you can redistribute it and/or modify  */
/* it under the terms of the GNU General Public License as published by  */
/* the Free Software Foundation; either version 3 of the License, or (at */
/* your option) any later version.                                       */
/* This program is distributed in the hope that it will be useful, but   */
/* WITHOUT ANY WARRANTY; without even the implied warranty of            */
/* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the GNU      */
/* General Public License for more details.                              */
/*                                                                       */
/* You should have received a copy of the GNU General Public License     */
/* along with this program; if not, write to the Free Software           */
/* Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA 02110, USA  */
/*************************************************************************/

#ifndef SPA_FWD_H
#define SPA_FWD_H

//! @file forward declarations for spa.h

namespace spa
{
	class error_base;
	class port_not_found_error;
	class out_of_range_error;

	class simple_str;
	class port_ref_base;

	template<class T> class port_ref;

	template<class T> class ringbuffer;
	template<> class ringbuffer<char>;

	template<class T> class ringbuffer_in;

	template<class T> class ringbuffer_out;

	class visitor;

	class plugin;
	class descriptor;

	typedef descriptor* (*descriptor_loader_t) (unsigned long);
}

#endif // SPA_FWD_H
