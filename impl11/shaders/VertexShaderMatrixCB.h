// VertexShaderMatrixCB
cbuffer ConstantBuffer : register(b2)
{
	matrix projEyeMatrix;
	matrix viewMatrix;
	matrix fullViewMatrix;
	// Used for metric reconstruction
	float Znear, Zfar, DeltaX, DeltaY;
};

