/*
 * Screen-Space Directional Occlusion, based on Ritschel's paper with
 * code adapted from Pascal Gilcher's MXAO.
 * Adapted for XWA by Leo Reyes.
 * Licensed under the MIT license. See LICENSE.txt
 */
#include "..\shader_common.h"
#include "..\shading_system.h"
#include "..\SSAOPSConstantBuffer.h"

#define AO_INTENSITY 4.0

// The Foreground 3D position buffer (linear X,Y,Z)
Texture2D    texPos   : register(t0);
SamplerState sampPos  : register(s0) = 
	sampler_state {
		Filter = MIN_MAG_MIP_LINEAR;
		MaxLOD = MAX_MIP_LEVELS;
		MinLOD = 0;
	};

// The normal buffer
Texture2D    texNorm   : register(t1);
SamplerState sampNorm  : register(s1);

// The color buffer
Texture2D    texColor  : register(t2);
SamplerState sampColor : register(s2);

// The ssao mask
Texture2D    texSSAOMask  : register(t3);
SamplerState sampSSAOMask : register(s3);

// The bloom mask
Texture2D    texBloomMask  : register(t4);
SamplerState sampBloomMask : register(s4);

struct PixelShaderInput
{
	float4 pos : SV_POSITION;
	float2 uv  : TEXCOORD;
};

struct PixelShaderOutput
{
	float4 ssao        : SV_TARGET0; // SSDO Direct output
	float4 bentNormal  : SV_TARGET1; // Bent Normal
	//float4 emission    : SV_TARGET2; // Emission Mask
};

inline float3 getPosition(in float2 uv, in float level) {
	// The use of SampleLevel fixes the following error:
	// warning X3595: gradient instruction used in a loop with varying iteration
	// This happens because the texture is sampled within an if statement (if FGFlag then...)
	return texPos.SampleLevel(sampPos, uv, level).xyz;
}

inline float3 getNormal(in float2 uv, in float level) {
	return texNorm.SampleLevel(sampNorm, uv, level).xyz;
}

/*
 * From Pascal Gilcher's SSR shader.
 * https://github.com/martymcmodding/qUINT/blob/master/Shaders/qUINT_ssr.fx
 * (Used with permission from the author)
 */
/*
float3 get_normal_from_color(float2 uv, float2 offset)
{
	float3 offset_swiz = float3(offset.xy, 0);
	// Luminosity samples
	float hpx = dot(texColor.SampleLevel(sampColor, float2(uv + offset_swiz.xz), 0).xyz, 0.333) * fn_scale;
	float hmx = dot(texColor.SampleLevel(sampColor, float2(uv - offset_swiz.xz), 0).xyz, 0.333) * fn_scale;
	float hpy = dot(texColor.SampleLevel(sampColor, float2(uv + offset_swiz.zy), 0).xyz, 0.333) * fn_scale;
	float hmy = dot(texColor.SampleLevel(sampColor, float2(uv - offset_swiz.zy), 0).xyz, 0.333) * fn_scale;

	// Depth samples
	float dpx = getPosition(uv + offset_swiz.xz, 0).z;
	float dmx = getPosition(uv - offset_swiz.xz, 0).z;
	float dpy = getPosition(uv + offset_swiz.zy, 0).z;
	float dmy = getPosition(uv - offset_swiz.zy, 0).z;

	// Depth differences in the x and y axes
	float2 xymult = float2(abs(dmx - dpx), abs(dmy - dpy)) * fn_sharpness;
	//xymult = saturate(1.0 - xymult);
	xymult = saturate(fn_max_xymult - xymult);

	float3 normal;
	normal.xy = float2(hmx - hpx, hmy - hpy) * xymult / offset.xy * 0.5;
	normal.z = 1.0;

	return normalize(normal);
}
*/

