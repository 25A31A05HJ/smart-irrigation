#ifndef IRRIGATION_SCHEDULER_H
#define IRRIGATION_SCHEDULER_H

#include <algorithm>
#include <string>
#include <vector>

#include "Graph.h"
#include "MinHeap.h"
#include "Zone.h"

// One pending irrigation job stored in the Min-Heap.
struct IrrigationTask {
    int zoneId;
    double effMoisture;  // moisture after counting forecast rain (%)
    double deficit;      // target - effMoisture (%)
};

// Greedy priority: driest zone first; tie -> bigger deficit; tie -> lower id.
struct TaskCompare {
    bool operator()(const IrrigationTask& a, const IrrigationTask& b) const {
        if (a.effMoisture != b.effMoisture) return a.effMoisture < b.effMoisture;
        if (a.deficit != b.deficit) return a.deficit > b.deficit;
        return a.zoneId < b.zoneId;
    }
};

struct ScheduleEntry {
    int order = 0;
    int zoneId = 0;
    std::string name;
    std::string crop;
    double moisture = 0, effMoisture = 0, target = 0;
    double distance = -1;  // pipe distance from tank (m), -1 if unreachable
    double need = 0, delivered = 0, fromSource = 0;
    double startMin = 0, runMin = 0;
    std::string status;
    std::vector<int> path;  // node ids from tank to the zone outlet
};

struct SkippedZone {
    int zoneId = 0;
    std::string name;
    std::string crop;
    double effMoisture = 0, target = 0;
};

struct ScheduleResult {
    double budget = 0, flowRate = 0;
    double totalSource = 0, totalDelivered = 0, remaining = 0;
    double totalMinutes = 0, savedByForecast = 0;
    std::vector<ScheduleEntry> entries;
    std::vector<SkippedZone> skipped;
};

class IrrigationScheduler {
private:
    static constexpr double RAIN_TO_MOISTURE = 1.5;    // % moisture gained per mm rain
    static constexpr double LITERS_PER_PCT_M2 = 0.1;   // litres per (1 % deficit * 1 m^2)
    static constexpr double PIPE_LOSS_PER_M = 0.0004;  // fraction of water lost per metre
    static constexpr double MAX_PIPE_LOSS = 0.5;       // cap on total loss

    std::vector<Zone> zones;
    Graph graph;
    int sourceNode;
    double flowRateLpm;  // pump flow in litres per minute

    double litersFor(double deficit, const Zone& z) const {
        return std::max(0.0, deficit) * z.areaM2 * LITERS_PER_PCT_M2;
    }

    const Zone* findConst(int id) const {
        for (const auto& z : zones)
            if (z.id == id) return &z;
        return nullptr;
    }

public:
    IrrigationScheduler(const Graph& g, int source, double flowLpm)
        : graph(g), sourceNode(source), flowRateLpm(flowLpm > 0 ? flowLpm : 50.0) {}

    // ---------- accessors ----------
    const std::vector<Zone>& getZones() const { return zones; }
    const Graph& network() const { return graph; }
    int source() const { return sourceNode; }
    double flowRate() const { return flowRateLpm; }

    double effectiveMoisture(const Zone& z) const {
        return std::min(100.0, z.moisture + z.rainForecastMm * RAIN_TO_MOISTURE);
    }
    double litersNeeded(const Zone& z) const {
        return litersFor(z.targetMoisture - effectiveMoisture(z), z);
    }

    // ---------- zone management ----------
    int nextZoneId() const {
        int m = 0;
        for (const auto& z : zones) m = std::max(m, z.id);
        return m + 1;
    }

    bool addZone(const Zone& z) {
        if (!graph.validNode(z.nodeId) || z.nodeId == sourceNode) return false;
        if (findConst(z.id) != nullptr) return false;
        if (z.areaM2 <= 0) return false;
        zones.push_back(z);
        return true;
    }

    Zone* findZone(int id) {
        for (auto& z : zones)
            if (z.id == id) return &z;
        return nullptr;
    }

    bool removeZone(int id) {
        for (std::size_t i = 0; i < zones.size(); ++i) {
            if (zones[i].id == id) {
                zones.erase(zones.begin() + static_cast<std::ptrdiff_t>(i));
                return true;
            }
        }
        return false;
    }

    void clearZones() { zones.clear(); }

    // ---------- the scheduling algorithm ----------
    // Greedy (driest first) + Min-Heap (priority list) + Graph (Dijkstra route).
    ScheduleResult computeSchedule(double budgetLiters) const {
        ScheduleResult r;
        r.budget = budgetLiters;
        r.flowRate = flowRateLpm;

        Graph::ShortestPaths sp = graph.dijkstra(sourceNode);
        MinHeap<IrrigationTask, TaskCompare> heap;

        for (const auto& z : zones) {
            double eff = effectiveMoisture(z);
            double deficit = z.targetMoisture - eff;
            double naive = z.targetMoisture - z.moisture;
            r.savedByForecast += litersFor(naive, z) - litersFor(deficit, z);
            if (deficit > 0.0) {
                heap.push({z.id, eff, deficit});
            } else {
                SkippedZone s;
                s.zoneId = z.id;
                s.name = z.name;
                s.crop = z.crop;
                s.effMoisture = eff;
                s.target = z.targetMoisture;
                r.skipped.push_back(s);
            }
        }

        double remaining = budgetLiters;
        double clockMin = 0.0;
        int order = 0;

        while (!heap.empty()) {
            IrrigationTask t = heap.pop();  // greedy: driest zone first
            const Zone* z = findConst(t.zoneId);
            if (z == nullptr) continue;

            ScheduleEntry e;
            e.order = ++order;
            e.zoneId = z->id;
            e.name = z->name;
            e.crop = z->crop;
            e.moisture = z->moisture;
            e.effMoisture = t.effMoisture;
            e.target = z->targetMoisture;
            e.need = litersFor(t.deficit, *z);
            e.startMin = clockMin;

            double dist = sp.dist[z->nodeId];
            e.path = graph.buildPath(sp, z->nodeId);

            if (dist == Graph::INF) {
                e.status = "UNREACHABLE";
            } else {
                e.distance = dist;
                if (remaining <= 0.0) {
                    e.status = "DEFERRED";
                } else {
                    double loss = std::min(MAX_PIPE_LOSS, dist * PIPE_LOSS_PER_M);
                    double requiredAtSource = e.need / (1.0 - loss);
                    if (requiredAtSource <= remaining) {
                        e.fromSource = requiredAtSource;
                        e.delivered = e.need;
                        e.status = "FULL";
                    } else {
                        e.fromSource = remaining;
                        e.delivered = remaining * (1.0 - loss);
                        e.status = "PARTIAL";
                    }
                    remaining -= e.fromSource;
                    e.runMin = e.delivered / flowRateLpm;
                    clockMin += e.runMin;
                    r.totalSource += e.fromSource;
                    r.totalDelivered += e.delivered;
                }
            }
            r.entries.push_back(e);
        }

        r.remaining = remaining;
        r.totalMinutes = clockMin;
        return r;
    }
};

#endif
