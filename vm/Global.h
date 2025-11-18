#pragma once
#include <iostream>
#include <chrono>
#include <Windows.h>
extern bool running;
struct HostDatenTime {
	int hour;
	int minute;
	int second;
	int century;
	int year;
	int month;
	int day;
};
uint64_t get_ms_since_midnight();
HostDatenTime get_host_time();