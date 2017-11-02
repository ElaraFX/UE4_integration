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
 
 float intensity(float3 color)
 {
	return (color.x + color.y + color.z) / 3;
 }

// START FUNCTION BODY

void max_stdout
(
	float rgbAmount /* 1.0 */,
	float rgbOffset /* 0.0 */,
	float outputAmount  /* 1.0 */,
	float bumpAmount /* 1.0*/,
	int invert /* 0 */,
	int clamp0 /* 0*/,
	int alphaFromRGB /* 0*/,
	int useColorMap /* 1*/,
	int useRGBCurve /* 0*/,
	in float3 stdout_color /* 0.0, 0.0, 0*/ ,
	float tex_alpha_stdout_color /* 1 */ ,
	in float3 stdout_bump /* 0, 0, 0 */,
	out float4 result /* 0, 0, 0 */,
	out float3 result_bump /* 0, 0, 0 */
)
{
	float3 outColor = stdout_color;
	float outAlpha = tex_alpha_stdout_color;

	if (rgbAmount != 1.0)
	{
		outColor *= rgbAmount;
	}
	if (rgbOffset != 0.0)
	{
		outColor += rgbOffset * outAlpha;
	}
	if (outputAmount != 1.0)
	{
		outColor *= outputAmount;
		outAlpha *= outputAmount;
	}
	if (invert)
	{
		outColor = 1.0 - outColor;
	}
	if (clamp0)
	{
		outColor = clamp(outColor, float3(0, 0, 0), float3(1, 1, 1));
	}
	if (alphaFromRGB)
	{
        outAlpha = intensity(outColor);
	}
	outAlpha = clamp(outAlpha, 0, 1);
	result.rgb = outColor;
	result.a = outAlpha;
	
	// EvalNormal
	{
		result_bump = stdout_bump;
		if (bumpAmount != 1.0)
		{
			result_bump *= bumpAmount;
			gResult_bump.xyz *= bumpAmount;
		}
		if (invert)
		{
			result_bump = -result_bump;
			gResult_bump.xyz = -gResult_bump.xyz;
		}
	}
}
