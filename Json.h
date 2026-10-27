#ifndef JSON_H
#define JSON_H

#include <cmath>
#include <iomanip>
#include <sstream>
#include <string>
#include <vector>

#include "IrrigationScheduler.h"

inline std::string jsonEscape(const std::string& s) {
    std::string out;
    for (char ch : s) {
        unsigned char c = static_cast<unsigned char>(ch);
        switch (c) {
            case '"': out += "\\\""; break;
            case '\\': out += "\\\\"; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default:
                if (c < 0x20) {
                    char buf[8];
                    std::snprintf(buf, sizeof(buf), "\\u%04x", c);
                    out += buf;
                } else {
                    out += ch;
                }
        }
    }
    return out;
}

inline std::string jnum(double v) {
    if (!std::isfinite(v)) return "-1";
    std::ostringstream o;
    o << std::fixed << std::setprecision(2) << v;
    return o.str();
}

inline std::string jstr(const std::string& s) { return "\"" + jsonEscape(s) + "\""; }

inline std::string zonesJson(const IrrigationScheduler& s) {
    std::ostringstream o;
    o << "{\"ok\":true,\"flowRate\":" << jnum(s.flowRate()) << ",\"zones\":[";
    const auto& zs = s.getZones();
    for (std::size_t i = 0; i < zs.size(); ++i) {
        const Zone& z = zs[i];
        if (i) o << ",";
        o << "{\"id\":" << z.id << ",\"name\":" << jstr(z.name) << ",\"crop\":" << jstr(z.crop)
          << ",\"moisture\":" << jnum(z.moisture) << ",\"target\":" << jnum(z.targetMoisture)
          << ",\"area\":" << jnum(z.areaM2) << ",\"rain\":" << jnum(z.rainForecastMm)
          << ",\"node\":" << z.nodeId << ",\"nodeName\":" << jstr(s.network().nodeName(z.nodeId))
          << ",\"effMoisture\":" << jnum(s.effectiveMoisture(z))
          << ",\"needLiters\":" << jnum(s.litersNeeded(z)) << "}";
    }
    o << "]}";
    return o.str();
}

inline std::string networkJson(const IrrigationScheduler& s) {
    const Graph& g = s.network();
    std::ostringstream o;
    o << "{\"ok\":true,\"source\":" << s.source() << ",\"nodes\":[";
    for (int i = 0; i < g.size(); ++i) {
        if (i) o << ",";
        o << "{\"id\":" << i << ",\"name\":" << jstr(g.nodeName(i)) << ",\"x\":" << g.nodeX(i)
          << ",\"y\":" << g.nodeY(i) << "}";
    }
    o << "],\"edges\":[";
    const auto& pipes = g.allPipes();
    for (std::size_t i = 0; i < pipes.size(); ++i) {
        if (i) o << ",";
        o << "{\"u\":" << pipes[i].u << ",\"v\":" << pipes[i].v << ",\"length\":" << jnum(pipes[i].length)
          << "}";
    }
    o << "]}";
    return o.str();
}

inline std::string scheduleJson(const ScheduleResult& r) {
    std::ostringstream o;
    o << "{\"ok\":true,\"budget\":" << jnum(r.budget) << ",\"flowRate\":" << jnum(r.flowRate)
      << ",\"totalSource\":" << jnum(r.totalSource) << ",\"totalDelivered\":" << jnum(r.totalDelivered)
      << ",\"lost\":" << jnum(r.totalSource - r.totalDelivered) << ",\"remaining\":" << jnum(r.remaining)
      << ",\"totalMinutes\":" << jnum(r.totalMinutes) << ",\"savedByForecast\":" << jnum(r.savedByForecast)
      << ",\"entries\":[";
    for (std::size_t i = 0; i < r.entries.size(); ++i) {
        const ScheduleEntry& e = r.entries[i];
        if (i) o << ",";
        o << "{\"order\":" << e.order << ",\"zoneId\":" << e.zoneId << ",\"name\":" << jstr(e.name)
          << ",\"crop\":" << jstr(e.crop) << ",\"moisture\":" << jnum(e.moisture)
          << ",\"effMoisture\":" << jnum(e.effMoisture) << ",\"target\":" << jnum(e.target)
          << ",\"distance\":" << jnum(e.distance) << ",\"need\":" << jnum(e.need)
          << ",\"delivered\":" << jnum(e.delivered) << ",\"fromSource\":" << jnum(e.fromSource)
          << ",\"startMin\":" << jnum(e.startMin) << ",\"runMin\":" << jnum(e.runMin)
          << ",\"status\":" << jstr(e.status) << ",\"path\":[";
        for (std::size_t k = 0; k < e.path.size(); ++k) {
            if (k) o << ",";
            o << e.path[k];
        }
        o << "]}";
    }
    o << "],\"skipped\":[";
    for (std::size_t i = 0; i < r.skipped.size(); ++i) {
        const SkippedZone& s = r.skipped[i];
        if (i) o << ",";
        o << "{\"zoneId\":" << s.zoneId << ",\"name\":" << jstr(s.name) << ",\"crop\":" << jstr(s.crop)
          << ",\"effMoisture\":" << jnum(s.effMoisture) << ",\"target\":" << jnum(s.target) << "}";
    }
    o << "]}";
    return o.str();
}

#endif
