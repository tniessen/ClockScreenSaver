#pragma once

#include <Windows.h>

typedef struct {
	PWSTR name;
	PWSTR value;
} PROPERTY, *PPROPERTY, **PPPROPERTY;

typedef struct {
	UINT count;
	PPROPERTY items;
} PROPERTIES, *PPROPERTIES;

void FreeProperties(PPROPERTIES props);

BOOL SetProperty(PPROPERTIES props, PWSTR name, PWSTR value);

PWSTR GetProperty(PPROPERTIES props, PWSTR name);

BOOL SetUIntProperty(PPROPERTIES props, PWSTR name, UINT value);

BOOL GetUIntProperty(PPROPERTIES props, PWSTR name, PUINT value);

BOOL SetRgbProperty(PPROPERTIES props, PWSTR name, COLORREF value);

BOOL GetRgbProperty(PPROPERTIES props, PWSTR name, LPCOLORREF value);

BOOL SetBoolProperty(PPROPERTIES props, PWSTR name, BOOL value);

BOOL GetBoolProperty(PPROPERTIES props, PWSTR name, PBOOL value);

BOOL ReadProperties(PPROPERTIES props, PWSTR path);

BOOL WriteProperties(PPROPERTIES props, PWSTR path);