/*
// n1: base normal
// n2: detail normal
float3 blend_normals(float3 n1, float3 n2)
{
	// I got this from Pascal Gilcher; but there's more details here:
	// https://blog.selfshadow.com/publications/blending-in-detail/
	//return normalize(float3(n1.xy*n2.z + n2.xy*n1.z, n1.z*n2.z));

	// UDN:
	//return normalize(float3(n1.xy + n2.xy, n1.z));
	
	//n1 += float3( 0,  0, 1);
	//n2 *= float3(-1, -1, 1);
	//return n1 * dot(n1, n2) / n1.z - n2;

	//float3 t = tex2D(texBase, uv).xyz*float3(2, 2, 2) + float3(-1, -1, 0);
	n1.z += 1.0;
	//float3 u = tex2D(texDetail, uv).xyz*float3(-2, -2, 2) + float3(1, 1, -1);
	n2.xy = -n2.xy;

	//float3 r = t * dot(t, u) / t.z - u;
	//return normalize(n1 * dot(n1, n2) / n1.z - n2);
	return normalize(n1 * dot(n1, n2) - n1.z * n2);
	//return r * 0.5 + 0.5;
}
*/

struct ColNorm {
	float3 col; // SSDO Direct Color
	float3 N;   // Normal
	//float3 E;   // Emission color
};

