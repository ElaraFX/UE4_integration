/**************************************************************************
 * Copyright (C) 2015 Rendease Co., Ltd.
 * All rights reserved.
 *
 * This program is commercial software: you must not redistribute it 
 * and/or modify it without written permission from Rendease Co., Ltd.
 *
 * This program is distributed in the hope that it will be useful, 
 * but WITHOUT ANY WARRANTY; without even the implied warranty of 
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the 
 * End User License Agreement for more details.
 *
 * You should have received a copy of the End User License Agreement along 
 * with this program.  If not, see <http://www.rendease.com/licensing/>
 *************************************************************************/

#ifndef EI_DATAFLOWX_H
#define EI_DATAFLOWX_H

/** C++ interfaces of dataflow system.
 * \file ei_dataflowx.h
 * \author Elvic Liang
 */

#include <ei_dataflow.h>

/** The accessor for data items stored in database.
 */
template <typename T>
class eiDataAccessor
{
public:
	inline eiDataAccessor(const eiTag tag)
	{
		m_tag = tag;
		m_ptr = (T *)ei_db_access(tag);
	}

	inline ~eiDataAccessor()
	{
		ei_db_end(m_tag);
	}

	inline T *operator -> () const
	{
		return m_ptr;
	}

	inline T *get() const
	{
		return m_ptr;
	}

private:
	eiTag			m_tag;
	T				*m_ptr;
};

/** The deferred accessor which will defer the data 
 * initialization to the next time of access.
 */
template <typename T>
class eiDeferDataAccessor
{
public:
	inline eiDeferDataAccessor(const eiTag tag)
	{
		m_tag = tag;
		m_ptr = (T *)ei_db_access_defer(tag);
	}

	inline ~eiDeferDataAccessor()
	{
		ei_db_end_defer(m_tag);
	}

	inline T *operator -> () const
	{
		return m_ptr;
	}

	inline T *get() const
	{
		return m_ptr;
	}

private:
	eiTag			m_tag;
	T				*m_ptr;
};

#endif
