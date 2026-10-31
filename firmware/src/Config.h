#pragma once
#include <stdint.h>

namespace Config {
constexpr const char* Version = "0.6.0";

constexpr int ScreenW = 240;
constexpr int ScreenH = 135;
constexpr uint32_t FrameUs = 33000;   // about 30 frames a second

// Passive scan: the Deck only listens for the beacons routers send about
// ten times a second, and never sends anything. 150 ms on each of the 13
// channels catches at least one beacon from each router in range.
constexpr uint32_t ScanMsPerChannel = 150;
constexpr uint32_t ScanGapMs = 400;   // pause between scans
}
