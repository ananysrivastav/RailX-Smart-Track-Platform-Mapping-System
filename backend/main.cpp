// ============================================================
//  RailFlow - Smart Track & Platform Mapping System
//  SINGLE FILE VERSION
//  Compile:  g++ -std=c++17 -O2 -o railflow main.cpp
//  Run:      ./railflow.exe
// ============================================================

#include <iostream>
#include <vector>
#include <string>
#include <map>
#include <set>
#include <queue>
#include <limits>
#include <algorithm>
#include <fstream>
#include <sstream>
#include <iomanip>
#include <cmath>

// ============================================================
//  CLOCK UTILITY
//  Times stored internally as minutes from simulation start.
//  Base time = 6:00 AM
//  Example:  36 min = 6:36 AM  |  63 min = 7:03 AM
// ============================================================

const int BASE_HOUR   = 6;
const int BASE_MINUTE = 0;

std::string toClockStr(double sim_mins) {
    int total  = BASE_HOUR * 60 + BASE_MINUTE + (int)round(sim_mins);
    int h      = (total / 60) % 24;
    int m      =  total % 60;
    std::string period = (h < 12) ? "AM" : "PM";
    int dh = h % 12;
    if (dh == 0) dh = 12;
    std::ostringstream o;
    o << dh << ":" << std::setw(2) << std::setfill('0') << m << " " << period;
    return o.str();
}

// ============================================================
//  PRIORITY & TRAIN
// ============================================================

enum class Priority { LOW = 1, NORMAL = 2, HIGH = 3 };

std::string priorityToStr(Priority p) {
    if (p == Priority::HIGH)   return "HIGH";
    if (p == Priority::NORMAL) return "NORMAL";
    return "LOW";
}

struct Train {
    int         id;
    std::string name;
    double      speed;
    double      distance;
    Priority    priority;
    int         source_node;
    int         dest_node;

    double      eta;
    double      arrival_time;
    double      departure_time;
    double      stop_duration;

    int              platform_assigned;
    std::vector<int> route;
    bool             has_conflict;
    bool             rerouted;
    std::string      conflict_info;
    std::string      original_route_str;

    Train(int id, const std::string& name, double speed, double distance,
          Priority priority, int source_node)
        : id(id), name(name), speed(speed), distance(distance),
          priority(priority), source_node(source_node), dest_node(0),
          eta(0), arrival_time(0), departure_time(0), stop_duration(10),
          platform_assigned(-1), has_conflict(false), rerouted(false)
    {}
};

// ============================================================
//  GRAPH
// ============================================================

struct Edge {
    int         to;
    double      weight;
    std::string track_id;
};

struct Station {
    int         id;
    std::string name;
    double      x, y;
};

class RailwayGraph {
public:
    int                            num_nodes;
    std::vector<Station>           stations;
    std::vector<std::vector<Edge>> adj;

    RailwayGraph() : num_nodes(0) {}

    void addStation(int id, const std::string& name, double x, double y) {
        if (id >= (int)stations.size()) {
            stations.resize(id + 1);
            adj.resize(id + 1);
            num_nodes = (int)stations.size();
        }
        stations[id] = {id, name, x, y};
    }

    void addEdge(int u, int v, double weight) {
        std::string tid = "T" + std::to_string(u) + "-" + std::to_string(v);
        adj[u].push_back({v, weight, tid});
        adj[v].push_back({u, weight,
            "T" + std::to_string(v) + "-" + std::to_string(u)});
    }

