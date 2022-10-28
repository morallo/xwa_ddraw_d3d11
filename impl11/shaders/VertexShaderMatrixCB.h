// VertexShaderMatrixCB
cbuffer ConstantBuffer : register(b2)
{
	matrix projEyeMatrix[2];
	matrix viewMatrix;
	matrix fullViewMatrix;
	// Used for metric reconstruction
	float Znear, Zfar, DeltaX, DeltaY;
};

