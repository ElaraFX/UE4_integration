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

#define BRDF_PHONG	0
#define BRDF_BLINN	1
#define BRDF_WARD	2

bool backfacing(float3 I, float3 N)
{
	return dot(I, N) >= 0;
}

float fresnel_schlick(float cosi, float eta, out float R0)
{
	// special case: ignore fresnel
	if (eta <= 0.0)
		return 1.0;

	if (cosi < 0.0)
	{
		eta = 1.0 / eta;
		cosi = -cosi;
	}

	float Rn = (1.0f - eta) / (1.0f + eta);
	R0 = Rn * Rn;
	float F1 = 1.0f - cosi;
	float F2 = F1 * F1;
	float F5 = F2 * F2 * F1;

	return clamp(R0 + (1.0f - R0) * F5, 0.0f, 1.0f);
}

float IsRefract(float3 direction, float3 normal, float refract_ior) 
{
	bool bFrontFace=(dot(normal, direction)<0.0f); 
	float ior=(bFrontFace? 1.0f/refract_ior : refract_ior); 
	float3 _normal=(bFrontFace? normal : -normal);
	float cosIncident=-dot(direction, _normal); 
	return 1.0-ior*ior*(1.0-cosIncident*cosIncident);
}

// See http://www.rorydriscoll.com/2012/01/11/derivative-maps/
// http://jbit.net/~sparky/sfgrad_bump/mm_sfgrad_bump.pdf for details
float3 PerturbNormalLQ(float3 surf_pos , float3 surf_norm , float height, float amount) 
{ 
	float3 vSigmaS = ddx( surf_pos ); 
	float3 vSigmaT = ddy( surf_pos ); 
	float3 vN = surf_norm; // normalized
	float3 vR1 = cross(vSigmaT ,vN); 
	float3 vR2 = cross(vN,vSigmaS);
	float fDet = dot(vSigmaS , vR1); 
	float dBs = ddx_fine( height ); 
	float dBt = ddy_fine( height );
	float3 vSurfGrad = sign(fDet) * ( dBs * vR1 + dBt * vR2 ); 
	return normalize(abs(fDet)*vN- 0.3 * vSurfGrad);
}

// START FUNCTION BODY