    std::vector<int> dijkstra(int src, int dst,
                               std::vector<double>& dist_out) const {
        const double INF = std::numeric_limits<double>::infinity();
        dist_out.assign(num_nodes, INF);
        std::vector<int> prev(num_nodes, -1);
        using P = std::pair<double, int>;
        std::priority_queue<P, std::vector<P>, std::greater<P>> pq;
        dist_out[src] = 0.0;
        pq.push({0.0, src});
        while (!pq.empty()) {
            auto [d, u] = pq.top(); pq.pop();
            if (d > dist_out[u]) continue;
            if (u == dst) break;
            for (const auto& e : adj[u]) {
                double nd = dist_out[u] + e.weight;
                if (nd < dist_out[e.to]) {
                    dist_out[e.to] = nd;
                    prev[e.to] = u;
                    pq.push({nd, e.to});
                }
            }
        }
        std::vector<int> path;
        if (dist_out[dst] == INF) return path;
        for (int v = dst; v != -1; v = prev[v]) path.push_back(v);
        std::reverse(path.begin(), path.end());
        return path;
    }

    std::vector<int> dijkstraExclude(int src, int dst,
        const std::vector<std::pair<int,int>>& blocked,
        std::vector<double>& dist_out) const {
        const double INF = std::numeric_limits<double>::infinity();
        dist_out.assign(num_nodes, INF);
        std::vector<int> prev(num_nodes, -1);
        using P = std::pair<double, int>;
        std::priority_queue<P, std::vector<P>, std::greater<P>> pq;
        dist_out[src] = 0.0;
        pq.push({0.0, src});
        while (!pq.empty()) {
            auto [d, u] = pq.top(); pq.pop();
            if (d > dist_out[u]) continue;
            if (u == dst) break;
            for (const auto& e : adj[u]) {
                bool blk = false;
                for (auto& [bu, bv] : blocked)
                    if ((u==bu && e.to==bv)||(u==bv && e.to==bu)) {
                        blk = true; break;
                    }
                if (blk) continue;
                double nd = dist_out[u] + e.weight;
                if (nd < dist_out[e.to]) {
                    dist_out[e.to] = nd;
                    prev[e.to] = u;
                    pq.push({nd, e.to});
                }
            }
        }
        std::vector<int> path;
        if (dist_out[dst] == INF) return path;
        for (int v = dst; v != -1; v = prev[v]) path.push_back(v);
        std::reverse(path.begin(), path.end());
        return path;
    }

    void printGraph() const {
        std::cout << "\n[Graph] Station Network:\n";
        for (int i = 0; i < num_nodes; i++) {
            std::cout << "  [" << i << "] " << stations[i].name << " -> ";
            for (auto& e : adj[i])
                std::cout << stations[e.to].name << "(" << e.weight << "m) ";
            std::cout << "\n";
        }
    }

    static RailwayGraph buildDefault() {
        RailwayGraph g;
        g.addStation(0, "Main Station",   50, 50);
        g.addStation(1, "North Junction", 50, 25);
        g.addStation(2, "South Junction", 50, 75);
        g.addStation(3, "East Junction",  75, 50);
        g.addStation(4, "West Bypass",    25, 50);
        g.addStation(5, "North Entry",    50,  5);
        g.addStation(6, "South Entry",    50, 95);
        g.addStation(7, "East Entry",     95, 50);
        g.addEdge(5, 1,  8.0);
        g.addEdge(6, 2,  9.0);
        g.addEdge(7, 3,  6.0);
        g.addEdge(1, 0,  5.0);
        g.addEdge(2, 0,  6.0);
        g.addEdge(3, 0,  4.0);
        g.addEdge(4, 0,  7.0);
        g.addEdge(1, 4, 10.0);
        g.addEdge(2, 4, 11.0);
        g.addEdge(3, 1,  8.0);
        return g;
    }
};

// ============================================================
//  ETA CALCULATION
// ============================================================

void computeETAs(std::vector<Train>& trains) {
    std::cout << "\n[ETACalc] Computing ETAs...\n";
    for (auto& t : trains) {
        t.eta            = (t.distance / t.speed) * 60.0;
        t.arrival_time   = t.eta;
        t.departure_time = t.arrival_time + t.stop_duration;
        std::cout << "  Train " << t.id << " [" << t.name << "]"
                  << "  dist="  << t.distance << " km"
                  << "  speed=" << t.speed    << " km/h"
                  << "  ETA="   << std::fixed << std::setprecision(1) << t.eta << " min"
                  << "  Arrives: " << toClockStr(t.arrival_time) << "\n";
    }
}

