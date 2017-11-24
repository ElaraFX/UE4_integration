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

#define MAPSLOT_TEXTURE		0
#define MAPSLOT_ENV			1

#define UVWSRC_EXPLICIT		0
#define UVWSRC_OBJECT_XYZ	1
#define UVWSRC_VERTEX_COLOR	2
#define UVWSRC_WORLD_XYZ	3

#define ENV_SPHERICAL		1
#define ENV_CYLINDRICAL		2
#define ENV_SHRINK			3
#define ENV_SCREEN			4

#define AXIS_UV		0
#define AXIS_VW		1
#define AXIS_WU		2
#define M_PI 3.14159265f

float frac(float a)
{
	return a - floor(a);
}

float mirror(float x)
{
	return (x < 0.5) ? 2.0 * x : (1.0 - 2.0 * (x - 0.5));
}

float4x4 rotate_x(float angle)
{
	float sine = sin(angle);
	float cosine = cos(angle);

	return float4x4(
		1, 0, 0, 0, 
		0, cosine, sine, 0, 
		0, -sine, cosine, 0, 
		0, 0, 0, 1);
}

float4x4 rotate_y(float angle)
{
	float sine = sin(angle);
	float cosine = cos(angle);

	return float4x4(
		cosine, 0, -sine, 0, 
		0, 1, 0, 0, 
		sine, 0, cosine, 0, 
		0, 0, 0, 1);
}

float4x4 rotate_z(float angle)
{
	float sine = sin(angle);
	float cosine = cos(angle);

	return float4x4(
		cosine, sine, 0, 0, 
		-sine, cosine, 0, 0, 
		0, 0, 1, 0, 
		0, 0, 0, 1);
}

// START FUNCTION BODY

void max_stduv
(
	int slotType /* 0 */, 
	int coordMapping /* 0 */, 
	int mapChannel /* 0 */, 
	int uvwSource /* 0 */, 
	int hideMapBack /* 0 */, 
	float uOffset /* 0 */, 
	float uScale /* 0 */, 
	int uWrap /* 0 */, 
	int uMirror /* 0 */, 
	float vOffset /* 0 */, 
	float vScale /* 0 */, 
	int vWrap /* 0 */, 
	int vMirror /* 0 */, 
	float uAngle /* 0 */, 
	float vAngle /* 0 */, 
	float wAngle /* 0 */, 
	int axis /* 0 */, 
	int clip /* 0 */, 
	float blur /* 0 */, 
	float blurOffset /* 0 */, 
	int uvNoise /* 0 */, 
	int uvNoiseAnimate /* 0 */, 
	float uvNoiseAmount /* 0 */, 
	float uvNoiseSize /* 1 */, 
	int uvNoiseLevel /* 1 */, 
	float uvNoisePhase /* 0 */, 
	int realWorldScale /* 0 */, 
	out float3 result /* 0, 0, 0 */
)
{
	float3 uv = float3(0, 0, 0);
	
	if (slotType == MAPSLOT_TEXTURE)
	{
		if (uvwSource == UVWSRC_EXPLICIT || uvwSource == UVWSRC_VERTEX_COLOR)
		{
			switch (mapChannel)
			{
			case 0: uv.xy = uv0; break;
			case 1: uv.xy = uv1; break;
			}
		}
	}
	else
	{
		float3 ray_dir = I;

		if (coordMapping == ENV_SPHERICAL)
		{
			uv[0] = 0.5 + atan2(ray_dir[0], -ray_dir[1]) * (1.0 / (2.0 * M_PI));
			uv[1] = 0.5 + asin(ray_dir[2]) * (1.0 / M_PI);
			uv[2] = 0.0;
		}
		else if (coordMapping == ENV_CYLINDRICAL)
		{
			uv[0] = 0.5 + atan2(ray_dir[0], -ray_dir[1]) * (1.0 / (2.0 * M_PI));
			float r = sqrt(ray_dir[0] * ray_dir[0] + ray_dir[1] * ray_dir[1]);
			if (abs(ray_dir[2]) > r)
			{
				uv[1] = (ray_dir[2] > 0.0) ? 1.0 - 0.25 * (r / ray_dir[2]) : -0.25 * (r / ray_dir[2]);
			}
			else
			{
				uv[1] = 0.25 * (2.0 + ray_dir[2] / r);
			}

			uv[2] = 0.0;
		}
		else if (coordMapping == ENV_SHRINK)
		{
			float a1 = atan2(ray_dir[0], -ray_dir[1]);
			float a2 = asin(ray_dir[2]);
			float r = 0.5 - (a2 + M_PI * 0.5) * (1.0 / (2.0 * M_PI));
			uv[0] = 0.5 + r * cos(a1);
			uv[1] = 0.5 + r * sin(a1);
			uv[2] = 0.0;
		}
		else if (coordMapping == ENV_SCREEN)
		{
			// TODO : ENV_SCREEN
		}
	}

	float centerOffset = (realWorldScale) ? 0.0 : 0.5;

	uv[0] -= uOffset + centerOffset;
	uv[1] -= vOffset + centerOffset;
	uv[2] -= centerOffset;
	
	if (uAngle != 0 || vAngle != 0 || wAngle != 0)
	{
		matrix xform = 1;
		
		if (slotType == MAPSLOT_TEXTURE)
		{
			if (uAngle != 0)
			{
				xform *= rotate_x(uAngle);
			}
			if (vAngle != 0)
			{
				xform *= rotate_y(vAngle);
			}
			if (wAngle != 0)
			{
				xform *= rotate_z(wAngle);
			}
		}
		else
		{
			float sine = sin(wAngle);
			float cosine = cos(wAngle);

			xform = float4x4(
				cosine, -sine, 0, 0, 
				sine, cosine, 0, 0, 
				0, 0, 1, 0, 
				0, 0, 0, 1);
		}
		
		// must use vector semantics here
		float3 in_vec = uv;
		float3 out_vec = mul(float4(in_vec, 1.0f), xform).xyz;;
		uv = out_vec;
	}

	if (slotType == MAPSLOT_TEXTURE)
	{
		if (axis == AXIS_VW)
		{
			float tmp = uv[0];
			uv[0] = uv[1];
			uv[1] = uv[2];
			uv[2] = tmp;
		}
		else if (axis == AXIS_WU)
		{
			float tmp = uv[1];
			uv[1] = uv[0];
			uv[0] = uv[2];
			uv[2] = tmp;
		}
	}

	uv[0] *= uScale;
	uv[1] *= vScale;

	uv[0] += centerOffset;
	uv[1] += centerOffset;
	uv[2] += centerOffset;

	float ufrac = frac(uv[0]);
	if (uWrap)
	{
		uv[0] = ufrac;
	}
	else if (uMirror)
	{
		uv[0] = mirror(ufrac);
	}

	float vfrac = frac(uv[1]);
	if (vWrap)
	{
		uv[1] = vfrac;
	}
	else if (vMirror)
	{
		uv[1] = mirror(vfrac);
	}

	if (!clip)
	{
		float ufloor = uv[0] - ufrac;
		float vfloor = uv[1] - vfrac;
		uv[0] += ufloor;
		uv[1] += vfloor;
	}

	result = uv;
}
