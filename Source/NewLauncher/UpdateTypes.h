#pragma once

#include <string>

struct Arquivo
{
    int FileID = 0;
    std::string FileHash;
    std::string FilePath;
    bool ToUpdate = false;
};
