#include "simulation.hpp"
#include <cmath>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <stdexcept>
using namespace tide;
static int checks = 0;
void check(bool ok, const char *why) {
    ++checks;
    if (!ok)
        throw std::runtime_error(why);
}
std::string state(const Simulation &s) {
    std::ostringstream o;
    o << std::setprecision(17);
    s.brain.write(o);
    o << s.day << ' ' << s.money << ' ' << s.food << ' ' << s.rng;
    for (auto &t : s.tiles)
        o << int(t.terrain) << ' ' << int(t.building) << ' ' << t.people[0] << ' ' << t.people[1]
          << ' ' << t.people[2] << ' ' << t.happiness << '\n';
    return o.str();
}
void playCampaign(Simulation &s) {
    int waits = 0;
    auto construct = [&](int x, int y, Building b) {
        auto &t = s.tiles[Simulation::index(x, y)];
        if (t.building != Building::None || t.terrain == Terrain::Water)
            return false;
        while (!s.canBuild(x, y, b).ok && waits < 3000 && !s.lost) {
            s.tick();
            ++waits;
        }
        return s.build(x, y, b).ok;
    };
    for (int y = 5; y < 23; ++y)
        construct(14, y, Building::Road);
    for (int y = 5; y < 23; y += 3)
        for (int x = 5; x < 23; ++x)
            construct(x, y, Building::Road);
    auto add = [&](Building b, int target) {
        for (int y = 6; y < 23 && s.count(b) < target; ++y)
            for (int x = 5; x < 23 && s.count(b) < target; ++x) {
                auto &t = s.tiles[Simulation::index(x, y)];
                if (t.building != Building::None || t.terrain == Terrain::Water)
                    continue;
                bool adjacent = false;
                for (auto [dx, dy] : {std::pair{1, 0}, {-1, 0}, {0, 1}, {0, -1}})
                    if (Simulation::inside(x + dx, y + dy)) {
                        auto &a = s.tiles[Simulation::index(x + dx, y + dy)];
                        adjacent |= a.connected && a.building == Building::Road;
                    }
                if (adjacent)
                    construct(x, y, b);
            }
    };
    for (int phase = 1; phase <= 4; ++phase) {
        add(Building::Market, phase * 2);
        add(Building::Farm, phase * 2 + 1);
        add(Building::Pump, phase);
        add(Building::Windmill, phase);
        add(Building::Workshop, phase * 2);
        add(Building::Park, phase * 3);
        add(Building::Home, phase * 5);
        for (int d = 0; d < 40; ++d)
            s.tick();
        std::cout << "phase " << phase << " day " << s.day << " pop " << s.stats.population
                  << " cash " << s.money << " balance " << s.stats.income - s.stats.expenses
                  << " water " << s.stats.water << " food " << s.stats.foodProduction
                  << " happiness " << s.stats.happiness << "\n";
    }
    for (int d = 0; d < 200 && !s.won && !s.lost; ++d)
        s.tick();
}
int main() {
    try {
        Simulation a(123), b(123);
        check(state(a) == state(b), "same seed reproduces town and model");
        check(a.stats.population == 24, "starter population");
        check(a.count(Building::TownHall) == 1, "one hall");
        check(!a.build(-1, 4, Building::Home).ok, "reject out of bounds");
        check(!a.build(0, 0, Building::Home).ok, "reject water");
        check(!a.demolish(14, 14).ok, "protect hall");
        double money = a.money;
        check(a.build(10, 13, Building::Home).ok, "build connected home");
        check(a.money == money - 85, "construction charges correct price");
        check(a.tiles[Simulation::index(10, 13)].connected, "home connected");
        check(!a.build(10, 13, Building::Home).ok, "reject occupied tile");
        check(a.demolish(10, 14).ok, "road demolition");
        check(!a.tiles[Simulation::index(10, 13)].connected, "broken street disconnects home");
        check(a.build(10, 14, Building::Road).ok, "road restored");
        check(a.tiles[Simulation::index(10, 13)].connected, "restored service");
        a.money = 0;
        check(!a.build(10, 12, Building::Home).ok, "reject unaffordable building");
        a = Simulation(123);
        auto before = a.brain.samples;
        for (int i = 0; i < 20; ++i)
            a.tick();
        check(a.brain.samples > before, "online learning collects town experience");
        check(a.stats.population > 24, "residents move in");
        auto tmp = std::filesystem::temp_directory_path() / "tidemind-core-test.save";
        check(a.save(tmp).ok, "save successful");
        check(b.load(tmp).ok, "load successful");
        check(state(a) == state(b), "exact save round trip");
        for (int i = 0; i < 60; ++i) {
            a.tick();
            b.tick();
        }
        check(state(a) == state(b), "save resume deterministic over 60 days");
        std::string previous = state(b);
        {
            std::ofstream out(tmp);
            out << "TIDEMIND 900\n";
        }
        check(!b.load(tmp).ok, "reject corrupt save");
        check(state(b) == previous, "corrupt load preserves current town");
        std::filesystem::remove(tmp);
        NeuralNetwork net(72);
        NeuralNetwork::Features x{1, 1, 1, .8, 1, 0, .3, 1, 0, 1, 0, 0};
        double target = .91;
        double first = std::abs(net.predict(x) - target);
        for (int i = 0; i < 1000; ++i)
            net.train(x, target);
        check(std::abs(net.predict(x) - target) < first * .1, "backprop reduces prediction error");
        auto good = x;
        auto poor = x;
        poor[0] = poor[1] = poor[2] = poor[7] = 0;
        poor[5] = 1;
        check(a.brain.predict(good) > a.brain.predict(poor) + .15,
              "learned network distinguishes supplied and deprived homes");
        Simulation preferenceA;
        for (auto &t : preferenceA.tiles)
            if (t.building == Building::Home)
                t.people = {4, 4, 4};
        preferenceA.build(10, 13, Building::Home);
        preferenceA.build(18, 13, Building::Home);
        preferenceA.build(10, 15, Building::Park);
        preferenceA.refresh();
        Simulation preferenceB = preferenceA;
        int left = Simulation::index(10, 13), right = Simulation::index(18, 13);
        // Hold every city input constant and change only the learned weights.
        // A model favoring parks and one avoiding them must choose opposite homes.
        for (int n = 0; n < 5000; ++n)
            for (int c = 0; c < 3; ++c) {
                preferenceA.brain.train(preferenceA.features(left, c), .95, .1);
                preferenceA.brain.train(preferenceA.features(right, c), .1, .1);
                preferenceB.brain.train(preferenceB.features(left, c), .1, .1);
                preferenceB.brain.train(preferenceB.features(right, c), .95, .1);
            }
        preferenceA.tick();
        preferenceB.tick();
        check(preferenceA.tiles[left].population() > 0 &&
                  preferenceA.tiles[right].population() == 0,
              "park-favoring learned model selects the garden home");
        check(preferenceB.tiles[right].population() > 0 &&
                  preferenceB.tiles[left].population() == 0,
              "changing only neural weights changes actual resident destination");
        check(preferenceA.relocations > 0 && preferenceB.relocations > 0,
              "learned model also relocates existing residents");
        Simulation failed;
        failed.money = -10000;
        for (int i = 0; i < 10; ++i)
            failed.tick();
        check(failed.lost, "bankruptcy ends campaign");
        int day = failed.day;
        failed.tick();
        check(failed.day == day, "lost town stops simulation");
        failed.reset(4);
        check(!failed.lost && failed.day == 1, "restart restores playable town");
        Simulation starving;
        for (int i = 0; i < TileCount; ++i)
            if (starving.tiles[i].building == Building::Farm)
                starving.demolish(i % MapSize, i / MapSize);
        starving.food = 0;
        for (int i = 0; i < 25; ++i)
            starving.tick();
        check(starving.stats.population < 24, "food failure causes departure");
        Simulation win;
        playCampaign(win);
        std::cout << "Campaign probe: day " << win.day << ", pop " << win.stats.population
                  << ", happiness " << win.stats.happiness << ", food " << win.food << ", stable "
                  << win.stableDays << "\n";
        check(win.won, "campaign is winnable through actual construction and simulation");
        check(win.stats.population >= 180 && win.stableDays >= 10,
              "win requires sustained thriving town");
        std::cout << checks << " checks passed\n";
        return 0;
    } catch (const std::exception &e) {
        std::cerr << "FAILED after " << checks << " checks: " << e.what() << '\n';
        return 1;
    }
}
