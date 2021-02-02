#pragma once

#define __int8 char
#define __int16 short
#define __int32 int
#define __int64 long long

#define BYTEn(x, n) (*((_BYTE*)&(x)+n))

#define BYTE1(x) BYTEn(x, 1) 
#define BYTE2(x) BYTEn(x, 2)

//typedef unsigned __int8 BYTE;
typedef unsigned __int8 _BYTE;
//typedef unsigned int DWORD;
typedef unsigned int _DWORD;
//typedef int LONG;

/*
typedef struct _RECT {
	LONG left;
	LONG top;
	LONG right;
	LONG bottom;
} RECT, *PRECT;
*/

// S0x07D4FA0
struct XwaGlobalLight
{
	/* 0x0000 */ int PositionX;
	/* 0x0004 */ int PositionY;
	/* 0x0008 */ int PositionZ;
	/* 0x000C */ float DirectionX;
	/* 0x0010 */ float DirectionY;
	/* 0x0014 */ float DirectionZ;
	/* 0x0018 */ float Intensity;
	/* 0x001C */ float XwaGlobalLight_m1C;
	/* 0x0020 */ float ColorR;
	/* 0x0024 */ float ColorB;
	/* 0x0028 */ float ColorG;
	/* 0x002C */ float BlendStartIntensity;
	/* 0x0030 */ float BlendStartColor1C;
	/* 0x0034 */ float BlendStartColorR;
	/* 0x0038 */ float BlendStartColorB;
	/* 0x003C */ float BlendStartColorG;
	/* 0x0040 */ float BlendEndIntensity;
	/* 0x0044 */ float BlendEndColor1C;
	/* 0x0048 */ float BlendEndColorR;
	/* 0x004C */ float BlendEndColorB;
	/* 0x0050 */ float BlendEndColorG;
};

struct XwaVector3
{
	/* 0x0000 */ float x;
	/* 0x0004 */ float y;
	/* 0x0008 */ float z;
};

// S0x0000002
struct XwaMatrix3x3
{
	/* 0x0000 */ float _11;
	/* 0x0004 */ float _12;
	/* 0x0008 */ float _13;
	/* 0x000C */ float _21;
	/* 0x0010 */ float _22;
	/* 0x0014 */ float _23;
	/* 0x0018 */ float _31;
	/* 0x001C */ float _32;
	/* 0x0020 */ float _33;
};

// S0x0000012
struct XwaTransform
{
	/* 0x0000 */ XwaVector3 Position;
	/* 0x000C */ XwaMatrix3x3 Rotation;
};