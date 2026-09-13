#include "civilisation.hpp"
#include <algorithm>
#include <cmath>
#include <iomanip>
#include <istream>
#include <ostream>
namespace tide {
const char *Civilisation::name() const {
    static const char *names[] = {"Glenmere Commons", "Brasshaven League", "Pearlwater Kin"};
    return names[std::clamp(culture, 0, 2)];
}
const char *Civilisation::cultureName() const {
    static const char *names[] = {"Agrarian cooperatives", "Merchant city-states",
                                  "Island craft guilds"};
    return names[std::clamp(culture, 0, 2)];
}
const char *Civilisation::capital() const {
    static const char *names[] = {"Greenbank", "Brasshaven", "Pearlwater"};
    return names[std::clamp(culture, 0, 2)];
}
const char *Civilisation::era() const {
    return population >= 180 ? "Commonwealth" : population >= 90 ? "City-state" : "Settlement";
}
double Civilisation::harvest(bool winter) const {
    return farms * (culture == 0 ? 38.0 : 30.0) * (winter ? .65 : 1.0);
}
int Civilisation::jobs() const {
    return workshops * (culture == 2 ? 28 : 22) + farms * 8;
}
void Civilisation::advance(int day) {
    food = std::clamp(food + harvest((day - 1) / 30 % 4 == 3) - population * .65, 0.0, 3000.0);
    money = std::clamp(money + 12 + population * (culture == 1 ? .31 : .23) - homes * .4 -
                           farms * 1.5 - workshops * 2.5,
                       0.0, 1e9);
    if (food < 1 && population > 4) {
        --population;
        lastAction = "Food shortage; residents are leaving";
    } else if (day % 2 == 0 && food > population * 2 && population < homes * 12 &&
               population < jobs())
        ++population;
    if (day % 5 != 0)
        return;
    // Independent civic budgets and decisions: protect reserves before expansion.
    // Cultures have materially different yields, income and workshop efficiency.
    if ((harvest(true) < population * .8 || food < population * 4) && money >= 120 && farms < 80) {
        money -= 120;
        ++farms;
        lastAction = "Planted a new farming district";
    } else if (jobs() < population + 12 && money >= 170 && workshops < 80) {
        money -= 170;
        ++workshops;
        lastAction = "Founded a new workshop district";
    } else if (homes * 12 - population < 12 && money >= 85 && homes < 80) {
        money -= 85;
        ++homes;
        lastAction = "Founded a new residential district";
    } else
        lastAction = "Building reserves for future growth";
}
std::array<Civilisation, 3> createCivilisations(uint32_t seed) {
    std::array<Civilisation, 3> civs;
    for (int i = 0; i < 3; ++i) {
        auto &c = civs[i];
        c.culture = i;
        c.population = 30 + i * 6 + int(seed % 5);
        c.money = 400 + i * 50;
    }
    return civs;
}
void writeCivilisations(std::ostream &out, const std::array<Civilisation, 3> &civs) {
    out << "CIVILISATIONS 1\n";
    for (auto &c : civs)
        out << c.culture << ' ' << c.population << ' ' << c.homes << ' ' << c.farms << ' '
            << c.workshops << ' ' << c.money << ' ' << c.food << ' ' << c.relations << ' '
            << c.envoyReady << ' ' << c.allied << ' ' << int(c.route) << ' ' << c.shipments << ' '
            << std::quoted(c.lastAction) << ' ' << std::quoted(c.routeStatus) << '\n';
}
bool readCivilisations(std::istream &in, std::array<Civilisation, 3> &result) {
    std::string magic;
    int version;
    if (!(in >> magic >> version) || magic != "CIVILISATIONS" || version != 1)
        return false;
    std::array<Civilisation, 3> civs;
    for (int i = 0; i < 3; ++i) {
        auto &c = civs[i];
        int route;
        if (!(in >> c.culture >> c.population >> c.homes >> c.farms >> c.workshops >> c.money >>
              c.food >> c.relations >> c.envoyReady >> c.allied >> route >> c.shipments >>
              std::quoted(c.lastAction) >> std::quoted(c.routeStatus)))
            return false;
        if (c.culture != i || c.homes < 1 || c.homes > 80 || c.farms < 1 || c.farms > 80 ||
            c.workshops < 1 || c.workshops > 80 || c.population < 0 ||
            c.population > c.homes * 12 || !std::isfinite(c.money) || c.money < 0 ||
            c.money > 1e12 || !std::isfinite(c.food) || c.food < 0 || c.food > 3000 ||
            c.relations < 0 || c.relations > 100 || c.envoyReady < 1 || c.envoyReady > 100000010 ||
            route < 0 || route > 2 || c.shipments > 1000000000 || c.lastAction.size() > 200 ||
            c.routeStatus.size() > 200)
            return false;
        c.route = TradeRoute(route);
    }
    result = std::move(civs);
    return true;
}
} // namespace tide