// ============================================================
//  CONFLICT DETECTION
// ============================================================

struct TrackUsage {
    int         train_id, node_from, node_to;
    double      time_enter, time_exit;
    std::string track_id;
};

struct Conflict {
    int         train_a, train_b;
    std::string track_id;
    double      overlap_start, overlap_end;
    std::string description;
};

std::vector<TrackUsage> buildUsage(const Train& t, const RailwayGraph& g) {
    std::vector<TrackUsage> usage;
    double total = 0;
    for (int i = 0; i + 1 < (int)t.route.size(); i++) {
        int u = t.route[i], v = t.route[i+1];
        for (auto& e : g.adj[u]) if (e.to == v) { total += e.weight; break; }
    }
    double cur = t.arrival_time - total;
    for (int i = 0; i + 1 < (int)t.route.size(); i++) {
        int u = t.route[i], v = t.route[i+1];
        double seg = 0; std::string tid;
        for (auto& e : g.adj[u]) if (e.to == v) { seg = e.weight; tid = e.track_id; break; }
        usage.push_back({t.id, u, v, cur, cur + seg, tid});
        cur += seg;
    }
    return usage;
}

bool timeOverlaps(double a1, double a2, double b1, double b2, double buf = 2.0) {
    return (a1 - buf) < b2 && (b1 - buf) < a2;
}

bool sameTrack(int u1, int v1, int u2, int v2) {
    return (u1==u2 && v1==v2) || (u1==v2 && v1==u2);
}

std::vector<Conflict> detectConflicts(std::vector<Train>& trains,
                                       const RailwayGraph& g) {
    std::cout << "\n[ConflictDetector] Detecting Conflicts...\n";
    std::vector<std::vector<TrackUsage>> all_usage;
    for (auto& t : trains) all_usage.push_back(buildUsage(t, g));

    std::vector<Conflict> conflicts;
    for (int i = 0; i < (int)trains.size(); i++) {
        for (int j = i + 1; j < (int)trains.size(); j++) {
            for (auto& ua : all_usage[i]) {
                for (auto& ub : all_usage[j]) {
                    if (sameTrack(ua.node_from, ua.node_to,
                                   ub.node_from, ub.node_to)) {
                        if (timeOverlaps(ua.time_enter, ua.time_exit,
                                          ub.time_enter, ub.time_exit)) {
                            Conflict c;
                            c.train_a       = trains[i].id;
                            c.train_b       = trains[j].id;
                            c.track_id      = ua.track_id;
                            c.overlap_start = std::max(ua.time_enter, ub.time_enter);
                            c.overlap_end   = std::min(ua.time_exit,  ub.time_exit);

                            std::ostringstream oss;
                            oss << "Train " << trains[i].name
                                << " & "    << trains[j].name
                                << " conflict on " << ua.track_id
                                << " @ ["   << toClockStr(c.overlap_start)
                                << " - "    << toClockStr(c.overlap_end) << "]";
                            c.description = oss.str();
                            conflicts.push_back(c);
                            trains[i].has_conflict = true;
                            trains[j].has_conflict = true;
                            if (trains[i].conflict_info.empty())
                                trains[i].conflict_info = c.description;
                            if (trains[j].conflict_info.empty())
                                trains[j].conflict_info = c.description;
                            std::cout << "  WARNING: " << c.description << "\n";
                        }
                    }
                }
            }
        }
    }
    if (conflicts.empty()) std::cout << "  OK: No conflicts detected.\n";
    return conflicts;
}

// ============================================================
//  ROUTING ENGINE
// ============================================================

std::string pathStr(const std::vector<int>& path, const RailwayGraph& g) {
    std::string s;
    for (int i = 0; i < (int)path.size(); i++) {
        if (i) s += " -> ";
        s += g.stations[path[i]].name;
    }
    return s;
}