void max_vray_mtl
(
	float3 diffuse_color /* 0, 0, 0 */, 
	float diffuse_roughness /* 0 */, 
	float3 selfIllumination /* 0, 0, 0 */, 
	int selfIllumination_gi /* 0 */, 
	float selfIllumination_multiplier /* 1 */, 
	float3 reflection_color /* 0, 0, 0 */, 
	float reflection_glossiness /* 0 */, 
	float hilight_glossiness /* 0 */, 
	int reflection_subdivs /* 0 */, 
	int reflection_fresnel /* 0 */, 
	int reflection_maxDepth /* 0 */, 
	float3 reflection_exitColor /* 0, 0, 0 */, 
	int reflection_useInterpolation /* 0 */, 
	float reflection_ior /* 0 */, 
	int reflection_lockGlossiness /* 0 */, 
	int reflection_lockIOR /* 0 */, 
	float reflection_dimDistance /* 0 */, 
	int reflection_dimDistance_on /* 0 */, 
	float reflection_dimDistance_falloff /* 0 */, 
	int reflection_affectAlpha /* 0 */, 
	float3 refraction_color /* 0, 0, 0 */, 
	float refraction_glossiness /* 0 */, 
	int refraction_subdivs /* 0 */, 
	float refraction_ior /* 0 */, 
	float3 refraction_fogColor /* 0, 0, 0 */, 
	float refraction_fogMult /* 1 */, 
	float refraction_fogBias /* 0 */, 
	int refraction_affectShadows /* 0 */, 
	int refraction_affectAlpha /* 0 */, 
	int refraction_maxDepth /* 0 */, 
	float3 refraction_exitColor /* 0, 0, 0 */, 
	int refraction_useExitColor /* 0 */, 
	int refraction_useInterpolation /* 0 */, 
	float refraction_dispersion /* 0.0 */, 
	int refraction_dispersion_on /* 0 */, 
	int translucency_on /* 0 */, 
	float translucency_thickness /* 0 */, 
	float translucency_scatterCoeff /* 0 */, 
	float translucency_fbCoeff /* 0 */, 
	float translucency_multiplier /* 1 */, 
	float3 translucency_color /* 0, 0, 0 */, 
	int brdf_type /* 0 */, 
	float anisotropy /* 0 */, 
	float anisotropy_rotation /* 0 */, 
	int anisotropy_derivation /* 0 */, 
	int anisotropy_axis /* 0 */, 
	int anisotropy_channel /* 0 */, 
	float soften /* 0 */, 
	int refraction_fogUnitsScale_on /* 0 */, 
	float option_cutOff /* 0 */, 
	int preservationMode /* 0 */, 
	in float3 texmap_diffuse /* 0, 0, 0 */, 
	int texmap_diffuse_on /* 0 */, 
	int texmap_diffuse_isnull /* 0 */, 
	float tex_alpha_texmap_diffuse /* 1 */, 
	float texmap_diffuse_multiplier /* 1 */, 
	in float3 texmap_reflection /* 0, 0, 0 */, 
	int texmap_reflection_on /* 0 */, 
	int texmap_reflection_isnull /* 0 */, 
	float tex_alpha_texmap_reflection /* 1 */, 
	float texmap_reflection_multiplier /* 1 */, 
	in float3 texmap_refraction /* 0, 0, 0 */, 
	int texmap_refraction_on /* 0 */, 
	int texmap_refraction_isnull /* 0 */, 
	float tex_alpha_texmap_refraction /* 1 */, 
	float texmap_refraction_multiplier /* 1 */, 
	in float3 texmap_bump /* 0, 0, 0 */, 
	int texmap_bump_on /* 0 */, 
	int texmap_bump_isnull /* 0 */, 
	float tex_alpha_texmap_bump /* 1 */, 
	float texmap_bump_multiplier /* 1 */,
	in float3 tex_bump /* 0, 0, 0 */,
	in float3 texmap_reflectionGlossiness /* 0, 0, 0 */, 
	int texmap_reflectionGlossiness_on /* 0 */, 
	int texmap_reflectionGlossiness_isnull /* 0 */, 
	float tex_alpha_texmap_reflectionGlossiness /* 1 */, 
	float texmap_reflectionGlossiness_multiplier /* 1 */, 
	in float3 texmap_refractionGlossiness /* 0, 0, 0 */, 
	int texmap_refractionGlossiness_on /* 0 */, 
	int texmap_refractionGlossiness_isnull /* 0 */, 
	float tex_alpha_texmap_refractionGlossiness /* 1 */, 
	float texmap_refractionGlossiness_multiplier /* 1 */, 
	in float3 texmap_refractionIOR /* 0, 0, 0 */, 
	int texmap_refractionIOR_on /* 0 */, 
	int texmap_refractionIOR_isnull /* 0 */, 
	float tex_alpha_texmap_refractionIOR /* 1 */, 
	float texmap_refractionIOR_multiplier /* 1 */, 
	in float3 texmap_displacement /* 0, 0, 0 */, 
	int texmap_displacement_on /* 0 */, 
	int texmap_displacement_isnull /* 0 */, 
	float tex_alpha_texmap_displacement /* 1 */, 
	float texmap_displacement_multiplier /* 1 */, 
	in float3 texmap_translucent /* 0, 0, 0 */, 
	int texmap_translucent_on /* 0 */, 
	int texmap_translucent_isnull /* 0 */, 
	float tex_alpha_texmap_translucent /* 1 */, 
	float texmap_translucent_multiplier /* 1 */, 
	in float3 texmap_hilightGlossiness /* 0, 0, 0 */, 
	int texmap_hilightGlossiness_on /* 0 */, 
	int texmap_hilightGlossiness_isnull /* 0 */, 
	float tex_alpha_texmap_hilightGlossiness /* 1 */, 
	float texmap_hilightGlossiness_multiplier /* 1 */, 
	in float3 texmap_reflectionIOR /* 0, 0, 0 */, 
	int texmap_reflectionIOR_on /* 0 */, 
	int texmap_reflectionIOR_isnull /* 0 */, 
	float tex_alpha_texmap_reflectionIOR /* 1 */, 
	float texmap_reflectionIOR_multiplier /* 1 */, 
	in float3 texmap_opacity /* 0, 0, 0 */, 
	int texmap_opacity_on /* 0 */, 
	int texmap_opacity_isnull /* 0 */, 
	float tex_alpha_texmap_opacity /* 1 */, 
	float texmap_opacity_multiplier /* 1 */, 
	in float3 texmap_roughness /* 0, 0, 0 */, 
	int texmap_roughness_on /* 0 */, 
	int texmap_roughness_isnull /* 0 */, 
	float tex_alpha_texmap_roughness /* 1 */, 
	float texmap_roughness_multiplier /* 1 */, 
	in float3 texmap_anisotropy /* 0, 0, 0 */, 
	int texmap_anisotropy_on /* 0 */, 
	int texmap_anisotropy_isnull /* 0 */, 
	float tex_alpha_texmap_anisotropy /* 1 */, 
	float texmap_anisotropy_multiplier /* 1 */, 
	in float3 texmap_anisotropy_rotation /* 0, 0, 0 */, 
	int texmap_anisotropy_rotation_on /* 0 */, 
	int texmap_anisotropy_rotation_isnull /* 0 */, 
	float tex_alpha_texmap_anisotropy_rotation /* 1 */, 
	float texmap_anisotropy_rotation_multiplier /* 1 */, 
	in float3 texmap_refraction_fog /* 0, 0, 0 */, 
	int texmap_refraction_fog_on /* 0 */, 
	int texmap_refraction_fog_isnull /* 0 */, 
	float tex_alpha_texmap_refraction_fog /* 1 */, 
	float texmap_refraction_fog_multiplier /* 1 */, 
	in float3 texmap_self_illumination /* 0, 0, 0 */, 
	int texmap_self_illumination_on /* 0 */, 
	int texmap_self_illumination_isnull /* 0 */, 
	float tex_alpha_texmap_self_illumination /* 1 */, 
	float texmap_self_illumination_multiplier /* 1 */, 
	out float4 output0 /* 0 */,
	out float4 output1 /* 0 */,
	out float4 output2 /* 0 */
)
{
	float3 final_refraction = refraction_color;
	if (texmap_refraction_on && !texmap_refraction_isnull)
	{
		float texmap_refraction_amount = texmap_refraction_multiplier * 0.01;
		final_refraction = (1 - tex_alpha_texmap_refraction * texmap_refraction_amount) * refraction_color + texmap_refraction_amount * texmap_refraction;
	}

	int no_bump = 0;
	float3 Nshading = N * View_CullingSign * Primitive_LocalToWorldDeterminantSign;
	if (!no_bump && texmap_bump_on && !texmap_bump_isnull)
	{
		float height = gResult_bump.w > 0 ? gResult_bump.r : intensity(tex_bump);
		float3 P = GetWorldPosition(Parameters);
		// To fix the artifects appeared in the edge of triangles, try using the following statements
		// float height = Texture2DSample(e0tex_fileName0, e0tex_fileName0Sampler, float2(uv0.x, 1 - uv0.y)).r;
		Nshading = PerturbNormalLQ(P, N, height, texmap_bump_multiplier * 0.01);
	}

	float final_refraction_ior = refraction_ior;
	if (texmap_refractionIOR_on && !texmap_refractionIOR_isnull)
	{
		float texmap_refractionIOR_amount = texmap_refractionIOR_multiplier * 0.01;
	
		final_refraction_ior = 
			(1 - tex_alpha_texmap_refractionIOR * texmap_refractionIOR_amount) * refraction_ior 
			+ texmap_refractionIOR_amount * intensity(texmap_refractionIOR);
	}

	float final_reflection_ior = reflection_ior;
	if (reflection_lockIOR != 0)
	{
		final_reflection_ior = final_refraction_ior;
	}
	else
	{
		if (texmap_reflectionIOR_on && !texmap_reflectionIOR_isnull)
		{
			float texmap_reflectionIOR_amount = texmap_reflectionIOR_multiplier * 0.01;
	
			final_reflection_ior = 
				(1 - tex_alpha_texmap_reflectionIOR * texmap_reflectionIOR_amount) * reflection_ior 
				+ texmap_reflectionIOR_amount * intensity(texmap_reflectionIOR);
		}
	}

	float refraction_eta = final_refraction_ior;
	float reflection_eta = final_reflection_ior;
	if (backfacing(I, Nshading))
	{
		refraction_eta = 1 / final_refraction_ior;
		reflection_eta = 1 / final_reflection_ior;
	}
	
	float3 final_reflection = reflection_color;
	if (texmap_reflection_on && !texmap_reflection_isnull)
	{
		float texmap_reflection_amount = texmap_reflection_multiplier * 0.01;
	
		final_reflection = (1 - tex_alpha_texmap_reflection * texmap_reflection_amount) * reflection_color + texmap_reflection_amount * texmap_reflection;
	}
	
	float Fr = 1.0;
	float R0 = 1.0;
	if (reflection_fresnel)
	{
		Fr = fresnel_schlick(dot(Nshading, -I), reflection_eta, R0);
	}

	float3 final_diffuse = diffuse_color;
	if (texmap_diffuse_on && !texmap_diffuse_isnull)
	{
		float texmap_diffuse_amount = texmap_diffuse_multiplier * 0.01;
		final_diffuse = (1 - tex_alpha_texmap_diffuse * texmap_diffuse_amount) * diffuse_color + texmap_diffuse_amount * texmap_diffuse;
	}
	
	final_diffuse *= max(0.45, 1 - Fr * final_reflection) * (1 - final_refraction);
	float final_roughness = diffuse_roughness;
	if (final_diffuse[0] + final_diffuse[1] + final_diffuse[2] > 0)
	{
		if (texmap_roughness_on && !texmap_roughness_isnull)
		{
			float texmap_roughness_amount = texmap_roughness_multiplier * 0.01;
			final_roughness = 
				(1 - tex_alpha_texmap_roughness * texmap_roughness_amount) * diffuse_roughness 
				+ texmap_roughness_amount * intensity(texmap_roughness);
		}
	}

	float3 final_transparency = float3(0,0,0);
	// if (IsRefract(I, Nshading, refraction_ior) > 1e-6) // No refraction check w.r.t elara render
	{
		final_transparency += final_refraction;
	}
	if (texmap_opacity_on && !texmap_opacity_isnull)
	{
		float texmap_opacity_amount = texmap_opacity_multiplier * 0.01;
		float3 final_opacity = (1 - texmap_opacity_amount) + texmap_opacity_amount * texmap_opacity;
		final_transparency += 1 - final_opacity;
	}
	
	float3 final_emissive = selfIllumination;
	if (texmap_self_illumination_on && !texmap_self_illumination_isnull)
	{
		float texmap_selfIllumination_amount = texmap_self_illumination_multiplier * 0.01;
	
		final_emissive = (1 - tex_alpha_texmap_self_illumination * texmap_selfIllumination_amount) * selfIllumination + texmap_selfIllumination_amount * texmap_self_illumination;
	}
	
	final_reflection *= R0;
	// base color
	output0.xyz = final_diffuse;
	// specular
	output0.w = intensity(final_reflection);
	// emissive
	output1.xyz = final_emissive;
	// roughness
	output1.w = final_roughness;
	float opacity = 1 - intensity(saturate(final_transparency));
	output2 = float4(Nshading,opacity);
}