inline ColNorm doSSDODirect(in float2 input_uv, in float2 sample_uv, in float3 color,
	in float3 P, in float3 Normal, in float cur_radius_sqr, in float max_radius_sqr, in float max_dist_sqr,
	out float ao_factor)
{
	ColNorm output;
	output.col = 0;
	output.N   = 0;
	//output.E   = 0;
	ao_factor  = 0;
	//shadow_factor = 1.0;

	// Early exit: darken the edges of the effective viewport
	//if (sample_uv.x < x0 || sample_uv.x > x1 ||
	//	sample_uv.y < y0 || sample_uv.y > y1) 
	if (any(sample_uv.xy < p0) || any(sample_uv.xy > p1))
		return output;
	const float2 uv_diff = sample_uv - input_uv;
	
#ifdef GENMIPMAPS
	//float miplevel = L / max_radius * MAX_MIP_LEVELS; // Don't know if this miplevel actually improves performance
	//const float miplevel = cur_radius_sqr / max_radius_sqr * MAX_MIP_LEVELS; // Is this miplevel better than using L? (I think L was sqrt(cur_radius_sqr))
	const float miplevel = sqrt(cur_radius_sqr) / sqrt(max_radius_sqr) * MAX_MIP_LEVELS;
#else
	const float miplevel = 0.0;
#endif

	float3 occluder = getPosition(sample_uv, miplevel);
	float3 occ_mask = texSSAOMask.SampleLevel(sampSSAOMask, sample_uv, 0).xyz;
	float  occ_material = occ_mask.x;

	//float3 occluder_normal = getNormal(sample_uv, 0);
	//if (occluder.z > INFINITY_Z) // The sample is at infinity, don't compute any SSDO
	//	return output;
	//output.was_sampled = occluder.z < INFINITY_Z;

	// diff: Vector from current pos (P) to the sampled neighbor
	//       If the occluder is farther than P, then diff.z will be positive
	//		 If the occluder is closer than P, then  diff.z will be negative
	const float3 diff = occluder - P;
	const float  diff_sqr = dot(diff, diff);
	//const float dist = length(diff);
	const float3 v = diff * rsqrt(diff_sqr); // normalize(diff)
	// weight = 1 when the occluder is close, 0 when the occluder is at max_dist
	//const float weight = 1.0 - saturate((diff.z*diff.z) / (max_dist*max_dist));
	//const float weight = 1.0 - saturate(diff.z / max_dist);
	//const float weight = 1 - saturate(diff.z / max_dist);
	//weight = 1 - weight * weight;
	
	/*
	  The right way to fix this is probably going to be as follows:
	  Compute the equation of the plane with normal n passing through p:
	  if n = [A,B,C] and |n| = 1, then the eqn is:
	  Ax + By + Cz - (A*p.x + B*p.y + C*p.z) = 0;
	  Then compute the ray passing through the eye (0,0,0)? into the current sample's 3D position
	  in parametric form: L = p0 + t*ray_dir
	  Compute the intersection of this ray with the plane and get the Z_int value
	  If weight is close to 0 (that is, if the occluder is too far away), then
	  consider this sample as a non-occluder and use the first path below, the one we
	  use to add to the bent normal (if (diff.z > 0.0)) and compute the color with the new
	  intersection we just computed.
	  If weight is close to 1, consider this a regular occluder and take the second path
	  below.
	 */
	float3 B = 0;
	//const float occ_dot = dot(Normal, v);
	
	B.x =  uv_diff.x;
	B.y = -uv_diff.y;
	//B.z = 0.01 * (max_radius - cur_radius) / max_radius;
	//B.z = sqrt(max_radius * max_radius - cur_radius * cur_radius);
	//B.z = 0.5 * sqrt(max_radius * max_radius - cur_radius * cur_radius); // Too bluish
	//B.z = 0.1 * sqrt(max_radius * max_radius - cur_radius * cur_radius); // ORIGINAL
	//B.z = 0.05 * sqrt(max_radius * max_radius - cur_radius * cur_radius); // Kind of OK
	//B.z = 0.01 * sqrt(max_radius * max_radius - cur_radius * cur_radius); // Too noisy
	B.z = Bz_mult * sqrt(max_radius_sqr - cur_radius_sqr);
	//B.z = Bz_mult * sqrt(max_radius * max_radius - cur_radius * cur_radius) / max_radius; // Looks good for bzmult = 0.0025
	// Adding the normalized B to BentNormal seems to yield better normals
	// if B is added to BentNormal before normalization, the resulting normals
	// look more faceted
	B = normalize(B);

	// The occluder is behind the current point when diff.z > 0 (weight = 1)
	// If (diff.z > 0) --> weight = 1: NO Occlusion, compute Bent Normal, etc
	// If (diff.z < 0) --> weight = 0: Occluded direction, Bent Normal/SSDO contribution is 0
	const float weight = diff.z > 0.0 ? 1.0 : 0.0;

	// Compute SSAO to get a better bent normal
	const float3 SSAO_Normal = float3(Normal.xy, -Normal.z);
	//const float3 SSAO_Normal = Normal;
	const float ao_dot = max(0.0, dot(SSAO_Normal, v) - bias);
	const float ao_weight = saturate(1 - diff_sqr / max_dist_sqr);
	ao_factor = ao_dot * ao_weight;

	// If weight == 0, use the current point's Normal to compute shading
	// If weight == 1, use the Bent Normal
	// Vary between these two values depending on 'weight':
	//B = lerp(Normal, B, weight);
	//B = Normal;
		
	// output.N = B; // ORIGINAL
	output.N = weight * B; // The bent normal should be weighed by the occlusion: it should be smaller on more occluded areas (I think)
	//output.N = weight * v; // This also works; but I can't get the contact shadows right and everything looks overly bluish -- maybe I need to scale down the Z axis?

	//output.col = LightColor.rgb * saturate(dot(B, MainLight.xyz));
	//B.z = -B.z; // Flip the z component?
	//output.col = weight * saturate(dot(B, MainLight.xyz)); // ORIGINAL
	[loop]
	for (uint i = 0; i < LightCount; i++)
		output.col += saturate(dot(B, LightVector[i].xyz));
	output.col = output.col * weight; // / LightCount;
	//output.col = weight; // This doesn't produce a contact shadow because it does not depend on the light direction
	//output.col  = LightColor.rgb  * saturate(dot(B, MainColor.xyz)) + invLightColor * saturate(dot(B, -MainLight.xyz));
	
	// Compute the shadow factor
	// If the sample vector B points right in the direction of L, and weight = 0, then this
	// point is in shadow
	//shadow_factor = weight * saturate(dot(B, LightVector[0].xyz)); // This is just a sub-component of SSDO...

	/*else { // The occluder is in front of the current point: i.e. The occluder *is* occluding the current point
		// This is where we would be computing AO if this were SSAO.
		// This is where the black/white halos appear around the foreground objects
		output.N = 0; // This is OK for bent normal accumulation, the accumulation is weighed by the occlusion factor
		output.col = 0;
	}
	*/

	/*
	// Compute emission, if applicable
	if (occ_material > EMISSION_LO) {
		float3 occ_color = texColor.SampleLevel(sampColor, sample_uv, 0).xyz;
		// NOTE: We're not writing the 3D pos for lasers because we don't want lasers to contribute to
		//       SSAO/SSDO, so there's no point trying to compute v for these elements!
		// v = occluder - P, so it goes from the current point to the occluder
		//output.E = occ_color * max(dot(B, -v), 0); // Should I use B or Normal?
		//output.E = occ_color * max(dot(SSAO_Normal, -v), 0); // Should I use B or Normal?
		output.E = occ_color;
	}
	*/
	return output;
}

