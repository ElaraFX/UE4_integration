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

#ifndef EI_COLOR_H
#define EI_COLOR_H

#include <ei_util.h>

struct eiColor {

	eiScalar r, g, b;

	inline eiScalar & operator[] (unsigned int i)
	{
		return *(&r + i);
	}
	
	inline eiScalar operator[] (unsigned int i) const
	{
		return *(&r + i);
	}

	inline eiColor & operator = (eiScalar rhs)
	{
		r = rhs;
		g = rhs;
		b = rhs;
		return *this;
	}

	inline eiColor operator + (const eiColor & rhs) const
	{
		eiColor temp;
		temp.r = r + rhs.r;
		temp.g = g + rhs.g;
		temp.b = b + rhs.b;
		return temp;
	}

	inline eiColor & operator += (const eiColor & rhs)
	{
		r += rhs.r;
		g += rhs.g;
		b += rhs.b;
		return *this;
	}

	inline eiColor operator - (const eiColor & rhs) const
	{
		eiColor temp;
		temp.r = r - rhs.r;
		temp.g = g - rhs.g;
		temp.b = b - rhs.b;
		return temp;
	}

	inline eiColor & operator -= (const eiColor & rhs)
	{
		r -= rhs.r;
		g -= rhs.g;
		b -= rhs.b;
		return *this;
	}

	inline eiColor operator * (const eiColor & rhs) const
	{
		eiColor temp;
		temp.r = r * rhs.r;
		temp.g = g * rhs.g;
		temp.b = b * rhs.b;
		return temp;
	}

	inline eiColor & operator *= (const eiColor & rhs)
	{
		r *= rhs.r;
		g *= rhs.g;
		b *= rhs.b;
		return *this;
	}

	inline eiColor operator / (const eiColor & rhs) const
	{
		eiColor temp;
		temp.r = r / rhs.r;
		temp.g = g / rhs.g;
		temp.b = b / rhs.b;
		return temp;
	}

	inline eiColor & operator /= (const eiColor & rhs)
	{
		r /= rhs.r;
		g /= rhs.g;
		b /= rhs.b;
		return *this;
	}

	inline eiColor operator + (eiScalar rhs) const
	{
		eiColor temp;
		temp.r = r + rhs;
		temp.g = g + rhs;
		temp.b = b + rhs;
		return temp;
	}

	inline eiColor & operator += (eiScalar rhs)
	{
		r += rhs;
		g += rhs;
		b += rhs;
		return *this;
	}

	inline eiColor operator - (eiScalar rhs) const
	{
		eiColor temp;
		temp.r = r - rhs;
		temp.g = g - rhs;
		temp.b = b - rhs;
		return temp;
	}

	inline eiColor & operator -= (eiScalar rhs)
	{
		r -= rhs;
		g -= rhs;
		b -= rhs;
		return *this;
	}

	inline eiColor operator * (eiScalar rhs) const
	{
		eiColor temp;
		temp.r = r * rhs;
		temp.g = g * rhs;
		temp.b = b * rhs;
		return temp;
	}

	inline eiColor & operator *= (eiScalar rhs)
	{
		r *= rhs;
		g *= rhs;
		b *= rhs;
		return *this;
	}

	inline eiColor operator / (eiScalar rhs) const
	{
		eiColor temp;
		const eiScalar inv = rcpf(rhs);
		temp.r = r * inv;
		temp.g = g * inv;
		temp.b = b * inv;
		return temp;
	}

	inline eiColor & operator /= (eiScalar rhs)
	{
		const eiScalar inv = rcpf(rhs);
		r *= inv;
		g *= inv;
		b *= inv;
		return *this;
	}

	inline bool operator == (const eiColor & rhs) const
	{
		return (r == rhs.r && g == rhs.g && b == rhs.b);
	}

	inline bool operator != (const eiColor & rhs) const
	{
		return !(*this == rhs);
	}

	inline eiScalar average() const
	{
		return (r + g + b) * (1.0f / 3.0f);
	}

	inline eiScalar reduce_add() const
	{
		return (r + g + b);
	}

	// OpenSubdiv interop
	inline void Clear(void * = 0)
	{
		(*this) = 0.0f;
	}

	inline void AddWithWeight(eiColor const & src, float weight)
	{
		(*this) += src * weight;
	}
};

