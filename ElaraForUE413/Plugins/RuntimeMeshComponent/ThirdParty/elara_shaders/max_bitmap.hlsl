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

#define FILTER_PYRAMID		0
#define FILTER_SUMMED_AREA	1
#define FILTER_NONE			2

#define ALPHA_FILE			0
#define ALPHA_RGB			1
#define ALPHA_NONE			2

#define BASETYPE_UNKNOWN	0
#define BASETYPE_NONE		1
#define BASETYPE_UINT8		2
#define BASETYPE_INT8		3
#define BASETYPE_UINT16		4
#define BASETYPE_INT16		5

#define maxFLOOR(x)		((int)(x) - ((x) < 0.0))
#define maxFRAC(x)		(x - (float)maxFLOOR(x))

// START FUNCTION BODY

void max_bitmap
(
	Texture2D tex_fileName /* 0 */, 
	SamplerState tex_fileNameSampler /* 0 */, 
	in float3 tex_coords /* 0, 0, 0 */, 
	int tex_filtering /* 0 */, 
	float tex_clipu /* 0 */,
	float tex_clipv /* 0 */,
	float tex_clipw /* 1 */,
	float tex_cliph /* 1 */,
	float blur /* 0.0 */,
	int tex_apply /* 1 */,
	int tex_cropPlace /* 0 */,
	int tex_monoOutput /* 0 */,
	int tex_rgbOutput /* 0 */,
	int tex_alphaSource /* 2 */,
	int tex_preMultAlpha /* 1 */,
	out float4 result /* 0, 0, 0 ,1 */,
	out float3 result_bump /* 0, 0, 0 */
)
{
	result = float4(0, 0, 0, 1);
	result_bump = float3(0, 0, 0);
	float mu = tex_coords[0];
	float mv = tex_coords[1];
	
	if (mv >= 0.0 && mv <= 1.0 && mu >= 0.0 && mu <= 1.0)
	{
		float fu = maxFRAC(mu);
		float fv = maxFRAC(mv);
		int needtexmap = 1;
	
		if (tex_apply == 1)
		{
			if (tex_cropPlace == 1)
			{
				float u0 = tex_clipu;
				float u1 = tex_clipu + tex_clipw;

				float v0 = 1.0 - (tex_clipv + tex_cliph);
				float v1 = 1.0 - tex_clipv;
				
				if (mu < u0 || mv < v0 || mu > u1 || mv > v1)
				{
					result.rgb = float3(0, 0, 0);
					result.a = 0;
					needtexmap = 0;
				}
				
				mu = (mu - u0) / tex_clipw;
				mv = (mv - v0) / tex_cliph;
			}
			else
			{
				mu = maxFRAC(tex_clipu + fu * tex_clipw);
				mv = maxFRAC(1.0 - tex_clipv - tex_cliph + fv * tex_cliph);
			}
		}
		else
		{
			mu = fu;
			mv = fv;
		} 
	
		if (needtexmap)
		{
			float fileAlpha = 1;
			float2 coord = float2(mu, 1.0 - mv);
			result = Texture2DSample(tex_fileName, tex_fileNameSampler, coord);
			result_bump = result.xyz;
			gResult_bump = result;
			gResult_bump.w = 1.0;
			// TODO : resolve gamma issue, gamma should be input parameter
			float Gamma = 1.0f;
			if (Gamma > 0 && Gamma != 1)
			{
				result.rgb = pow(result.rgb, Gamma);
			}
		
			if (tex_alphaSource == ALPHA_NONE)
			{
				result.a = 1;
			}
			else if (tex_alphaSource == ALPHA_RGB)
			{
				result.a = intensity(result.rgb);
			}
			else if (tex_alphaSource == ALPHA_FILE)
			{
				result.a = fileAlpha;
			}
			
			if (tex_rgbOutput == 1)
			{
				result.rgb = result.a;
			}
			else
			{
				if (tex_preMultAlpha == 0)
				{
					result.rgb = result.rgb * result.a;
				}
			}
		}
	}
	else
	{
		result.a = 0;
	}
}
