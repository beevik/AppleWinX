/****************************************************************************
*
*  APPLE //E EMULATOR FOR WINDOWS
*
*  Copyright (C) 1994-96, Michael O'Brien.  All rights reserved.
*
***/

#include "pch.h"
#pragma  hdrstop

/****************************************************************************
*
*   Variables
*
***/

static std::unordered_map<std::string, std::string> s_strings;
static std::unordered_map<std::string, int>         s_values;


/****************************************************************************
*
*   Local functions
*
***/

//===========================================================================
static void GetConfigFilename(char * filename, int filenameSize) {
#if defined(OS_WINDOWS)
    GetEnvironmentVariableA("APPDATA", filename, filenameSize);
    StrCat(filename, "\\AppleWinX", filenameSize);
    CreateDirectory(filename, NULL);
    StrCat(filename, "\\config.ini", filenameSize);
#elif defined(OS_POSIX)
    // TODO: write me
#endif
}


/****************************************************************************
*
*   Public functions
*
***/

//===========================================================================
void ConfigLoad() {
    s_strings.clear();
    s_values.clear();

    char filename[MAX_PATH];
    GetConfigFilename(filename, ARRSIZE(filename));

    char * buf = nullptr;
    FILE * fp = fopen(filename, "r");
    if (!fp)
        goto exit;

    fseek(fp, 0, SEEK_END);
    fpos_t size;
    if (fgetpos(fp, &size) != 0)
        goto exit;

    fseek(fp, 0, SEEK_SET);
    buf = new char[size];
    size_t bytesRead = fread(buf, 1, size, fp);

    const char * ptr  = buf;
    const char * term = buf + bytesRead;
    while (ptr < term) {
        const char * linePtr = ptr;
        while (ptr < term && *ptr != '\n' && *ptr != '\r')
            ++ptr;

        std::string line(linePtr, int(ptr - linePtr));

        while (ptr < term && (*ptr == '\n' || *ptr == '\r'))
            ++ptr;

        int equalsIndex = (int)line.find_first_of('=');
        if (equalsIndex == -1 || line.length() < equalsIndex + 1)
            continue;

        std::string key(line.c_str(), equalsIndex);
        if (line[equalsIndex + 1] == '"') {
            int startIndex = equalsIndex + 2;
            int endIndex   = (int)line.find_first_of('"', startIndex);
            if (endIndex != -1)
                s_strings[key] = std::string(line.c_str() + startIndex, endIndex - startIndex);
        }
        else {
            int value = StrToInt(line.c_str() + equalsIndex + 1, nullptr, 10);
            s_values[key] = value;
        }
    }

  exit:
    if (buf)
        delete[] buf;
    if (fp)
        fclose(fp);
}

//===========================================================================
void ConfigSave() {
    char filename[MAX_PATH];
    GetConfigFilename(filename, ARRSIZE(filename));

    FILE * fp = fopen(filename, "w");
    if (!fp)
        goto exit;

    char buf[256];
    for (auto & pair : s_strings) {
        StrPrintf(buf, ARRSIZE(buf), "%s=\"%s\"\n", pair.first.c_str(), pair.second.c_str());
        fwrite(buf, StrLen(buf), 1, fp);
    }

    for (auto & pair : s_values) {
        StrPrintf(buf, ARRSIZE(buf), "%s=%d\n", pair.first.c_str(), pair.second);
        fwrite(buf, StrLen(buf), 1, fp);
    }

exit:
    if (fp)
        fclose(fp);
}

//===========================================================================
bool ConfigGetString(
    const char *    key,
    char *          buffer,
    int             bufferSize,
    const char *    defaultValue
) {
    auto val = s_strings.find(std::string(key));
    if (val == s_strings.end()) {
        StrCopy(buffer, defaultValue ? defaultValue : "", bufferSize);
        return false;
    }

    StrCopy(buffer, val->second.c_str(), bufferSize);
    return false;
}

//===========================================================================
bool ConfigGetValue(const char * key, int * value, int defaultValue) {
    auto val = s_values.find(std::string(key));
    if (val == s_values.end()) {
        *value = defaultValue;
        return false;
    }

    *value = val->second;
    return true;
}

//===========================================================================
void ConfigSetString(const char * key, const char * value) {
    s_strings[std::string(key)] = std::string(value);
}

//===========================================================================
void ConfigSetValue(const char * key, int value) {
    s_values[std::string(key)] = value;
}
