/**
 * $LicenseInfo:firstyear=2012&license=fsviewerlgpl$
 * Phoenix Firestorm Viewer Source Code
 * Copyright (C) 2012, Nicky Dasmijn
 *
 * This library is free software; you can redistribute it and/or
 * modify it under the terms of the GNU Lesser General Public
 * License as published by the Free Software Foundation;
 * version 2.1 of the License only.
 *
 * This library is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public
 * License along with this library; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301  USA
 *
 * The Phoenix Viewer Project, Inc., 1831 Oakwood Drive, Fairmont, Minnesota 56031-3225 USA
 * http://www.phoenixviewer.com
 * $/LicenseInfo$
 */


#ifndef NDALLOCATORS_H
#define NDALLOCATORS_H


#include <new>
#include <stdlib.h>
#include <stdint.h>
#include "ndmemorypool.h"

namespace nd
{
	namespace allocators
	{
#ifdef ND_USE_ND_ALLOCS
		void *malloc( size_t aSize, size_t aAlign );
		void free( void* ptr );
		void *realloc( void *ptr, size_t aSize, size_t aAlign );
#else
		inline void *malloc( size_t aSize, size_t aAlign )
		{
			return ::malloc( aSize );
		}
		inline void free( void* ptr )
		{
			::free( ptr );
		}
		void *realloc( void *ptr, size_t aSize, size_t aAlign )
		{
			return ::realloc( ptr, aSize );
		}
#endif
	}
}

#endif
