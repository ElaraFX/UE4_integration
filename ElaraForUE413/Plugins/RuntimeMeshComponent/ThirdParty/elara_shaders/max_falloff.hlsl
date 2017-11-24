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

#define FALLDIR_VIEW 0
#define FALLDIR_XCAM 1
#define FALLDIR_YCAM 2
#define FALLDIR_OBJ  3
#define FALLDIR_XLOC 4
#define FALLDIR_YLOC 5
#define FALLDIR_ZLOC 6
#define FALLDIR_XWOR 7
#define FALLDIR_YWOR 8
#define FALLDIR_ZWOR 9
 
#define FALLTYPE_TOWRDS_AWY 0
#define FALLTYPE_PERP_PARA  1
#define FALLTYPE_FRESNEL    2
#define FALLTYPE_DIST_BLEND 4

struct Parameter{
 	int type;
	int direction;
	int mtlIOROverride;
	float ior;
	int extrapolateOn;
	float nearDistance;
	float farDistance;
	float3 nodePos;
	int self;
};

float Fresnel(float3 ReflectVector,float3 ViewRay, float ior )
{
	float3 VR;
	ReflectVector = -ReflectVector;
	VR = ReflectVector + ViewRay;
	VR = normalize(VR);
	float c = dot ( ViewRay, VR );


	float g = (ior * ior + c*c - 1.0);
	if (g < 0.0) {
		g = 0.0;
	} else {
		g = (float)sqrt(g);
	}

    if(g != -c) {
        c = (((g-c)*(g-c))/(2.0*(g+c)*(g+c))) * (1.0 + (((c*(g+c)-1.0)*(c*(g+c)-1.0))/((c*(g+c)+1.0)*(c*(g+c)+1.0))));
        return c;
    }
    else {
        return 0;
    }
}

float FalloffFunc(Parameter param, float3 I, float3 N, float3 P)
{
	float d = 0.0, l = 0.0;
	float3 v,n,p;
	float4x4 tm = ResolvedView.CameraViewToTranslatedWorld;
	float4x4 m = ResolvedView.TranslatedWorldToCameraView;
	{
		float3 nVector = normalize(float3(m[0][0],m[0][1],m[0][2]));
		m[0][0] = nVector[0];m[0][1] = nVector[1];m[0][2] = nVector[2];
		nVector = normalize(float3(m[1][0],m[1][1],m[1][2]));
		m[1][0] = nVector[0];m[1][1] = nVector[1];m[1][2] = nVector[2];
		nVector = normalize(float3(m[2][0],m[2][1],m[2][2]));
		m[2][0] = nVector[0];m[2][1] = nVector[1];m[2][2] = nVector[2];
	}
	
	if(param.direction == FALLDIR_VIEW){
		v = mul(I, (float3x3)m);
		n = mul(N, (float3x3)m);
		d = -dot(v, n);
		p = mul(float4(P, 1.0), m).xyz;
		l = length(p);
	}else if(param.direction == FALLDIR_XCAM){
		v = float3(-1, 0, 0);
		n = mul(float4(N, 1.0), m);
		d = - dot(n, v);
		p = mul(float4(P, 1.0), m).xyz;
		l = p[0];
	}else if(param.direction == FALLDIR_YCAM){
		v = float3( 0, -1, 0);
		n = mul(float4(N, 1.0), m);
		d = - dot(n,v);
		p = mul(float4(P, 1.0), m).xyz;
		l = p[1];
	}else if(param.direction == FALLDIR_OBJ){
		float3	p;

		n = mul(N, (float3x3)m);
		if (param.self)
		{
			v = float3(-1, -1, -1);
		}
		else
		{
			float3 op;
			op = param.nodePos + ResolvedView.PreViewTranslation;
			op = mul(float4(op, 1.0), m).xyz;
			p = mul(float4(P, 1.0), m).xyz;
			
			v = op - p;
		}
		l = length(v);
		v = normalize(v);

		d = dot(n, v);
	}else if(param.direction == FALLDIR_XLOC){
		v = float3(1, 0, 0);
		v = mul(v, (float3x3)(ResolvedView.CameraViewToTranslatedWorld));
		v = mul(v, (float3x3)(Primitive_WorldToLocal));
		n = mul(N, (float3x3)(Primitive_WorldToLocal));
		d = n[0];
		l = Primitive_LocalToWorld[0][0];
	}else if(param.direction == FALLDIR_YLOC){
		v = float3(0, 1, 0);
		v = mul(v, (float3x3)(ResolvedView.CameraViewToTranslatedWorld));
		v = mul(v, (float3x3)(Primitive_WorldToLocal));
		n = mul(N, (float3x3)(Primitive_WorldToLocal));
		d = n[1];
		l = Primitive_LocalToWorld[0][1];
	}else if(param.direction == FALLDIR_ZLOC){
		v = float3(0, 0, 1);
		v = mul(v, (float3x3)(ResolvedView.CameraViewToTranslatedWorld));
		v = mul(v, (float3x3)(Primitive_WorldToLocal));
		n = mul(N, (float3x3)(Primitive_WorldToLocal));
		d = n[2];
		l = Primitive_LocalToWorld[0][2];
	}else if(param.direction == FALLDIR_XWOR){
		v = float3(1, 0, 0);
		v = mul(v, (float3x3)(ResolvedView.CameraViewToTranslatedWorld));
		n = N;
		d = n[0];
		l = tm[0][0];
	}else if(param.direction == FALLDIR_YWOR){
		v = float3(0, 1, 0);
		v = mul(v, (float3x3)(ResolvedView.CameraViewToTranslatedWorld));
		n = N;
		d = n[1];
		l = tm[0][1];
	}else if(param.direction == FALLDIR_ZWOR){
		v = float3(0, 0, 1);
		v = mul(v, (float3x3)(ResolvedView.CameraViewToTranslatedWorld));
		n = N;
		d = n[2];
		l = tm[0][2];
	}
	
	if(param.type == FALLTYPE_TOWRDS_AWY){
		d = 1.0-0.5*(d+1.0);
	}else if(param.type == FALLTYPE_PERP_PARA){
		d = 1.0-(float)abs(d);
	}else if(param.type == FALLTYPE_FRESNEL){
		float3		R;
		float		VdotN;
		n = mul(N, (float3x3)(ResolvedView.TranslatedWorldToCameraView));
		VdotN = dot(n, v);
		
		R = - (2.0 * VdotN * n - v);
		if ( param.mtlIOROverride == 1 )
			d = Fresnel(R, v, param.ior);
		else{
			float ior = param.ior;
			if(ior == 0) ior = 1.0;
            d = Fresnel(R, v, ior);
        }
	}else if(param.type == FALLTYPE_DIST_BLEND){
		if ( !param.extrapolateOn && l <= param.nearDistance )
		{
			d = 1.0;
		}
		else
		if ( !param.extrapolateOn && l > param.farDistance )
		{
			d = 0.0;
		}
		else
		{
			d = (param.farDistance - param.nearDistance) ? ((param.farDistance - l) / (param.farDistance - param.nearDistance)) : 10000.0;
		}
	}

	return d;
}

