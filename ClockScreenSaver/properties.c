#include "properties.h"

static BOOL IsKeyChar(WCHAR c) {
	if (c >= 'A' && c <= 'Z') return TRUE;
	if (c >= 'a' && c <= 'z') return TRUE;
	if (c >= '0' && c <= '9') return TRUE;
	return (c == '_' || c == '.' || c == '-');
}

void FreeProperties(PPROPERTIES props) {
	if (props->items) {
		for (UINT i = 0; i < props->count; i++) {
			free(props->items[i].name);
			free(props->items[i].value);
		}

		free(props->items);
		props->items = NULL;
	}
	props->count = 0;
}

static BOOL FindProperty(PPROPERTIES props, PWSTR name, PUINT index) {
	for (*index = 0; *index < props->count; (*index)++) {
		if (wcscmp(name, props->items[*index].name) == 0) {
			return TRUE;
		}
	}
	return FALSE;
}

BOOL SetPropertyInternal(PPROPERTIES props, PWSTR name, PWSTR value) {
	// If a property with the same name already exists, replace its value
	UINT propIndex;
	if (FindProperty(props, name, &propIndex)) {
		free(props->items[propIndex].value);
		props->items[propIndex].value = value;
		return TRUE;
	}

	// Otherwise, we need to add a new property

	// Allocate new array
	PPROPERTY newItems = calloc(1 + props->count, sizeof(PROPERTY));
	if (!newItems) {
		return FALSE;
	}

	// Copy previous properties
	if (props->items) {
		memcpy(newItems, props->items, sizeof(PROPERTY) * props->count);
		free(props->items);
	}

	// Add new property
	newItems[props->count].name = name;
	newItems[props->count].value = value;
	props->count++;

	// Replace old properties pointer
	props->items = newItems;

	return TRUE;
}

BOOL SetProperty(PPROPERTIES props, PWSTR name, PWSTR value) {
	return SetPropertyInternal(props, _wcsdup(name), _wcsdup(value));
}

PWSTR GetProperty(PPROPERTIES props, PWSTR name) {
	UINT propIndex;
	if (FindProperty(props, name, &propIndex)) {
		return props->items[propIndex].value;
	}
	return NULL;
}

BOOL SetUIntProperty(PPROPERTIES props, PWSTR name, UINT value) {
	WCHAR val[128];
	wsprintf(val, L"%u", value);
	return SetProperty(props, name, val);
}

BOOL GetUIntProperty(PPROPERTIES props, PWSTR name, PUINT value) {
	PWSTR val = GetProperty(props, name);
	if (val) {
		PWSTR end;
		*value = wcstoul(val, &end, 10);
		if (!*end) {
			return TRUE;
		}
	}
	return FALSE;
}

static void ByteToHex(PWSTR str, BYTE b) {
	str[0] = "0123456789ABCDEF"[b / 16];
	str[1] = "0123456789ABCDEF"[b % 16];
	str[2] = '\0';
}

static BYTE HexDigitToByte(WCHAR c) {
	if (c >= 'a' && c <= 'f') return c - 'a' + 10;
	if (c >= 'A' && c <= 'F') return c - 'A' + 10;
	if (c >= '0' && c <= '9') return c - '0';
	return 0;
}

static BYTE HexToByte(PWSTR str) {
	return (HexDigitToByte(str[0]) << 4) | HexDigitToByte(str[1]);
}

BOOL SetRgbProperty(PPROPERTIES props, PWSTR name, COLORREF value) {
	WCHAR val[7];
	ByteToHex(val + 0, GetRValue(value));
	ByteToHex(val + 2, GetGValue(value));
	ByteToHex(val + 4, GetBValue(value));

	return SetProperty(props, name, val);
}

BOOL GetRgbProperty(PPROPERTIES props, PWSTR name, LPCOLORREF value) {
	PWSTR val = GetProperty(props, name);
	if (!val) return FALSE;
	if (wcslen(val) < 6) return FALSE;

	BYTE r = HexToByte(val + 0);
	BYTE g = HexToByte(val + 2);
	BYTE b = HexToByte(val + 4);
	*value = RGB(r, g, b);

	return TRUE;
}

BOOL SetBoolProperty(PPROPERTIES props, PWSTR name, BOOL value) {
	return SetProperty(props, name, value ? L"yes" : L"no");
}

BOOL GetBoolProperty(PPROPERTIES props, PWSTR name, PBOOL value) {
	PWSTR val = GetProperty(props, name);
	if (!val) return FALSE;

	if (_wcsicmp(val, L"yes") * _wcsicmp(val, L"true") == 0) {
		*value = TRUE;
		return TRUE;
	}
	else if (_wcsicmp(val, L"no") * _wcsicmp(val, L"false") == 0) {
		*value = FALSE;
		return TRUE;
	}
	else {
		return FALSE;
	}
}

