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
#define RAM_SIZE 2147483648

void EmulationLoop(UnicornData* ud);
UnicornData* SetupArch();