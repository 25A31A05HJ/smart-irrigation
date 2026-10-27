#ifndef ZONE_H
#define ZONE_H

#include <string>

// One irrigable part of the farm.
struct Zone {
    int id;                 // unique zone id (1, 2, 3 ...)
    std::string name;       // e.g. "North Field"
    std::string crop;       // e.g. "Wheat"
    double moisture;        // current soil moisture (%)
    double targetMoisture;  // moisture the crop needs (%)
    double areaM2;          // zone area in square metres
    double rainForecastMm;  // forecast rainfall in the next 24h (mm)
    int nodeId;             // node of this zone in the pipe-network graph
};

#endif