// This function actually null-terminates the result
static PWSTR ConvertUtf8ToUtf16(const PCHAR str, const int szIn, PUINT charsOut) {
	int reqSize = MultiByteToWideChar(CP_UTF8, 0, str, szIn, NULL, 0);
	PWSTR wstr = calloc(reqSize + 1, sizeof(WCHAR));
	if (!wstr) return NULL;

	*charsOut = MultiByteToWideChar(CP_UTF8, 0, str, szIn, wstr, reqSize);
	return wstr;
}

// This function does not necessarily null-terminate the result
static PSTR ConvertUtf16ToUtf8(const PWSTR wstr, const int szIn, PUINT bytesOut) {
	int reqSize = WideCharToMultiByte(CP_UTF8, 0, wstr, szIn, NULL, 0, NULL, NULL);
	PSTR str = calloc(reqSize, sizeof(CHAR));
	if (!str) return NULL;

	*bytesOut = WideCharToMultiByte(CP_UTF8, 0, wstr, szIn, str, reqSize, NULL, NULL);
	return str;
}

BOOL ReadProperties(PPROPERTIES props, PWSTR path) {
	HANDLE hFile = CreateFile(path, GENERIC_READ, 0, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		return FALSE;
	}

	LARGE_INTEGER fileSize;
	if (!GetFileSizeEx(hFile, &fileSize)) {
		return FALSE;
	}

	PCHAR dataUtf8 = calloc(fileSize.LowPart + 1, sizeof(CHAR));
	if (!dataUtf8) {
		return FALSE;
	}

	DWORD read;
	if (!ReadFile(hFile, dataUtf8, fileSize.LowPart, &read, NULL)) {
		free(dataUtf8);
		return FALSE;
	}

	CloseHandle(hFile);

	UINT dataLen;
	PWSTR data = ConvertUtf8ToUtf16(dataUtf8, fileSize.LowPart, &dataLen);
	free(dataUtf8);

	UINT i = 0, j, k, l, m;
	while (i < dataLen) {
		// Skip leading whitespace
		while (data[i] && iswspace(data[i])) i++;

		if (!data[i]) break;

		// Read property name
		j = i + 1;
		while (data[j] && IsKeyChar(data[j])) j++;
		if (j == i) {
			free(data);
			SetLastError(ERROR_INVALID_DATA);
			return FALSE;
		}

		if (!data[j]) {
			SetLastError(ERROR_INVALID_DATA);
			return FALSE;
		}

		// Skip spaces
		k = j;
		while (data[k] && iswspace(data[k])) k++;

		if (!data[k]) {
			SetLastError(ERROR_INVALID_DATA);
			return FALSE;
		}

		// Expect '='
		if (data[k++] != '=') {
			free(data);
			SetLastError(ERROR_INVALID_DATA);
			return FALSE;
		}

		// Skip spaces
		while (data[k] && data[k] != '\n' && iswspace(data[k])) k++;

		if (!data[k]) {
			SetLastError(ERROR_INVALID_DATA);
			return FALSE;
		}

		// Read value
		l = k;
		while (data[l] != '\n' && data[l]) l++;

		// Remove trailing whitespace from value (including CRLF)
		m = l;
		while (iswspace(data[l - 1])) l--;

		// Copy name into string
		PWSTR name = calloc(j - i + 1, sizeof(WCHAR));
		if (!name) {
			free(data);
			return FALSE;
		}
		wcsncpy_s(name, j - i + 1, data + i, j - i);

		// Copy value into string
		PWSTR value = calloc(l - k + 1, sizeof(WCHAR));
		if (!value) {
			free(data);
			return FALSE;
		}
		wcsncpy_s(value, l - k + 1, data + k, l - k);

		// Save property
		SetPropertyInternal(props, name, value);

		// Skip to end of line
		i = m;
	}

	free(data);

	return TRUE;
}

BOOL WriteProperties(PPROPERTIES props, PWSTR path) {
	HANDLE hFile = CreateFile(path, GENERIC_WRITE, 0, NULL, CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, NULL);
	if (hFile == INVALID_HANDLE_VALUE) {
		return FALSE;
	}

	for (UINT i = 0; i < props->count; i++) {
		PPROPERTY prop = &props->items[i];

		SIZE_T szLine = wcslen(prop->name) + 1 + wcslen(prop->value) + 2;
		PWSTR line = calloc(szLine, sizeof(WCHAR));
		if (!line) return FALSE;

		wcscpy_s(line, szLine, prop->name);
		wcscat_s(line, szLine, TEXT("="));
		wcscat_s(line, szLine, prop->value);
		wcscat_s(line, szLine, TEXT("\n"));

		UINT dataSize;
		PCHAR data = ConvertUtf16ToUtf8(line, szLine - 1, &dataSize);
		free(line);

		DWORD written;
		BOOL ret = WriteFile(hFile, data, dataSize, &written, NULL);

		free(data);

		if(!ret) {
			return FALSE;
		}
	}

	FlushFileBuffers(hFile);
	CloseHandle(hFile);

	return TRUE;
}
