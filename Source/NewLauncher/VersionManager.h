#pragma once

class VersionManager
{
public:
    static int Load(const char* path = "version.dat", int defaultVersion = 1);
    static bool Save(int version, const char* path = "version.dat");
};
