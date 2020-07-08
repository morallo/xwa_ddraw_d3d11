// MetricReconstructionCB
cbuffer ConstantBuffer : register(b6)
{
	float mr_aspect_ratio; // This is the same as g_fCurInGameAspectRatio
	float mr_FOVscale, mr_y_center, mr_z_metric_mult;

	float mr_cur_metric_scale, mr_shadow_OBJ_scale;
	float mr_screen_aspect_ratio; // g_fCurScreenWidth / g_fCurScreenHeight
	float mr_unused2;
};