double routeCost(const std::vector<int>& path, const RailwayGraph& g) {
    double cost = 0;
    for (int i = 0; i + 1 < (int)path.size(); i++) {
        int u = path[i], v = path[i+1];
        for (auto& e : g.adj[u]) if (e.to == v) { cost += e.weight; break; }
    }
    return cost;
}

void assignInitialRoutes(std::vector<Train>& trains, const RailwayGraph& g) {
    std::cout << "\n[Routing] Assigning Initial Routes via Dijkstra...\n";
    for (auto& t : trains) {
        std::vector<double> dist;
        t.route              = g.dijkstra(t.source_node, t.dest_node, dist);
        t.original_route_str = pathStr(t.route, g);
        std::cout << "  Train " << t.id << " [" << t.name << "]  "
                  << pathStr(t.route, g)
                  << "  (cost=" << routeCost(t.route, g) << " min)\n";
    }
}

void resolveConflicts(std::vector<Train>& trains,
                      const std::vector<Conflict>& conflicts,
                      const RailwayGraph& g) {
    if (conflicts.empty()) return;
    std::cout << "\n[Routing] Resolving Conflicts...\n";
    std::set<int> done;

    for (auto& c : conflicts) {
        Train* ta = nullptr; Train* tb = nullptr;
        for (auto& t : trains) {
            if (t.id == c.train_a) ta = &t;
            if (t.id == c.train_b) tb = &t;
        }
        if (!ta || !tb) continue;

        Train* yield = nullptr;
        if ((int)ta->priority < (int)tb->priority)      yield = ta;
        else if ((int)tb->priority < (int)ta->priority) yield = tb;
        else yield = (ta->arrival_time > tb->arrival_time) ? ta : tb;

        if (done.count(yield->id)) continue;
        std::cout << "  Yielding: Train " << yield->name
                  << " (priority=" << priorityToStr(yield->priority) << ")\n";

        std::vector<std::pair<int,int>> blocked;
        for (auto& cf : conflicts) {
            if (cf.train_a == yield->id || cf.train_b == yield->id) {
                for (int i = 0; i + 1 < (int)yield->route.size(); i++) {
                    int ru = yield->route[i], rv = yield->route[i+1];
                    for (auto& e : g.adj[ru])
                        if (e.to == rv && e.track_id == cf.track_id)
                            blocked.push_back({ru, rv});
                }
            }
        }

        std::vector<double> dist;
        auto new_route = g.dijkstraExclude(
            yield->source_node, yield->dest_node, blocked, dist);

        if (!new_route.empty()) {
            double delay          = routeCost(new_route, g) - routeCost(yield->route, g);
            yield->route          = new_route;
            yield->arrival_time  += delay;
            yield->departure_time = yield->arrival_time + yield->stop_duration;
            yield->rerouted       = true;
            yield->has_conflict   = false;
            std::cout << "  REROUTED: Train " << yield->name
                      << " -> " << pathStr(yield->route, g)
                      << "  new arrival: " << toClockStr(yield->arrival_time) << "\n";
        } else {
            double max_overlap = 0;
            for (auto& cf : conflicts)
                if (cf.train_a == yield->id || cf.train_b == yield->id)
                    max_overlap = std::max(max_overlap,
                        cf.overlap_end - cf.overlap_start + 3.0);
            yield->arrival_time  += max_overlap;
            yield->departure_time = yield->arrival_time + yield->stop_duration;
            yield->rerouted       = true;
            yield->has_conflict   = false;
            std::cout << "  DELAYED:  Train " << yield->name
                      << "  new arrival: " << toClockStr(yield->arrival_time)
                      << "  (+" << max_overlap << " min)\n";
        }
        done.insert(yield->id);
    }
}

// ============================================================
//  PLATFORM ALLOCATION (GREEDY)
// ============================================================

struct Platform {
    int         id;
    std::string name;
    struct Window { double arrive, depart; int train_id; };
    std::vector<Window> reservations;
};