inline eiColor operator - (const eiColor & rhs)
{
	eiColor temp;
	temp.r = -rhs.r;
	temp.g = -rhs.g;
	temp.b = -rhs.b;
	return temp;
}

inline eiColor operator + (eiScalar lhs, const eiColor & rhs)
{
	eiColor temp;
	temp.r = lhs + rhs.r;
	temp.g = lhs + rhs.g;
	temp.b = lhs + rhs.b;
	return temp;
}

inline eiColor operator - (eiScalar lhs, const eiColor & rhs)
{
	eiColor temp;
	temp.r = lhs - rhs.r;
	temp.g = lhs - rhs.g;
	temp.b = lhs - rhs.b;
	return temp;
}

inline eiColor operator * (eiScalar lhs, const eiColor & rhs)
{
	eiColor temp;
	temp.r = lhs * rhs.r;
	temp.g = lhs * rhs.g;
	temp.b = lhs * rhs.b;
	return temp;
}

inline eiColor operator / (eiScalar lhs, const eiColor & rhs)
{
	eiColor temp;
	temp.r = lhs / rhs.r;
	temp.g = lhs / rhs.g;
	temp.b = lhs / rhs.b;
	return temp;
}

inline eiColor ei_color(eiScalar rhs)
{
	eiColor temp;
	temp.r = rhs;
	temp.g = rhs;
	temp.b = rhs;
	return temp;
}

inline eiColor ei_color(eiScalar r, eiScalar g, eiScalar b)
{
	eiColor temp;
	temp.r = r;
	temp.g = g;
	temp.b = b;
	return temp;
}

inline eiBool almost_zero(const eiColor & a, eiScalar eps)
{
	return (almost_zero(a.r, eps) && almost_zero(a.g, eps) && almost_zero(a.b, eps));
}

inline eiBool almost_equal(const eiColor & a, const eiColor & b, eiScalar eps)
{
	return (almost_equal(a.r, b.r, eps) && almost_equal(a.g, b.g, eps) && almost_equal(a.b, b.b, eps));
}

template <>
inline eiColor clamp(eiColor x, eiColor lo, eiColor hi)
{
	return ei_color(
		clamp(x.r, lo.r, hi.r), 
		clamp(x.g, lo.g, hi.g), 
		clamp(x.b, lo.b, hi.b));
}

inline void ei_gamma(eiColor & c, const eiScalar gamma)
{
	if (gamma <= 0.0f || gamma == 1.0f)
	{
		return;
	}

	const eiScalar inv_gamma = rcpf(gamma);
	c.r = powf(max(c.r, 0.0f), inv_gamma);
	c.g = powf(max(c.g, 0.0f), inv_gamma);
	c.b = powf(max(c.b, 0.0f), inv_gamma);
}

inline void ei_gamma(eiScalar & c, const eiScalar gamma)
{
	if (gamma <= 0.0f || gamma == 1.0f)
	{
		return;
	}

	const eiScalar inv_gamma = rcpf(gamma);
	c = powf(max(c, 0.0f), inv_gamma);
}

inline void ei_degamma(eiColor & c, const eiScalar gamma)
{
	if (gamma <= 0.0f || gamma == 1.0f)
	{
		return;
	}

	c.r = powf(max(c.r, 0.0f), gamma);
	c.g = powf(max(c.g, 0.0f), gamma);
	c.b = powf(max(c.b, 0.0f), gamma);
}

inline void ei_degamma(eiScalar & c, const eiScalar gamma)
{
	if (gamma <= 0.0f || gamma == 1.0f)
	{
		return;
	}

	c = powf(max(c, 0.0f), gamma);
}

/* OSL interoperability */
#if defined eiCORE_EXPORTS || defined EI_OSL_INTEROP

inline OSL::Color3 to_col3(const eiColor & v)
{
	return OSL::Color3(v.r, v.g, v.b);
}

inline void to_col3(OSL::Color3 & c, const eiColor & v)
{
	c.x = v.r;
	c.y = v.g;
	c.z = v.b;
}

inline eiColor from_col3(const OSL::Color3 & c)
{
	return ei_color(c.x, c.y, c.z);
}

inline void from_col3(eiColor & v, const OSL::Color3 & c)
{
	v.r = c.x;
	v.g = c.y;
	v.b = c.z;
}

#endif

#endif
