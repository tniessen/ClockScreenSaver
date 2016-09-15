#include "settings.h"

SETTINGS defaultSettings = {
	.scale = 80,
	.space = 20,
	.showSeconds = TRUE,
	.useCustomFont = FALSE,
	.fontName = L"",
	.fontWeight = FW_DONTCARE,
	.fontItalic = FALSE,
	.fgColor = RGB(255, 255, 255),
	.bgColor = RGB(0, 0, 0)
};

void PropertiesToSettings(PSETTINGS settings, PPROPERTIES props) {
	if (!GetUIntProperty(props, L"scale", &settings->scale)) {
		settings->scale = defaultSettings.scale;
	}

	if (!GetUIntProperty(props, L"space", &settings->space)) {
		settings->space = defaultSettings.space;
	}

	if (!GetBoolProperty(props, L"showSeconds", &settings->showSeconds)) {
		settings->showSeconds = defaultSettings.showSeconds;
	}

	if (!GetBoolProperty(props, L"useCustomFont", &settings->useCustomFont)) {
		settings->useCustomFont = defaultSettings.useCustomFont;
	}

	settings->fontName = GetProperty(props, L"fontName");
	if (!settings->fontName) {
		settings->fontName = defaultSettings.fontName;
	}

	if (!GetUIntProperty(props, L"fontWeight", &settings->fontWeight)) {
		settings->fontWeight = defaultSettings.fontWeight;
	}

	if (!GetBoolProperty(props, L"fontItalic", &settings->fontItalic)) {
		settings->fontItalic = defaultSettings.fontItalic;
	}

	if (!GetRgbProperty(props, L"bgColor", &settings->bgColor)) {
		settings->bgColor = defaultSettings.bgColor;
	}

	if (!GetRgbProperty(props, L"fgColor", &settings->fgColor)) {
		settings->fgColor = defaultSettings.fgColor;
	}
}

void SettingsToProperties(PSETTINGS settings, PPROPERTIES props) {
	SetUIntProperty(props, L"scale", settings->scale);
	SetUIntProperty(props, L"space", settings->space);
	SetBoolProperty(props, L"showSeconds", settings->showSeconds);
	SetBoolProperty(props, L"useCustomFont", settings->useCustomFont);
	SetProperty(props, L"fontName", settings->fontName);
	SetUIntProperty(props, L"fontWeight", settings->fontWeight);
	SetBoolProperty(props, L"fontItalic", settings->fontItalic);
	SetRgbProperty(props, L"bgColor", settings->bgColor);
	SetRgbProperty(props, L"fgColor", settings->fgColor);
}

void RestoreDefaultSettings(PSETTINGS settings) {
	CopyMemory(settings, &defaultSettings, sizeof(SETTINGS));
}