/*
// Convert Reconstructed 3D back to 2D and then to post-process UV coord
inline float2 projectToUV(in float3 pos3D) {
	float3 P = pos3D;
	//float w = P.z / METRIC_SCALE_FACTOR;
	P.xy = P.xy / P.z;
	// Convert to vertex pos:
	//P.xy = ((P.xy / (vpScale.z * float2(aspect_ratio, 1))) - float2(-0.5, 0.5)) / vpScale.xy;
	// (-1,-1)-(1, 1)
	P.xy /= (vpScale.z * float2(aspect_ratio, 1));
	//P.xy -= float2(-0.5, 0.5); // Is this wrong? I think the y axis is inverted, so adding 0.5 would be fine (?)
	P.xy -= float2(-1.0, 1.0); // DirectSBS may be different
	P.xy /= vpScale.xy;
	// We now have P = input.pos
	P.x = (P.x * vpScale.x - 1.0f) * vpScale.z;
	P.y = (P.y * vpScale.y + 1.0f) * vpScale.z;
	//P *= 1.0f / w; // Don't know if this is 100% necessary... probably not

	// Now convert to UV coords: (0, 1)-(1, 0):
	//P.xy = lerp(float2(0, 1), float2(1, 0), (P.xy + 1) / 2);
	// The viewport used to render the original offscreenBuffer may not cover the full
	// screen, so the uv coords have to be adjusted to the limits of the viewport within
	// the full-screen quad:
	P.xy = lerp(float2(x0, y1), float2(x1, y0), (P.xy + 1) / 2);
	return P.xy;
}
*/

/*
float3 shadow_factor(in float3 P, float max_dist_sqr) {
	float3 cur_pos = P, occluder, diff;
	float2 cur_uv;
	float3 ray_step = shadow_step_size * LightVector[0].xyz;
	int steps = (int)shadow_steps;
	float max_shadow_length = shadow_step_size * shadow_steps;
	float max_shadow_length_sqr = max_shadow_length * 0.75; // Fade the shadow a little before it reaches a hard edge
	max_shadow_length_sqr *= max_shadow_length_sqr;
	float cur_length = 0, length_at_res = INFINITY_Z1;
	float res = 1.0;
	float weight = 1.0;
	//float occ_dot;

	// Handle samples that land outside the bounds of the image
	// "negative" cur_diff should be ignored
	[loop]
	for (int i = 1; i <= steps; i++) {
		cur_pos += ray_step;
		cur_length += shadow_step_size;
		cur_uv = projectToUV(cur_pos);

		// If the ray has exited the current viewport, we're done:
		if (cur_uv.x < x0 || cur_uv.x > x1 ||
			cur_uv.y < y0 || cur_uv.y > y1) {
			weight = saturate(1 - length_at_res * length_at_res / max_shadow_length_sqr);
			res = lerp(1, res, weight);
			return float3(res, cur_length / max_shadow_length, 1);
		}

		occluder = texPos.SampleLevel(sampPos, cur_uv, 0).xyz;
		diff = cur_pos - occluder;
		//v        = normalize(diff);
		//occ_dot  = max(0.0, dot(LightVector.xyz, v) - bias);

		if (diff.z > 0) { // Ignore negative z-diffs: the occluder is behind the ray
			// If diff.z is too large, ignore it. Or rather, fade with distance
			//float weight = saturate(1.0 - (diff.z * diff.z / max_dist_sqr));
			//float dist = saturate(lerp(1, diff.z), weight);
			float cur_res = saturate(shadow_k * diff.z / (cur_length + 0.00001));
			//cur_res = saturate(lerp(1, cur_res, weight)); // Fadeout if diff.z is too big
			if (cur_res < res) {
				res = cur_res;
				length_at_res = cur_length;
			}
		}

		//cur_pos += ray_step;
		//cur_length += shadow_step_size;
	}
	weight = saturate(1 - length_at_res * length_at_res / max_shadow_length_sqr);
	res = lerp(1, res, weight);
	return float3(res, cur_length / max_shadow_length, 0);
}
*/

