#include "simulation.hpp"
#include <algorithm>
#include <cmath>
#include <fstream>
#include <iomanip>
#include <limits>
#include <queue>
#include <sstream>
namespace tide {
const BuildingInfo &info(Building b) {
    static const BuildingInfo defs[] = {
        {"Inspect", "Select a tile to see its residents and services.", 0, 0, 0},
        {"Street", "Connect buildings to the town hall. Drag to paint.", 8, 0.04, 0},
        {"Cottage", "A home for 12 people. Needs a connected street.", 85, 0.4, 12},
        {"Farm", "Produces 32 food per day; winter harvests are smaller.", 120, 1.5, 8},
        {"Workshop", "Jobs for 24 people. Needs power; creates pollution.", 170, 2.5, 24},
        {"Windmill", "Clean power for 72 residents on the street network.", 220, 2.0, 72},
        {"Water tower", "Water for 90 residents on the street network.", 160, 1.8, 90},
        {"Garden", "Improves nearby homes within four tiles.", 65, 0.7, 0},
        {"Market", "12 jobs and trade income. Needs power and water.", 150, 1.8, 12},
        {"Clinic", "Improves wellbeing within six tiles; 8 jobs.", 240, 3.5, 8},
        {"Town hall", "The heart of the town and its street network.", 0, 0, 0}};
    int i = static_cast<int>(b);
    return defs[std::clamp(i, 0, 10)];
}
Simulation::Simulation(uint32_t s) : brain(s), rng(s) {
    reset(s);
}
void Simulation::reset(uint32_t s) {
    seed = s;
    rng.seed(s);
    brain = NeuralNetwork(s);
    day = 1;
    tax = 10;
    stableDays = debtDays = earnedMilestones = 0;
    won = lost = false;
    money = 1700;
    food = 180;
    relocations = 0;
    events.clear();
    history.clear();
    std::uniform_real_distribution<double> d(0, 1);
    for (int y = 0; y < MapSize; ++y)
        for (int x = 0; x < MapSize; ++x) {
            auto &t = tiles[index(x, y)];
            t = Tile{};
            double edge = std::min({x, y, MapSize - 1 - x, MapSize - 1 - y});
            double coast = 2.3 + std::sin(y * .48 + s * .01) * 1.2 + std::cos(x * .51) * 1.1;
            if (edge < coast || (x > 20 && y < 7))
                t.terrain = Terrain::Water;
            else {
                double v = d(rng);
                t.terrain = v < .18 ? Terrain::Forest : (v < .22 ? Terrain::Rock : Terrain::Grass);
            }
            if (x >= 8 && x <= 19 && y >= 8 && y <= 19)
                t.terrain = Terrain::Grass;
        }
    auto put = [&](int x, int y, Building b) {
        tiles[index(x, y)].building = b;
        tiles[index(x, y)].terrain = Terrain::Grass;
    };
    put(14, 14, Building::TownHall);
    for (int x = 9; x <= 18; ++x)
        if (x != 14)
            put(x, 14, Building::Road);
    for (int y = 10; y <= 18; ++y)
        if (y != 14)
            put(14, y, Building::Road);
    for (int x : {11, 12, 13}) {
        put(x, 13, Building::Home);
        tiles[index(x, 13)].people = {3, 3, 2};
    }
    put(15, 11, Building::Farm);
    put(15, 12, Building::Pump);
    put(16, 13, Building::Windmill);
    put(15, 15, Building::Workshop);
    put(13, 16, Building::Park);
    // Bootstrap from simulated preference experiences; all later updates are
    // collected in this town. Separate RNG preserves terrain/game randomness.
    std::mt19937 training(s ^ 0xBEEFu);
    for (int n = 0; n < 6000; ++n) {
        NeuralNetwork::Features f{};
        for (int i = 0; i < 9; ++i)
            f[i] = d(training);
        f[9 + n % 3] = 1;
        brain.train(f, NeuralNetwork::experiencedUtility(f), .07);
    }
    initialTraining = brain.samples;
    refresh();
    announce("Welcome to Tidemind. Grow a thriving town of 180 residents.");
    announce("Connect new cottages to streets, then balance food, water and jobs.");
    history.push_back({day, stats.population, money, stats.happiness});
}
void Simulation::announce(const std::string &s) {
    events.push_back(s);
    if (events.size() > 8)
        events.erase(events.begin());
}
int Simulation::count(Building b, bool only) const {
    int n = 0;
    for (auto &t : tiles)
        if (t.building == b && (!only || t.connected))
            ++n;
    return n;
}
bool Simulation::coastal(int i) const {
    int x = i % MapSize, y = i / MapSize;
    for (int dy = -3; dy <= 3; ++dy)
        for (int dx = -3; dx <= 3; ++dx)
            if (inside(x + dx, y + dy) && tiles[index(x + dx, y + dy)].terrain == Terrain::Water)
                return true;
    return false;
}
double Simulation::near(int i, Building b, int range) const {
    int x = i % MapSize, y = i / MapSize;
    double best = 0;
    for (int dy = -range; dy <= range; ++dy)
        for (int dx = -range; dx <= range; ++dx) {
            if (!inside(x + dx, y + dy))
                continue;
            auto &t = tiles[index(x + dx, y + dy)];
            int dist = std::abs(dx) + std::abs(dy);
            if (t.building == b && t.connected && dist <= range)
                best = std::max(best, 1.0 - double(dist) / (range + 1));
        }
    return best;
}
void Simulation::connect() {
    for (auto &t : tiles)
        t.connected = false;
    std::queue<int> q;
    for (int i = 0; i < TileCount; ++i)
        if (tiles[i].building == Building::TownHall) {
            q.push(i);
            tiles[i].connected = true;
        }
    while (!q.empty()) {
        int i = q.front();
        q.pop();
        int x = i % MapSize, y = i / MapSize;
        for (auto [dx, dy] : {std::pair{1, 0}, {-1, 0}, {0, 1}, {0, -1}}) {
            if (!inside(x + dx, y + dy))
                continue;
            int j = index(x + dx, y + dy);
            auto &t = tiles[j];
            if (t.connected || t.building == Building::None)
                continue;
            t.connected = true;
            if (t.building == Building::Road)
                q.push(j);
        }
    }
}
void Simulation::refresh() {
    connect();
    stats = Stats{};
    double happiness = 0;
    for (auto &t : tiles) {
        stats.population += t.population();
        happiness += t.happiness * t.population();
        if (t.building == Building::Home) {
            ++stats.homes;
            if (t.connected)
                stats.capacity += 12;
        }
        if (t.building != Building::None)
            stats.expenses += info(t.building).upkeep;
        if (!t.connected)
            continue;
        ++stats.connected;
        switch (t.building) {
        case Building::Pump:
            stats.water += 90;
            break;
        case Building::Windmill:
            stats.power += 72;
            break;
        case Building::Farm:
            stats.jobs += 8;
            break;
        case Building::Workshop:
            stats.jobs += 24;
            break;
        case Building::Market:
            stats.jobs += 12;
            break;
        case Building::Clinic:
            stats.jobs += 8;
            break;
        default:
            break;
        }
    }
    double need = std::max(1, stats.population);
    double water = std::min(1.0, stats.water / need), power = std::min(1.0, stats.power / need);
    stats.jobs = int(stats.jobs * (.35 + .65 * power));
    stats.foodProduction = count(Building::Farm, true) * 32.0 * (.3 + .7 * water) *
                           ((day - 1) / 30 % 4 == 3 ? .65 : 1.0);
    stats.foodUse = stats.population * .65;
    stats.happiness = stats.population ? happiness / stats.population : .5;
    stats.pollution = std::min(1.0, count(Building::Workshop, true) * .07);
    stats.income =
        12 + stats.population * tax * .025 + count(Building::Market, true) * 7 * water * power;
}
Result Simulation::canBuild(int x, int y, Building b) const {
    if (lost)
        return {false, "This town is insolvent. Start a new town to try again."};
    if (!inside(x, y))
        return {false, "Choose a tile on the island."};
    int kind = static_cast<int>(b);
    if (kind < 1 || kind > 9)
        return {false, "Choose a building first."};
    auto &t = tiles[index(x, y)];
    if (t.terrain == Terrain::Water)
        return {false, "Open water cannot be built on."};
    if (t.building != Building::None)
        return {false, "This tile is occupied. Inspect or demolish it first."};
    int clearing = t.terrain == Terrain::Forest ? 10 : (t.terrain == Terrain::Rock ? 20 : 0);
    if (money < info(b).cost + clearing)
        return {false, "Insufficient funds. Wait for income or remove unused buildings."};
    return {true, clearing ? "Includes clearing cost of " + std::to_string(clearing) + " coins."
                           : "Ready to build."};
}
Result Simulation::build(int x, int y, Building b) {
    auto r = canBuild(x, y, b);
    if (!r.ok)
        return r;
    auto &t = tiles[index(x, y)];
    money -=
        info(b).cost + (t.terrain == Terrain::Forest ? 10 : (t.terrain == Terrain::Rock ? 20 : 0));
    t.terrain = Terrain::Grass;
    t.building = b;
    refresh();
    return {true, std::string(info(b).name) + " built." +
                      (t.connected ? "" : " Connect it to a street.")};
}
Result Simulation::demolish(int x, int y) {
    if (!inside(x, y))
        return {false, "Choose a tile on the island."};
    auto &t = tiles[index(x, y)];
    if (t.building == Building::TownHall)
        return {false, "The town hall cannot be demolished."};
    if (t.building == Building::None)
        return {false, "There is no building here."};
    int refund = info(t.building).cost / 3;
    money += refund;
    if (t.population())
        announce(std::to_string(t.population()) + " displaced residents left town.");
    t.building = Building::None;
    t.people = {};
    t.happiness = .5;
    refresh();
    return {true, "Removed. Recovered " + std::to_string(refund) + " coins."};
}
NeuralNetwork::Features Simulation::features(int i, int cohort) const {
    NeuralNetwork::Features f{};
    if (i < 0 || i >= TileCount)
        return f;
    auto &t = tiles[i];
    double n = std::max(1, stats.population);
    f[0] = t.connected;
    f[1] = t.connected ? std::min(1.0, stats.water / n) : 0;
    f[2] = t.connected ? std::min(1.0, stats.power / n) : 0;
    f[3] = std::min(1.0, near(i, Building::Park, 4) * 1.5 + near(i, Building::Clinic, 6) * .55);
    f[4] = std::min(1.0, stats.jobs / n);
    f[5] = near(i, Building::Workshop, 4);
    f[6] = (tax - 5) / 15.0;
    f[7] = std::min(1.0, food / std::max(1.0, stats.foodUse * 3));
    f[8] = coastal(i) ? 1 : 0;
    f[9 + std::clamp(cohort, 0, 2)] = 1;
    return f;
}
double Simulation::appeal(int i, int cohort) const {
    return brain.predict(features(i, cohort));
}
void Simulation::tick() {
    if (lost)
        return;
    ++day;
    refresh();
    food = std::clamp(food + stats.foodProduction - stats.foodUse, 0.0, 3000.0);
    money += stats.income - stats.expenses;
    for (int i = 0; i < TileCount; ++i) {
        auto &t = tiles[i];
        if (t.building != Building::Home)
            continue;
        double weighted = 0;
        for (int c = 0; c < 3; ++c) {
            auto f = features(i, c);
            double utility = NeuralNetwork::experiencedUtility(f);
            if (t.people[c]) {
                brain.train(f, utility);
                weighted += utility * t.people[c];
            }
        }
        t.happiness = t.population() ? weighted / t.population() : appeal(i, 0);
        if (t.population() && (!t.connected || t.happiness < .36 || food < .01)) {
            for (int c = 0; c < 3; ++c)
                if (t.people[c] > 0) {
                    --t.people[c];
                    break;
                }
        }
    }
    refresh();
    // Inference directly selects arrivals' homes. One resident may also move
    // each day when the learned destination score improves by at least 0.05.
    for (int c = 0; c < 3; ++c) {
        int best = -1;
        double score = .48;
        for (int i = 0; i < TileCount; ++i) {
            auto &t = tiles[i];
            if (t.building != Building::Home || !t.connected || t.population() >= 12)
                continue;
            double p = appeal(i, c);
            if (p > score) {
                best = i;
                score = p;
            }
        }
        if (best < 0)
            continue;
        if (food > stats.foodUse && money > -200 && stats.population < stats.jobs * 1.6 + 12) {
            ++tiles[best].people[c];
            ++stats.population;
        }
        for (int i = 0; i < TileCount; ++i)
            if (i != best && tiles[i].people[c] > 0 && tiles[best].population() < 12 &&
                appeal(i, c) + .05 < score) {
                --tiles[i].people[c];
                ++tiles[best].people[c];
                ++relocations;
                break;
            }
    }
    refresh();
    int m = milestone();
    if (m > earnedMilestones) {
        money += (m - earnedMilestones) * 250;
        earnedMilestones = m;
        announce(m == 1   ? "Village charter: 40 residents! A 250 coin grant has arrived."
                 : m == 2 ? "Growing town: 100 residents! Another 250 coin grant."
                          : "180 residents! Keep the town thriving for 10 days.");
    }
    bool sustainable = stats.population >= 180 && stats.happiness >= .65 &&
                       stats.foodProduction >= stats.foodUse && stats.water >= stats.population &&
                       stats.power >= stats.population && stats.income >= stats.expenses &&
                       money >= 0;
    stableDays = sustainable ? stableDays + 1 : 0;
    if (stableDays >= 10 && !won) {
        won = true;
        announce("A thriving Tidemind! Your town has earned its independence. Keep building.");
    }
    debtDays = money < -500 ? debtDays + 1 : 0;
    if (debtDays >= 10) {
        lost = true;
        announce("The treasury is insolvent. Your town's story ends here. Start a new island.");
    }
    if (day % 30 == 1)
        announce(std::string(season()) + " has arrived." +
                 (std::string(season()) == "Winter" ? " Farms harvest 35% less food." : ""));
    history.push_back({day, stats.population, money, stats.happiness});
    if (history.size() > 180)
        history.erase(history.begin());
}
int Simulation::milestone() const {
    return stats.population >= 180   ? 3
           : stats.population >= 100 ? 2
           : stats.population >= 40  ? 1
                                     : 0;
}
const char *Simulation::season() const {
    static const char *s[] = {"Spring", "Summer", "Autumn", "Winter"};
    return s[(day - 1) / 30 % 4];
}
std::string Simulation::advice() const {
    if (lost)
        return "The town is insolvent. Start a new island from the pause menu.";
    if (stats.water < stats.population)
        return "Water is scarce. Add a connected water tower.";
    if (stats.power < stats.population)
        return "Power is strained. Add a connected windmill.";
    if (stats.foodProduction < stats.foodUse)
        return "Food reserves are falling. Build another farm before winter.";
    if (stats.jobs < stats.population * .8)
        return "Residents want work. Add a workshop or a market.";
    if (stats.capacity - stats.population < 9)
        return "Your town is filling up. Connect more cottages to the street network.";
    if (stats.happiness < .65)
        return "Build gardens near homes and keep workshops at a distance.";
    if (count(Building::Market, true) == 0)
        return "A market will create jobs and bring in extra trade income.";
    return "The town is balanced. Expand housing and services together; prepare extra food for "
           "winter.";
}
Result Simulation::save(const std::filesystem::path &path) const {
    try {
        if (path.has_parent_path())
            std::filesystem::create_directories(path.parent_path());
        auto tmp = path;
        tmp += ".tmp";
        std::ofstream out(tmp, std::ios::trunc);
        if (!out)
            return {false, "Cannot open save file."};
        out << std::setprecision(std::numeric_limits<double>::max_digits10);
        out << "TIDEMIND 1\n"
            << seed << ' ' << day << ' ' << tax << ' ' << stableDays << ' ' << debtDays << ' '
            << earnedMilestones << ' ' << won << ' ' << lost << ' ' << money << ' ' << food << ' '
            << initialTraining << ' ' << relocations << '\n';
        out << rng << '\n';
        brain.write(out);
        for (auto &t : tiles)
            out << int(t.terrain) << ' ' << int(t.building) << ' ' << t.people[0] << ' '
                << t.people[1] << ' ' << t.people[2] << ' ' << t.happiness << '\n';
        out << events.size() << '\n';
        for (auto &s : events)
            out << std::quoted(s) << '\n';
        out << history.size() << '\n';
        for (auto &h : history)
            out << h.day << ' ' << h.population << ' ' << h.money << ' ' << h.happiness << '\n';
        out.flush();
        if (!out)
            return {false, "Could not finish saving."};
        out.close();
        std::filesystem::rename(tmp, path);
        return {true, "Town and learned memories saved."};
    } catch (const std::exception &) {
        return {false, "Save failed. Check the save folder's permissions."};
    }
}
Result Simulation::load(const std::filesystem::path &path) {
    auto bad = []() {
        return Result{
            false, "Save is missing, damaged or from an unsupported version. Current town kept."};
    };
    try {
        if (!std::filesystem::exists(path) || std::filesystem::file_size(path) > 2000000)
            return bad();
        std::ifstream in(path);
        std::string magic;
        int version;
        if (!(in >> magic >> version) || magic != "TIDEMIND" || version != 1)
            return bad();
        Simulation s(1);
        if (!(in >> s.seed >> s.day >> s.tax >> s.stableDays >> s.debtDays >> s.earnedMilestones >>
              s.won >> s.lost >> s.money >> s.food >> s.initialTraining >> s.relocations))
            return bad();
        if (s.day < 1 || s.day > 100000000 || s.tax < 5 || s.tax > 20 || s.stableDays < 0 ||
            s.stableDays > s.day || s.debtDays < 0 || s.debtDays > s.day ||
            s.earnedMilestones < 0 || s.earnedMilestones > 3 || !std::isfinite(s.money) ||
            std::abs(s.money) > 1e12 || !std::isfinite(s.food) || s.food < 0 || s.food > 3000)
            return bad();
        if (!(in >> s.rng) || !s.brain.read(in) || s.initialTraining > s.brain.samples)
            return bad();
        int halls = 0;
        for (auto &t : s.tiles) {
            int terrain, b;
            if (!(in >> terrain >> b >> t.people[0] >> t.people[1] >> t.people[2] >> t.happiness) ||
                terrain < 0 || terrain > 3 || b < 0 || b > 10 || !std::isfinite(t.happiness) ||
                t.happiness < 0 || t.happiness > 1)
                return bad();
            for (int p : t.people)
                if (p < 0 || p > 12)
                    return bad();
            if (t.population() > 12 || (b != 2 && t.population()) || (terrain == 2 && b != 0))
                return bad();
            t.terrain = Terrain(terrain);
            t.building = Building(b);
            halls += b == 10;
        }
        if (halls != 1)
            return bad();
        size_t n;
        if (!(in >> n) || n > 8)
            return bad();
        s.events.clear();
        for (size_t i = 0; i < n; ++i) {
            std::string msg;
            if (!(in >> std::quoted(msg)) || msg.size() > 1000)
                return bad();
            s.events.push_back(msg);
        }
        if (!(in >> n) || n > 180)
            return bad();
        s.history.clear();
        for (size_t i = 0; i < n; ++i) {
            History h;
            if (!(in >> h.day >> h.population >> h.money >> h.happiness) || h.day < 1 ||
                h.day > s.day || h.population < 0 || h.population > TileCount * 12 ||
                !std::isfinite(h.money) || !std::isfinite(h.happiness) || h.happiness < 0 ||
                h.happiness > 1)
                return bad();
            s.history.push_back(h);
        }
        in >> std::ws;
        if (!in.eof())
            return bad();
        s.refresh();
        *this = std::move(s);
        return {true, "Town restored, including learned memories."};
    } catch (const std::exception &) {
        return bad();
    }
}
} // namespace tide
