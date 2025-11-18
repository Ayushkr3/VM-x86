#include "Global.h"
extern bool running = false;
HostDatenTime get_host_time() {
    std::time_t now = std::time(nullptr);
    std::tm local_tm;
    localtime_s(&local_tm, &now);

    HostDatenTime time;


    time.hour = local_tm.tm_hour;
    time.minute = local_tm.tm_min;
    time.second = local_tm.tm_sec;
    int full_year = local_tm.tm_year + 1900;

    time.century = full_year / 100;
    time.year = full_year % 100;


    time.month = local_tm.tm_mon + 1;

    time.day = local_tm.tm_mday;
    return time;
}
uint64_t get_ms_since_midnight() {
    using namespace std::chrono;
    auto now = system_clock::now();
    std::time_t t = system_clock::to_time_t(now);
    std::tm local_tm{};
#ifdef _WIN32
    localtime_s(&local_tm, &t);
#else
    localtime_r(&t, &local_tm);
#endif
    uint64_t seconds_since_midnight =
        (uint64_t)local_tm.tm_hour * 3600ULL +
        (uint64_t)local_tm.tm_min * 60ULL +
        (uint64_t)local_tm.tm_sec;
    auto ms_part = duration_cast<milliseconds>(now.time_since_epoch()).count() % 1000ULL;

    return seconds_since_midnight * 1000ULL + ms_part;
}