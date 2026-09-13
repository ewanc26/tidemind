#ifndef TIDEMIND_CIVILISATION_HPP
#define TIDEMIND_CIVILISATION_HPP
#include <array>
#include <cstdint>
#include <iosfwd>
#include <string>
namespace tide {
enum class TradeRoute { None, ImportFood, ExportFood };
struct Civilisation {
    int culture = 0, population = 36, homes = 4, farms = 2, workshops = 2;
    double money = 400, food = 180;
    int relations = 50, envoyReady = 1;
    bool allied = false;
    TradeRoute route = TradeRoute::None;
    uint64_t shipments = 0;
    std::string lastAction = "Settling the coast", routeStatus = "No trade agreement";
    const char *name() const;
    const char *cultureName() const;
    const char *capital() const;
    const char *era() const;
    double harvest(bool winter) const;
    int jobs() const;
    void advance(int day);
};
std::array<Civilisation, 3> createCivilisations(uint32_t seed);
void writeCivilisations(std::ostream &, const std::array<Civilisation, 3> &);
bool readCivilisations(std::istream &, std::array<Civilisation, 3> &);
} // namespace tide
#endif
