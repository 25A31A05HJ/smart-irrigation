// Smart Agriculture - Irrigation Scheduler (SDG 2 & SDG 6)
// BACKEND: C++ HTTP server + REST API.  FRONTEND: public/index.html
//
// DSA used:  Greedy (driest zone first)  |  Min-Heap (priority list)  |  Graph + Dijkstra (pipe network)

#include <cmath>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

#include "Graph.h"
#include "HttpServer.h"
#include "IrrigationScheduler.h"
#include "Json.h"
#include "Zone.h"

// ---------- sample farm ----------
static Graph buildFarmNetwork(int& sourceNode) {
    Graph g;
    //                  name         x    y   (x,y only used to draw the map)
    sourceNode = g.addNode("WaterTank", 60, 180);  // 0
    int j1 = g.addNode("Junction1", 170, 180);     // 1
    int j2 = g.addNode("Junction2", 290, 90);      // 2
    int j3 = g.addNode("Junction3", 290, 270);     // 3
    g.addNode("OutletA", 420, 40);                 // 4
    g.addNode("OutletB", 420, 170);                // 5
    g.addNode("OutletC", 420, 300);                // 6
    g.addNode("OutletD", 550, 100);                // 7
    g.addNode("OutletE", 550, 250);                // 8
    g.addNode("OutletF", 660, 180);                // 9

    g.addPipe(sourceNode, j1, 50);
    g.addPipe(j1, j2, 80);
    g.addPipe(j1, j3, 120);
    g.addPipe(j2, j3, 150);
    g.addPipe(j2, 4, 40);
    g.addPipe(j2, 5, 60);
    g.addPipe(j3, 5, 30);
    g.addPipe(j3, 6, 70);
    g.addPipe(5, 7, 90);
    g.addPipe(6, 8, 50);
    g.addPipe(7, 8, 40);
    g.addPipe(8, 9, 60);
    return g;
}

static void loadSampleZones(IrrigationScheduler& s) {
    //          id  name             crop        moist target area    rain node
    s.addZone({1, "North Field",   "Wheat",     22.0, 45.0, 1200.0, 0.0, 4});
    s.addZone({2, "East Orchard",  "Mango",     35.0, 50.0,  900.0, 0.0, 5});
    s.addZone({3, "South Paddy",   "Rice",      30.0, 70.0, 1500.0, 2.0, 6});
    s.addZone({4, "West Veggies",  "Tomato",    18.0, 55.0,  600.0, 0.0, 7});
    s.addZone({5, "Hill Plot",     "Maize",     48.0, 50.0,  800.0, 0.0, 8});
    s.addZone({6, "River Bed",     "Sugarcane", 40.0, 60.0, 1000.0, 15.0, 9});
}

// ---------- helpers ----------
static HttpResponse jsonResponse(int status, const std::string& body) {
    HttpResponse r;
    r.status = status;
    r.contentType = "application/json; charset=utf-8";
    r.body = body;
    return r;
}

static HttpResponse errorResponse(int status, const std::string& msg) {
    return jsonResponse(status, "{\"ok\":false,\"error\":" + jstr(msg) + "}");
}

static bool getNum(const std::map<std::string, std::string>& m, const std::string& key, double lo,
                   double hi, double& out) {
    auto it = m.find(key);
    if (it == m.end() || it->second.empty()) return false;
    char* end = nullptr;
    double v = std::strtod(it->second.c_str(), &end);
    if (*end != '\0' || !std::isfinite(v) || v < lo || v > hi) return false;
    out = v;
    return true;
}

static bool loadIndexHtml(std::string& html) {
    const char* candidates[] = {"public/index.html", "../public/index.html", "index.html"};
    for (const char* path : candidates) {
        std::ifstream f(path, std::ios::binary);
        if (f) {
            std::ostringstream ss;
            ss << f.rdbuf();
            html = ss.str();
            return true;
        }
    }
    return false;
}

