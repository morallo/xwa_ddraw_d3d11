// Copyright (c) 2014 Jérémy Ansel
// Licensed under the MIT license. See LICENSE.txt

#pragma once

class Config
{
public:
	Config();

	bool AspectRatioPreserved;
	bool MultisamplingAntialiasingEnabled;
	int MSAACount;
	bool AnisotropicFilteringEnabled;
	bool VSyncEnabled;
	bool WireframeFillMode;
	int JoystickEmul;
	bool SwapJoystickXZAxes;
	int XInputTriggerAsThrottle;
	bool InvertYAxis;
	float MouseSensitivity;
	float KbdSensitivity;

	float Concourse3DScale;

	int ProcessAffinityCore;

	bool EnhanceLasers;
	bool EnhanceIllumination;
	bool EnhanceEngineGlow;
	bool EnhanceExplosions;

	bool FXAAEnabled;
	bool StayInHyperspace;
};

extern Config g_config;
