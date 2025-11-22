#pragma once
#include <iostream>
#include <fstream>
#include <string>
#include <Windows.h>
#include "CPU.h"
#include "table.h"
#include "xed_table.h"
#include "io.h"
extern "C" {
#include "unicorn.h"
}
#include "unic.h"
#include "Global.h"

void EmulationLoop(UnicornData* ud);
UnicornData* SetupArch();