bool platformFree(const Platform& p, double arrive, double depart, double buf = 1.5) {
    for (auto& w : p.reservations)
        if ((arrive - buf) < w.depart && (w.arrive - buf) < depart) return false;
    return true;
}

void allocatePlatforms(std::vector<Train>& trains, std::vector<Platform>& platforms) {
    std::cout << "\n[PlatformAllocator] Assigning Platforms (Greedy)...\n";
    std::vector<Train*> sorted;
    for (auto& t : trains) sorted.push_back(&t);
    std::sort(sorted.begin(), sorted.end(),
        [](Train* a, Train* b){ return a->arrival_time < b->arrival_time; });

    for (auto* t : sorted) {
        bool assigned = false;
        std::vector<int> order;
        for (int i = 0; i < (int)platforms.size(); i++) order.push_back(i);
        if (t->priority == Priority::LOW) std::reverse(order.begin(), order.end());

        for (int pi : order) {
            if (platformFree(platforms[pi], t->arrival_time, t->departure_time)) {
                platforms[pi].reservations.push_back(
                    {t->arrival_time, t->departure_time, t->id});
                t->platform_assigned = platforms[pi].id;
                std::cout << "  OK: Train " << t->id << " [" << t->name << "]"
                          << "  Arrives: " << toClockStr(t->arrival_time)
                          << "  Departs: " << toClockStr(t->departure_time)
                          << "  -> " << platforms[pi].name << "\n";
                assigned = true; break;
            }
        }
        if (!assigned) {
            auto& p = platforms[0];
            double last_end = 0;
            for (auto& w : p.reservations) last_end = std::max(last_end, w.depart);
            double wait = last_end - t->arrival_time + 1.5;
            t->arrival_time  += wait;
            t->departure_time = t->arrival_time + t->stop_duration;
            p.reservations.push_back({t->arrival_time, t->departure_time, t->id});
            t->platform_assigned = p.id;
            std::cout << "  DELAYED: Train " << t->name
                      << "  new arrival: " << toClockStr(t->arrival_time)
                      << "  -> Platform-1\n";
        }
    }
}

void printSchedule(const std::vector<Platform>& platforms,
                   const std::vector<Train>&    trains) {
    std::map<int, std::string> nm;
    for (auto& t : trains) nm[t.id] = t.name;
    std::cout << "\n[Schedule] Final Platform Schedule:\n";
    for (auto& p : platforms) {
        std::cout << "  " << p.name << ":\n";
        auto res = p.reservations;
        std::sort(res.begin(), res.end(),
            [](auto& a, auto& b){ return a.arrive < b.arrive; });
        for (auto& w : res)
            std::cout << "    [" << toClockStr(w.arrive)
                      << "  to  " << toClockStr(w.depart) << "]"
                      << "  Train " << nm[w.train_id] << "\n";
        if (p.reservations.empty()) std::cout << "    (empty)\n";
    }
}

// ============================================================
//  JSON EXPORT
//  Exports raw minutes (arrive_mins/depart_mins) for timeline
//  math AND clock strings (arrive/depart) for display.
// ============================================================

std::string escJson(const std::string& s) {
    std::string o;
    for (char c : s) {
        if      (c == '"')  o += "\\\"";
        else if (c == '\\') o += "\\\\";
        else                o += c;
    }
    return o;
}

