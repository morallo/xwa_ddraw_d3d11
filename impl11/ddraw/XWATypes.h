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

// These definitions are now in xwa_structures.h
struct XwaGlobalLight;
struct XwaVector3;
struct XwaMatrix3x3;
struct XwaTransform;
