// Hidden Area Mesh Pixel Shader
// Minimal pixel shader for depth-only rendering of the VR hidden area mesh.
// TEMP DEBUG: color writes enabled, outputs bright green for visibility.

float4 main() : SV_TARGET
{
	return float4(0, 1, 0, 1); // TEMP: bright green for debugging
}