PixelShaderOutput main(PixelShaderInput input)
{
	PixelShaderOutput output;
	ColNorm ssdo_aux;
	float3 p = getPosition(input.uv, 0);
	float3 Normal = getNormal(input.uv, 0);
	//Normal.z = -Normal.z;
	float3 SSAO_Normal = float3(Normal.xy, -Normal.z); // For SSAO we need to flip the Z component because the code expects a different coord system
	//float3 SSAO_Normal = Normal;
	//float3 color = texColor.SampleLevel(sampColor, input.uv, 0).xyz;
	float3 color = 0;
	//float3 ssao_mask = texSSAOMask.SampleLevel(sampSSAOMask, input.uv, 0).xyz;
	//float  material  = mask.x;
	float4 bloom_mask_rgba = texBloomMask.SampleLevel(sampBloomMask, input.uv, 0);
	float bloom_mask = bloom_mask_rgba.a * dot(0.333, bloom_mask_rgba.rgb);
	//float3 bentNormal = float3(0, 0, 0);
	// A value of bentNormalInit == 0.2 seems to work fine.
	float3 bentNormal = bentNormalInit * Normal; // Initialize the bentNormal with the normal
	//float3 bentNormal = 0;
	//float3 bentNormal = float3(0, 0, 0.01);
	float3 ssdo;
	float radius;
	//bool FGFlag;
	
	//color = color * color; // Gamma correction (approx to pow 2.2)

	output.ssao = 1;
	output.bentNormal = float4(0, 0, 0.01, 1); //output.bentNormal = 0;
	//output.emission = 0;	

	// This apparently helps prevent z-fighting noise
	// SSDO version:
	//float m_offset = max(moire_offset, moire_offset * (p.z * 0.1));
	//p.z -= m_offset;

	// SSAO version:
	//float m_offset = moire_offset * (p.z * 0.1);
	float m_offset = moire_offset * (p.z * moire_scale);
	p += m_offset * SSAO_Normal;

	// Early exit: do not compute SSDO for objects at infinity
	if (p.z > INFINITY_Z1) return output;

	// Interpolate between near_sample_radius at z == 0 and far_sample_radius at 1km+
	// We need to use saturate() here or we actually get negative numbers!
	radius = lerp(near_sample_radius, far_sample_radius, saturate(p.z / 1000.0));
	//float nm_intensity = (1 - ssao_mask) * lerp(nm_intensity_near, nm_intensity_far, saturate(p.z / 4000.0));
	//radius = far_sample_radius;
	// Enable perspective-correct radius
	//if (z_division) 	radius /= p.z;

	//float2 offset = float2(0.5 / screenSizeX, 0.5 / screenSizeY); // Test setting
	//float2 offset = float2(1.0 / screenSizeX, 1.0 / screenSizeY); // Original setting
	//float2 offset = 0.1 * float2(1.0 / screenSizeX, 1.0 / screenSizeY);
	//float3 FakeNormal = 0; 
	//if (fn_enable) FakeNormal = get_normal_from_color(input.uv, offset); // Let's postpone normal mapping until the final combine stage

	/*
	// Compute shadows
	float max_dist_sqr = max_dist * max_dist;
	m_offset = max(moire_offset, moire_offset * (p.z * 0.1));
	p += n * m_offset;
	float3 shadow = shadow_factor(p, max_dist_sqr);
	shadow = shadow.xxx;
	if (!shadow_enable)
		shadow = 1;
	*/

	const float max_dist_sqr = max_dist * max_dist;
	float sample_jitter = dot(floor(input.pos.xy % 4 + 0.1), float2(0.0625, 0.25)) + 0.0625;
	float2 sample_uv, sample_direction;
	const float2x2 rotMatrix = float2x2(0.76465, -0.64444, 0.64444, 0.76465); //cos/sin 2.3999632 * 16 
	sincos(2.3999632 * 16 * sample_jitter, sample_direction.x, sample_direction.y); // 2.3999632 * 16
	// After sincos, sample_direction is unitary and 2D
	sample_direction *= radius;
	// Then we multiply it by "radius", so now sample_direction has length "radius"
	const float max_radius = radius * (float)(samples + sample_jitter); // Using samples - 1 causes imaginary numbers in B.z (because of the sqrt)
	const float max_radius_sqr = max_radius * max_radius;
	float ao_factor = 0.0, ao = 0.0;
	//float3 emission = 0;
	//float shadow_factor = 1.0, min_shadow_factor = 1.0;

	// SSDO Direct Calculation
	ssdo = 0;
	[loop]
	for (uint j = 0; j < samples; j++)
	{
		float radius_sqr = radius * (j + sample_jitter);
		radius_sqr *= radius_sqr;

		sample_uv = input.uv + sample_direction.xy * (j + sample_jitter);
		sample_direction.xy = mul(sample_direction.xy, rotMatrix);
		ssdo_aux = doSSDODirect(input.uv, sample_uv, color, p, Normal, 
			radius_sqr, max_radius_sqr, max_dist_sqr, ao_factor);
		ssdo       += ssdo_aux.col;
		bentNormal += ssdo_aux.N;
		//emission   += ssdo_aux.E;
		ao         += ao_factor;
	}
	// This AO won't be displayed to the user, hard-code the intensity:
	ao = saturate(1.0 - AO_INTENSITY * ao / (float)samples);

	// DEBUG
	//if (fn_enable) bentNormal = FakeNormal;
	// DEBUG
	//bentNormal /= (float)samples;
	//bentNormal = normalize(bentNormal); // This looks a bit better
	//const float3 debugBentNormal = normalize(bentNormal);
	// Let's make a cleaner bent normal. The bent normal is the smooth normal when there's no occlusion
	// so let's use the AO term to select the normal or the bent normal
	bentNormal = ao * normalize(lerp(bentNormal, Normal, ao * ao)); // Using ao^2 makes the dark areas darker... kind of looks better, I think
	//output.bentNormal.xyz = bentNormal; // * 0.5 + 0.5;
	// Start fading the bent normals at INFINITY_Z0 and fade out completely at INFINITY_Z1
	output.bentNormal.xyz = lerp(bentNormal, 0, saturate((p.z - INFINITY_Z0) / INFINITY_FADEOUT_RANGE));
	// Bent normal tweak: don't smooth the bent normal itself; but the difference with the smooth normal
	output.bentNormal.xyz = Normal - output.bentNormal.xyz;
	
	//// Compute the contact shadow and put it in the y channel (not done here anymore)
	float bentDiff = max(dot(bentNormal, LightVector[0].xyz), 0.0);
	//float normDiff = max(dot(Normal, LightVector[0].xyz), 0.0);
	////float bentDiff = dot(bentNormal, LightVector[0].xyz); // Removing max also reduces the effect, looks better with max
	////float normDiff = dot(n, LightVector[0].xyz);
	//float contactShadow = 1.0 - clamp(normDiff - bentDiff, 0.0, 1.0);

	ssdo   = saturate(intensity * ssdo / (float)samples); // * shadow;
	ssdo.y = ao;
	//ssdo.z = shadow_factor;
	if (bloom_mask < 0.975) bloom_mask = 0.0; // Only inhibit SSDO when bloom > 0.975
	ssdo = lerp(ssdo, 1, bloom_mask);
	ssdo = pow(abs(ssdo), power);
	// Start fading the effect at INFINITY_Z0 and fade out completely at INFINITY_Z1
	output.ssao.xyz = lerp(ssdo, 1, saturate((p.z - INFINITY_Z0) / INFINITY_FADEOUT_RANGE));

	// Compute the final emission component
	//output.emission = float4(saturate(emission_intensity * emission / (float)samples), 1);

	// DEBUG
	if (ssao_debug == 1)
		output.ssao.xyz = output.ssao.xxx;
	if (ssao_debug == 2)
		output.ssao.xyz = bentDiff;
	if (ssao_debug == 3)
		output.ssao.xyz = ao * ao; // Gamma correction (or at least makes it looks like the output from SSAO anyway)
	//if (ssao_debug == 4)
	//	output.ssao.xyz = debugBentNormal;
	if (ssao_debug == 5)
		output.ssao.xyz = bentNormal;
	if (ssao_debug == 6)
		output.ssao.xyz = Normal;
	//if (ssao_debug == 7)
	//	output.ssao.xyz = lerp(float3(0, 0, 0.1), float3(1, 1, 1), contactShadow);

	//if (ssao_debug != 0)
	//	return output;
	// DEBUG
	
	//if (fn_enable) bentNormal = blend_normals(nm_intensity * FakeNormal, bentNormal); // bentNormal is not really used, it's just for debugging.
	//bentNormal /= BLength; // Bent Normals are not supposed to be normalized
	
	//output.bentNormal.xyz = output.ssao.xyz * bentNormal;
	//output.bentNormal.xyz = radius * 100; // DEBUG! Use this to visualize the radius
	return output;
}