// ---------- routing ----------
static HttpResponse handleRequest(IrrigationScheduler& sched, const HttpRequest& req) {
    const bool isGet = req.method == "GET";
    const bool isPost = req.method == "POST";

    if (isGet && (req.path == "/" || req.path == "/index.html")) {
        std::string html;
        if (!loadIndexHtml(html)) {
            HttpResponse r;
            r.status = 500;
            r.body = "public/index.html not found. Run the server from the irrigation_web folder.";
            return r;
        }
        HttpResponse r;
        r.contentType = "text/html; charset=utf-8";
        r.body = html;
        return r;
    }

    if (isGet && req.path == "/api/zones") return jsonResponse(200, zonesJson(sched));
    if (isGet && req.path == "/api/network") return jsonResponse(200, networkJson(sched));

    if (isGet && req.path == "/api/schedule") {
        double budget = 0;
        if (!getNum(req.query, "budget", 0, 10000000, budget))
            return errorResponse(400, "budget must be a number between 0 and 10000000 litres");
        return jsonResponse(200, scheduleJson(sched.computeSchedule(budget)));
    }

    if (isPost && req.path == "/api/update") {
        double id = 0, moisture = 0, rain = 0;
        if (!getNum(req.form, "id", 1, 1000000, id)) return errorResponse(400, "invalid zone id");
        if (!getNum(req.form, "moisture", 0, 100, moisture))
            return errorResponse(400, "moisture must be between 0 and 100");
        if (!getNum(req.form, "rain", 0, 200, rain))
            return errorResponse(400, "rain must be between 0 and 200 mm");
        Zone* z = sched.findZone(static_cast<int>(id));
        if (z == nullptr) return errorResponse(404, "zone not found");
        z->moisture = moisture;
        z->rainForecastMm = rain;
        return jsonResponse(200, zonesJson(sched));
    }

    if (isPost && req.path == "/api/add") {
        double moisture = 0, target = 0, area = 0, rain = 0, node = 0;
        auto n = req.form.find("name");
        auto c = req.form.find("crop");
        if (n == req.form.end() || n->second.empty() || n->second.size() > 30)
            return errorResponse(400, "zone name is required (max 30 characters)");
        if (c == req.form.end() || c->second.empty() || c->second.size() > 30)
            return errorResponse(400, "crop is required (max 30 characters)");
        if (!getNum(req.form, "moisture", 0, 100, moisture)) return errorResponse(400, "moisture must be 0-100");
        if (!getNum(req.form, "target", 1, 100, target)) return errorResponse(400, "target must be 1-100");
        if (!getNum(req.form, "area", 1, 1000000, area)) return errorResponse(400, "area must be 1-1000000");
        if (!getNum(req.form, "rain", 0, 200, rain)) return errorResponse(400, "rain must be 0-200");
        if (!getNum(req.form, "node", 0, 1000, node)) return errorResponse(400, "invalid outlet node");
        Zone z{sched.nextZoneId(), n->second, c->second, moisture, target, area, rain, static_cast<int>(node)};
        if (!sched.addZone(z)) return errorResponse(400, "could not add zone (bad outlet node?)");
        return jsonResponse(200, zonesJson(sched));
    }

    if (isPost && req.path == "/api/delete") {
        double id = 0;
        if (!getNum(req.form, "id", 1, 1000000, id)) return errorResponse(400, "invalid zone id");
        if (!sched.removeZone(static_cast<int>(id))) return errorResponse(404, "zone not found");
        return jsonResponse(200, zonesJson(sched));
    }

    if (isPost && req.path == "/api/reset") {
        sched.clearZones();
        loadSampleZones(sched);
        return jsonResponse(200, zonesJson(sched));
    }

    return errorResponse(404, "not found");
}

int main(int argc, char** argv) {
    int port = 8080;
    if (argc > 1) {
        port = std::atoi(argv[1]);
        if (port < 1 || port > 65535) {
            std::cerr << "Port must be between 1 and 65535\n";
            return 1;
        }
    }

    int source = 0;
    Graph farm = buildFarmNetwork(source);
    IrrigationScheduler scheduler(farm, source, 60.0);  // 60 L/min pump
    loadSampleZones(scheduler);

    HttpServer server(port);
    bool ok = server.run([&scheduler](const HttpRequest& req) { return handleRequest(scheduler, req); });
    return ok ? 0 : 1;
}
