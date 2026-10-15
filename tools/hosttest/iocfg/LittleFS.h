#pragma once
#include "FS.h"
namespace fs { class LittleFSFS: public FS {}; }
extern fs::LittleFSFS LittleFS;