void exportJSON(const std::vector<Train>&    trains,
                const RailwayGraph&          g,
                const std::vector<Conflict>& conflicts,
                const std::vector<Platform>& platforms,
                const std::string&           filepath) {
    std::ofstream f(filepath);
    if (!f.is_open()) {
        std::cerr << "[JSON] ERROR: Cannot open " << filepath << "\n";
        return;
    }
    f << std::fixed << std::setprecision(2);
    f << "{\n";

    f << "  \"metadata\":{\n"
      << "    \"project\":\"RailFlow\","
      << "\"version\":\"1.0\","
      << "\"trains\":"    << trains.size()    << ","
      << "\"platforms\":" << platforms.size() << ","
      << "\"conflicts_detected\":" << conflicts.size() << "\n"
      << "  },\n";

    // Stations
    f << "  \"stations\":[\n";
    for (int i = 0; i < g.num_nodes; i++) {
        auto& s = g.stations[i];
        f << "    {\"id\":"    << s.id
          << ",\"name\":\""    << escJson(s.name) << "\""
          << ",\"x\":"         << s.x
          << ",\"y\":"         << s.y << "}";
        if (i + 1 < g.num_nodes) f << ",";
        f << "\n";
    }
    f << "  ],\n";

    // Edges
    f << "  \"edges\":[\n";
    std::set<std::string> seen; bool fe = true;
    for (int u = 0; u < g.num_nodes; u++) {
        for (auto& e : g.adj[u]) {
            std::string key = std::to_string(std::min(u,e.to)) + "-"
                            + std::to_string(std::max(u,e.to));
            if (seen.count(key)) continue; seen.insert(key);
            if (!fe) f << ",\n";
            f << "    {\"from\":" << u
              << ",\"to\":"   << e.to
              << ",\"weight\":" << e.weight
              << ",\"track_id\":\"" << escJson(e.track_id) << "\"}";
            fe = false;
        }
    }
    f << "\n  ],\n";

    // Trains
    f << "  \"trains\":[\n";
    for (int i = 0; i < (int)trains.size(); i++) {
        auto& t = trains[i];
        f << "    {\n"
          << "      \"id\":"               << t.id                                      << ",\n"
          << "      \"name\":\""           << escJson(t.name)                           << "\",\n"
          << "      \"speed\":"            << t.speed                                   << ",\n"
          << "      \"distance\":"         << t.distance                                << ",\n"
          << "      \"priority\":\""       << priorityToStr(t.priority)                 << "\",\n"
          << "      \"source_node\":"      << t.source_node                             << ",\n"
          << "      \"eta_mins\":"         << t.eta                                     << ",\n"
          << "      \"arrival_mins\":"     << t.arrival_time                            << ",\n"
          << "      \"departure_mins\":"   << t.departure_time                          << ",\n"
          << "      \"arrival_time\":\""   << escJson(toClockStr(t.arrival_time))       << "\",\n"
          << "      \"departure_time\":\"" << escJson(toClockStr(t.departure_time))     << "\",\n"
          << "      \"stop_duration\":"    << t.stop_duration                           << ",\n"
          << "      \"platform_assigned\":" << t.platform_assigned                      << ",\n"
          << "      \"has_conflict\":"     << (t.has_conflict ? "true" : "false")       << ",\n"
          << "      \"rerouted\":"         << (t.rerouted     ? "true" : "false")       << ",\n"
          << "      \"conflict_info\":\""  << escJson(t.conflict_info)                  << "\",\n"
          << "      \"original_route\":\"" << escJson(t.original_route_str)             << "\",\n"
          << "      \"route\":[";
        for (int j = 0; j < (int)t.route.size(); j++) { if (j) f << ","; f << t.route[j]; }
        f << "]\n    }";
        if (i + 1 < (int)trains.size()) f << ",";
        f << "\n";
    }
    f << "  ],\n";

    // Conflicts
    f << "  \"conflicts\":[\n";
    for (int i = 0; i < (int)conflicts.size(); i++) {
        auto& c = conflicts[i];
        f << "    {\n"
          << "      \"train_a\":"              << c.train_a                              << ",\n"
          << "      \"train_b\":"              << c.train_b                              << ",\n"
          << "      \"track_id\":\""           << escJson(c.track_id)                    << "\",\n"
          << "      \"overlap_start_mins\":"   << c.overlap_start                        << ",\n"
          << "      \"overlap_end_mins\":"     << c.overlap_end                          << ",\n"
          << "      \"overlap_start\":\""      << escJson(toClockStr(c.overlap_start))   << "\",\n"
          << "      \"overlap_end\":\""        << escJson(toClockStr(c.overlap_end))     << "\",\n"
          << "      \"description\":\""        << escJson(c.description)                 << "\"\n"
          << "    }";
        if (i + 1 < (int)conflicts.size()) f << ",";
        f << "\n";
    }
    f << "  ],\n";

    // Platforms
    f << "  \"platforms\":[\n";
    for (int i = 0; i < (int)platforms.size(); i++) {
        auto& p = platforms[i];
        f << "    {\"id\":" << p.id
          << ",\"name\":\"" << escJson(p.name) << "\""
          << ",\"reservations\":[";
        for (int j = 0; j < (int)p.reservations.size(); j++) {
            if (j) f << ",";
            auto& r = p.reservations[j];
            f << "{"
              << "\"train_id\":"    << r.train_id                     << ","
              << "\"arrive_mins\":" << r.arrive                       << ","
              << "\"depart_mins\":" << r.depart                       << ","
              << "\"arrive\":\""    << escJson(toClockStr(r.arrive))  << "\","
              << "\"depart\":\""    << escJson(toClockStr(r.depart))  << "\""
              << "}";
        }
        f << "]}";
        if (i + 1 < (int)platforms.size()) f << ",";
        f << "\n";
    }
    f << "  ]\n}\n";
    f.close();
    std::cout << "\n[JSON] Exported to: " << filepath << "\n";
}

