#ifndef TIDEMIND_SIMULATION_HPP
#define TIDEMIND_SIMULATION_HPP
#include "civilisation.hpp"
#include "neural.hpp"
#include <array>
#include <filesystem>
#include <string>
#include <vector>
namespace tide {
constexpr int MapSize = 28, TileCount = MapSize * MapSize;
enum class Terrain { Grass, Forest, Water, Rock };
enum class Building {
    None,
    Road,
    Home,
    Farm,
    Workshop,
    Windmill,
    Pump,
    Park,
    Market,
    Clinic,
    TownHall
};
struct BuildingInfo {
    const char *name;
    const char *description;
    int cost;
    double upkeep;
    int capacity;
};
const BuildingInfo &info(Building b);
struct Tile {
    Terrain terrain = Terrain::Grass;
    Building building = Building::None;
    bool connected = false;
    std::array<int, 3> people{};
    double happiness = 0.5;
    int population() const {
        return people[0] + people[1] + people[2];
    }
};
struct Stats {
    int population = 0, capacity = 0, jobs = 0, homes = 0, connected = 0;
    double happiness = 0.5, water = 0, power = 0, foodProduction = 0, foodUse = 0;
    double income = 0, expenses = 0, pollution = 0;
};
struct Result {
    bool ok;
    std::string message;
};
struct History {
    int day, population;
    double money, happiness;
};
class Simulation {
  public:
    explicit Simulation(uint32_t seed = 7261);
    void reset(uint32_t seed);
    Result canBuild(int x, int y, Building b) const;
    Result build(int x, int y, Building b);
    Result demolish(int x, int y);
    void refresh();
    void tick();
    Result diplomacy(int civilisation, int action);
    void advanceCivilisations();
    NeuralNetwork::Features features(int index, int cohort) const;
    double appeal(int index, int cohort) const;
    std::string advice() const;
    Result save(const std::filesystem::path &path) const;
    Result load(const std::filesystem::path &path);
    int count(Building b, bool connectedOnly = false) const;
    int milestone() const;
    const char *season() const;
    static bool inside(int x, int y) {
        return x >= 0 && y >= 0 && x < MapSize && y < MapSize;
    }
    static int index(int x, int y) {
        return y * MapSize + x;
    }
    void announce(const std::string &s);
    std::array<Tile, TileCount> tiles{};
    uint32_t seed = 7261;
    int day = 1, tax = 10, stableDays = 0, debtDays = 0, earnedMilestones = 0;
    bool won = false, lost = false;
    double money = 1700, food = 180;
    Stats stats;
    NeuralNetwork brain;
    uint64_t initialTraining = 0, relocations = 0;
    std::vector<std::string> events;
    std::vector<History> history;
    std::mt19937 rng;
    std::array<Civilisation, 3> civilisations;

  private:
    double near(int index, Building b, int range) const;
    bool coastal(int index) const;
    void connect();
};
} // namespace tide
#endif