// START FUNCTION BODY

void max_falloff
(
	float3 color1 /* 0, 0, 0 */,
	in float3 map1 /* 0, 0, 0 */,
	float tex_alpha_map1 /* 1 */,
	in float3 map1_bump /* 0, 0, 0 */,
	int map1On /* 1 */,
	float map1Amount /* 100 */,
	float3 color2 /* 1, 1, 1 */,
	in float3 map2 /* 0, 0, 0 */,
	float tex_alpha_map2 /* 1 */,
	in float3 map2_bump /* 0, 0, 0 */,
	int map2On /* 1 */,
	float map2Amount /* 100 */,
	int type /* 1 */,
	int direction /* 3 */,
	int mtlIOROverride /* 1 */,
	float ior /* 1.6 */,
	int extrapolateOn /* 0 */,
	float nearDistance /* 0 */,
	float farDistance /* 100 */,
	float3 nodePos /* 0, 0, 0 */,
	int self /* 1 */,
	out float4 result /* 0, 0, 0, 1 */,
	out float3 result_bump /* 0, 0, 0 */
)
{
	float		f;
	float3		c1,c2;
	float		a1,a2;
	float3		n1,n2;
	if(map1On == 1 && map1Amount != 0.0){
		c1 = color1 + map1Amount * 0.01 * (map1 - color1);
		a1 = 1 + map1Amount * 0.01 * (tex_alpha_map1 - 1);
		n1 = 1 + map1Amount * 0.01 * (map1_bump - 1);
	}else{
		c1 = color1;
		a1 = 1;
		n1 = 0;
	}
	
	if(map2On == 1 && map2Amount != 0.0){
		c2 = color2 + map2Amount * 0.01 * (map2 - color2);
		a2 = 1 + map2Amount * 0.01 * (tex_alpha_map2 - 1);
		n2 = 1 + map2Amount * 0.01 * (map2_bump - 1);
	}else{
		c2 = color2;
		a2 = 1;
		n2 = 0;
	}
	Parameter param;
	param.type = type;
	param.direction = direction;
	param.mtlIOROverride = mtlIOROverride;
	param.ior = ior;
	param.extrapolateOn = extrapolateOn;
	param.nearDistance = nearDistance;
	param.farDistance = farDistance;
	param.nodePos = nodePos;
	param.self = self;

	f = FalloffFunc(param, I, N, GetTranslatedWorldPosition(Parameters));
	
	result.xyz = c1 + f * ( c2 - c1);
	result.w = a1 + f * ( a2 - a1 );
	result_bump = n1 + f * ( n2 - n1 );
}