// ============================================================
//  MAIN
// ============================================================

int main() {
    std::cout << "=========================================\n";
    std::cout << " RailFlow: Smart Track & Platform Mapping\n";
    std::cout << " Simulation base time: 6:00 AM\n";
    std::cout << "=========================================\n";

    RailwayGraph g = RailwayGraph::buildDefault();
    g.printGraph();

    std::vector<Train> trains = {
        Train(1, "Rajdhani-12",  110, 77.0, Priority::HIGH,   5),
        Train(2, "Shatabdi-04",  90,  63.0, Priority::HIGH,   6),
        Train(3, "Express-07",   70,  56.0, Priority::NORMAL, 5),
        Train(4, "Local-19",     50,  40.0, Priority::LOW,    6),
        Train(5, "Intercity-22", 100, 60.0, Priority::HIGH,   7),
        Train(6, "Freight-31",   40,  42.0, Priority::LOW,    7),
    };

    computeETAs(trains);
    assignInitialRoutes(trains, g);
    auto conflicts = detectConflicts(trains, g);
    resolveConflicts(trains, conflicts, g);

    std::vector<Platform> platforms;
    for (int i = 0; i < 4; i++)
        platforms.push_back({i, "Platform-" + std::to_string(i+1), {}});
    allocatePlatforms(trains, platforms);
    printSchedule(platforms, trains);

    // Final summary table
    std::cout << "\n=========================================\n";
    std::cout << " FINAL SUMMARY\n";
    std::cout << "=========================================\n";
    std::cout << std::left
              << std::setw(4)  << "ID"
              << std::setw(16) << "Name"
              << std::setw(8)  << "Priority"
              << std::setw(12) << "Arrives"
              << std::setw(12) << "Departs"
              << std::setw(10) << "Platform"
              << std::setw(10) << "Status"
              << "\n" << std::string(72, '-') << "\n";
    for (auto& t : trains) {
        std::cout << std::setw(4)  << t.id
                  << std::setw(16) << t.name
                  << std::setw(8)  << priorityToStr(t.priority)
                  << std::setw(12) << toClockStr(t.arrival_time)
                  << std::setw(12) << toClockStr(t.departure_time)
                  << std::setw(10) << ("P-" + std::to_string(t.platform_assigned + 1))
                  << std::setw(10) << (t.rerouted ? "DELAYED" : "On Time")
                  << "\n";
    }
    std::cout << "\nConflicts detected (pre-routing): " << conflicts.size() << "\n";

    exportJSON(trains, g, conflicts, platforms, "../data/output.json");
    std::cout << "\nDone! Open frontend/index.html to view the dashboard.\n";
    return 0;
}
