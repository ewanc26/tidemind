#include "ui.hpp"
#include "simulation.hpp"
#include <SDL.h>
#include <SDL_ttf.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>
namespace tide {
namespace {
constexpr int W = 1400, H = 900;
struct Color {
    Uint8 r, g, b, a = 255;
};
constexpr Color Ink{32, 55, 55}, Muted{109, 124, 117}, Paper{247, 245, 232}, Cream{235, 233, 216},
    Line{214, 219, 200}, Green{58, 122, 94}, Gold{212, 170, 82}, Ocean{29, 66, 77},
    Red{184, 89, 69};
SDL_Color sc(Color c) {
    return {c.r, c.g, c.b, c.a};
}
Color tint(Color c, double f) {
    return {Uint8(std::clamp(c.r * f, 0.0, 255.0)), Uint8(std::clamp(c.g * f, 0.0, 255.0)),
            Uint8(std::clamp(c.b * f, 0.0, 255.0)), c.a};
}
struct Point {
    float x, y;
};
struct Button {
    SDL_Rect rect;
    int action;
};
struct Texture {
    SDL_Texture *ptr;
    int w, h;
};
class Game {
  public:
    explicit Game(const Options &o) : options(o), sim(o.seed) {}
    ~Game() {
        for (auto &[k, t] : texts)
            SDL_DestroyTexture(t.ptr);
        for (auto &[k, f] : fonts)
            TTF_CloseFont(f);
        if (renderer)
            SDL_DestroyRenderer(renderer);
        if (window)
            SDL_DestroyWindow(window);
        TTF_Quit();
        SDL_Quit();
    }
    int run();

  private:
    Options options;
    Simulation sim;
    SDL_Window *window = nullptr;
    SDL_Renderer *renderer = nullptr;
    std::map<int, TTF_Font *> fonts;
    std::map<std::string, Texture> texts;
    std::string fontPath;
    std::filesystem::path savePath;
    std::vector<Button> buttons;
    int tool = 0, selected = Simulation::index(12, 13), hover = -1, cohort = 0, speed = 1;
    bool paused = true, menu = false, help = true, overlay = false, quit = false, drag = false,
         painting = false;
    bool confirmNew = false, confirmLoad = false, confirmQuit = false, journal = false;
    float camX = 690, camY = 170, zoom = 1;
    int lastPaint = -1;
    double elapsed = 0, clock = 0, toastTime = 0;
    std::string toast = "Welcome, town planner.";
    bool smokeBuilt = false, smokeSaved = false, smokeLoaded = false, smokeDemo = false,
         smokeJournal = false, smokeRoads = false;
    void color(Color c) {
        SDL_SetRenderDrawColor(renderer, c.r, c.g, c.b, c.a);
    }
    void rect(int x, int y, int w, int h, Color c) {
        color(c);
        SDL_Rect r{x, y, w, h};
        SDL_RenderFillRect(renderer, &r);
    }
    void outline(int x, int y, int w, int h, Color c) {
        color(c);
        SDL_Rect r{x, y, w, h};
        SDL_RenderDrawRect(renderer, &r);
    }
    void line(float x, float y, float x2, float y2, Color c) {
        color(c);
        SDL_RenderDrawLineF(renderer, x, y, x2, y2);
    }
    void poly(std::initializer_list<Point> points, Color c) {
        std::vector<SDL_Vertex> v;
        for (auto p : points)
            v.push_back({{p.x, p.y}, sc(c), {0, 0}});
        std::vector<int> ix;
        for (int i = 1; i + 1 < int(v.size()); ++i) {
            ix.push_back(0);
            ix.push_back(i);
            ix.push_back(i + 1);
        }
        SDL_RenderGeometry(renderer, nullptr, v.data(), int(v.size()), ix.data(), int(ix.size()));
    }
    void circle(float x, float y, float r, Color c) {
        color(c);
        for (int dy = -int(r); dy <= int(r); ++dy) {
            float dx = std::sqrt(std::max(0.f, r * r - dy * dy));
            SDL_RenderDrawLineF(renderer, x - dx, y + dy, x + dx, y + dy);
        }
    }
    void text(std::string s, int x, int y, int size = 14, Color c = Ink);
    int wrapped(std::string s, int x, int y, int width, int size = 14, Color c = Muted);
    bool button(int x, int y, int w, int h, std::string label, int action, bool active = false);
    void badge(int x, int y, std::string label, Color c = Green) {
        rect(x, y, 8, 8, c);
        text(label, x + 16, y - 6, 12, c);
    }
    Point tilePoint(float x, float y) const {
        return {camX + (x - y) * 19 * zoom, camY + (x + y) * 9.5f * zoom};
    }
    int pick(int x, int y) const;
    void diamond(Point p, float hw, float hh, Color c) {
        poly({{p.x, p.y - hh}, {p.x + hw, p.y}, {p.x, p.y + hh}, {p.x - hw, p.y}}, c);
    }
    void box(Point p, float a, float b, float h, Color c);
    void tree(Point p, int variant = 0);
    void building(Point p, Building b, int variant = 0, float scale = 1);
    void world();
    void panel();
    void draw();
    void modal();
    void handle(const SDL_Event &e);
    void action(int id);
    void apply(int i);
    void notify(Result r) {
        toast = r.message;
        toastTime = 5;
    }
    void screenshot(const std::filesystem::path &path);
    void smokeStep(int frame);
};
void Game::text(std::string s, int x, int y, int size, Color c) {
    if (s.empty())
        return;
    if (!fonts.count(size)) {
        fonts[size] = TTF_OpenFont(fontPath.c_str(), size);
        if (!fonts[size])
            throw std::runtime_error(TTF_GetError());
    }
    std::string key = std::to_string(size) + ":" + s;
    if (!texts.count(key)) {
        if (texts.size() > 1600) {
            for (auto &[k, t] : texts)
                SDL_DestroyTexture(t.ptr);
            texts.clear();
        }
        SDL_Surface *surface = TTF_RenderUTF8_Blended(fonts[size], s.c_str(), {255, 255, 255, 255});
        if (!surface)
            return;
        SDL_Texture *t = SDL_CreateTextureFromSurface(renderer, surface);
        if (!t) {
            SDL_FreeSurface(surface);
            return;
        }
        texts[key] = {t, surface->w, surface->h};
        SDL_FreeSurface(surface);
    }
    auto &t = texts.at(key);
    SDL_SetTextureColorMod(t.ptr, c.r, c.g, c.b);
    SDL_SetTextureAlphaMod(t.ptr, c.a);
    SDL_Rect dst{x, y, t.w, t.h};
    SDL_RenderCopy(renderer, t.ptr, nullptr, &dst);
}
int Game::wrapped(std::string s, int x, int y, int width, int size, Color c) {
    if (!fonts.count(size))
        text(" ", 0, 0, size);
    std::istringstream in(s);
    std::string word, row;
    while (in >> word) {
        std::string next = row.empty() ? word : row + " " + word;
        int w = 0;
        TTF_SizeUTF8(fonts[size], next.c_str(), &w, nullptr);
        if (w > width && !row.empty()) {
            text(row, x, y, size, c);
            y += size + 6;
            row = word;
        } else
            row = next;
    }
    if (!row.empty()) {
        text(row, x, y, size, c);
        y += size + 6;
    }
    return y;
}
bool Game::button(int x, int y, int w, int h, std::string label, int id, bool active) {
    int mx, my;
    SDL_GetMouseState(&mx, &my);
    bool over = mx >= x && mx < x + w && my >= y && my < y + h;
    rect(x, y, w, h, active ? Green : over ? Line : Cream);
    if (active)
        text(label, x + 12, y + (h - 20) / 2, 14, Paper);
    else
        text(label, x + 12, y + (h - 20) / 2, 14, Ink);
    buttons.push_back({{x, y, w, h}, id});
    return over;
}
int Game::pick(int x, int y) const {
    if (x < 250 || x >= 1100 || y < 112 || y >= 820)
        return -1;
    float dx = (x - camX) / (19 * zoom), dy = (y - camY) / (9.5f * zoom);
    int tx = int(std::floor((dx + dy) * .5f + .5f)), ty = int(std::floor((dy - dx) * .5f + .5f));
    return Simulation::inside(tx, ty) ? Simulation::index(tx, ty) : -1;
}
void Game::box(Point p, float a, float b, float h, Color c) {
    a *= zoom;
    b *= zoom;
    h *= zoom;
    poly({{p.x - a, p.y}, {p.x, p.y + b}, {p.x, p.y + b - h}, {p.x - a, p.y - h}}, tint(c, .78));
    poly({{p.x, p.y + b}, {p.x + a, p.y}, {p.x + a, p.y - h}, {p.x, p.y + b - h}}, tint(c, .94));
    poly({{p.x - a, p.y - h}, {p.x, p.y - b - h}, {p.x + a, p.y - h}, {p.x, p.y + b - h}},
         tint(c, 1.1));
}
void Game::tree(Point p, int variant) {
    float s = zoom;
    line(p.x, p.y, p.x, p.y - 20 * s, {91, 90, 58});
    Color c = variant % 3 == 0   ? Color{87, 131, 89}
              : variant % 3 == 1 ? Color{64, 111, 84}
                                 : Color{106, 143, 91};
    if (variant % 2) {
        poly({{p.x - 9 * s, p.y - 7 * s}, {p.x, p.y - 31 * s}, {p.x + 9 * s, p.y - 7 * s}}, c);
        poly({{p.x, p.y - 31 * s}, {p.x + 9 * s, p.y - 7 * s}, {p.x, p.y - 10 * s}}, tint(c, .8));
    } else {
        circle(p.x, p.y - 17 * s, 9 * s, c);
        circle(p.x - 4 * s, p.y - 20 * s, 6 * s, tint(c, 1.14));
    }
}
void Game::building(Point p, Building b, int v, float scale) {
    float old = zoom;
    zoom *= scale;
    float z = zoom;
    switch (b) {
    case Building::Home: {
        Color wall = v % 3 == 0   ? Color{232, 219, 177}
                     : v % 3 == 1 ? Color{226, 228, 208}
                                  : Color{210, 222, 211};
        box(p, 11, 5.5f, 17, wall);
        Color roof = v % 3 == 0   ? Color{171, 98, 69}
                     : v % 3 == 1 ? Color{91, 126, 124}
                                  : Color{185, 151, 91};
        poly({{p.x - 13 * z, p.y - 17 * z},
              {p.x, p.y - 27 * z},
              {p.x + 13 * z, p.y - 17 * z},
              {p.x, p.y - 10 * z}},
             roof);
        poly({{p.x, p.y - 27 * z}, {p.x + 13 * z, p.y - 17 * z}, {p.x, p.y - 10 * z}},
             tint(roof, .8));
        rect(int(p.x + 3 * z), int(p.y - 9 * z), std::max(1, int(3 * z)), std::max(1, int(5 * z)),
             {61, 87, 88});
        line(p.x - 6 * z, p.y - 11 * z, p.x - 6 * z, p.y - 7 * z, {100, 119, 103});
        break;
    }
    case Building::Farm:
        diamond(p, 16 * z, 8 * z, {156, 136, 81});
        for (int i = -10; i <= 10; i += 5)
            line(p.x + i * z - 4 * z, p.y - 3 * z, p.x + i * z + 4 * z, p.y + 2 * z,
                 {211, 192, 100});
        box({p.x + 5 * z, p.y - 4 * z}, 5, 2.5f, 9, {185, 118, 74});
        break;
    case Building::Workshop:
        box(p, 13, 6, 17, {193, 170, 133});
        box({p.x - 6 * z, p.y - 4 * z}, 3, 1.5f, 30, {118, 119, 101});
        box({p.x + 4 * z, p.y - 6 * z}, 7, 3.5f, 19, {103, 134, 127});
        for (int k = 0; k < 3; ++k)
            circle(p.x - 6 * z + float(std::sin(clock + k)) * 3 * z,
                   p.y - (34 + k * 8 + std::fmod(clock * 3, 8)) * z, (2 + k) * z,
                   {189, 201, 184, Uint8(95 - k * 23)});
        break;
    case Building::Windmill: {
        box(p, 5, 2.5f, 34, {226, 229, 210});
        Point hub{p.x, p.y - 31 * z};
        circle(hub.x, hub.y, 3 * z, Ink);
        for (int i = 0; i < 3; ++i) {
            double a = clock * .65 + i * 2.094;
            Point tip{hub.x + float(std::cos(a)) * 20 * z, hub.y + float(std::sin(a)) * 20 * z};
            poly({hub,
                  {tip.x - float(std::sin(a)) * 2 * z, tip.y + float(std::cos(a)) * 2 * z},
                  tip},
                 Paper);
        }
        break;
    }
    case Building::Pump:
        for (int dx : {-6, 6})
            line(p.x + dx * z, p.y, p.x + dx * z, p.y - 24 * z, {86, 111, 109});
        box({p.x, p.y - 20 * z}, 10, 5, 10, {126, 172, 178});
        diamond({p.x, p.y - 30 * z}, 10 * z, 5 * z, {192, 221, 212});
        break;
    case Building::Park:
        diamond(p, 16 * z, 8 * z, {136, 170, 102});
        tree({p.x - 5 * z, p.y}, v);
        line(p.x + 4 * z, p.y + 2 * z, p.x + 11 * z, p.y - 2 * z, {181, 141, 80});
        for (int i = 0; i < 4; ++i)
            circle(p.x + float(i * 3 - 3) * z, p.y + 4 * z, 1.3f * z, Gold);
        break;
    case Building::Market:
        box(p, 13, 6, 12, {226, 205, 149});
        diamond({p.x, p.y - 15 * z}, 15 * z, 7 * z, {183, 93, 66});
        for (int i = -8; i <= 8; i += 8)
            line(p.x + i * z, p.y - 15 * z, p.x + (i + 3) * z, p.y - 13 * z, Paper);
        break;
    case Building::Clinic:
        box(p, 13, 6, 20, {228, 232, 216});
        diamond({p.x, p.y - 21 * z}, 14 * z, 7 * z, {122, 162, 146});
        rect(int(p.x + 3 * z), int(p.y - 11 * z), std::max(2, int(6 * z)), std::max(1, int(2 * z)),
             Red);
        rect(int(p.x + 5 * z), int(p.y - 13 * z), std::max(1, int(2 * z)), std::max(2, int(6 * z)),
             Red);
        break;
    case Building::TownHall:
        box(p, 15, 7, 24, {230, 217, 180});
        diamond({p.x, p.y - 25 * z}, 17 * z, 8 * z, {107, 139, 132});
        box({p.x, p.y - 26 * z}, 5, 2.5f, 14, {235, 224, 190});
        poly({{p.x - 7 * z, p.y - 41 * z},
              {p.x, p.y - 49 * z},
              {p.x + 7 * z, p.y - 41 * z},
              {p.x, p.y - 37 * z}},
             Gold);
        line(p.x, p.y - 46 * z, p.x, p.y - 60 * z, Ink);
        poly({{p.x, p.y - 60 * z}, {p.x + 12 * z, p.y - 57 * z}, {p.x, p.y - 53 * z}}, Green);
        break;
    default:
        break;
    }
    zoom = old;
}
void Game::world() {
    SDL_Rect clip{250, 112, 850, 708};
    SDL_RenderSetClipRect(renderer, &clip);
    rect(250, 112, 850, 708, Ocean);
    for (int i = 0; i < 38; ++i) {
        float x = 270 + float((i * 137) % 810), y = 140 + float((i * 79) % 650);
        float shift = float(std::sin(clock * .5 + i)) * 4;
        line(x + shift, y, x + shift + 10, y, {51, 88, 95});
    }
    for (int depth = 0; depth < 2 * MapSize - 1; ++depth)
        for (int y = 0; y < MapSize; ++y) {
            int x = depth - y;
            if (!Simulation::inside(x, y))
                continue;
            int i = Simulation::index(x, y);
            const auto &t = sim.tiles[i];
            auto p = tilePoint(float(x), float(y));
            float hw = 19 * zoom, hh = 9.5f * zoom;
            if (p.x + hw < 250 || p.x - hw > 1100 || p.y + hh < 112 || p.y - 65 * zoom > 820)
                continue;
            if (t.terrain == Terrain::Water) {
                if ((x + y) % 3 == 0)
                    line(p.x - 4 * zoom, p.y + float(std::sin(clock + i)) * .6f, p.x + 4 * zoom,
                         p.y, {55, 96, 104});
                continue;
            }
            if (y == MapSize - 1 || x == MapSize - 1 ||
                sim.tiles[Simulation::index(x, std::min(y + 1, MapSize - 1))].terrain ==
                    Terrain::Water ||
                sim.tiles[Simulation::index(std::min(x + 1, MapSize - 1), y)].terrain ==
                    Terrain::Water) {
                poly({{p.x - hw, p.y},
                      {p.x, p.y + hh},
                      {p.x, p.y + hh + 8 * zoom},
                      {p.x - hw, p.y + 8 * zoom}},
                     {145, 143, 103});
                poly({{p.x, p.y + hh},
                      {p.x + hw, p.y},
                      {p.x + hw, p.y + 8 * zoom},
                      {p.x, p.y + hh + 8 * zoom}},
                     {181, 169, 120});
            }
            int noise = (x * 31 + y * 73 + int(sim.seed % 11)) % 9;
            Color ground{Uint8(137 + noise), Uint8(161 + noise), Uint8(111 + noise)};
            if (t.terrain == Terrain::Forest)
                ground = {125, 151, 105};
            diamond(p, hw, hh, ground);
            if (overlay && t.building == Building::Home) {
                double a = sim.appeal(i, cohort);
                diamond(p, hw, hh,
                        {Uint8(190 * (1 - a) + 52 * a), Uint8(91 * (1 - a) + 191 * a), 95, 220});
            }
            if (t.building == Building::Road) {
                diamond(p, hw, hh, t.connected ? Color{198, 194, 156} : Color{151, 149, 124});
                for (auto [dx, dy] : {std::pair{1, 0}, {0, 1}}) {
                    int xx = x + dx, yy = y + dy;
                    if (Simulation::inside(xx, yy) &&
                        sim.tiles[Simulation::index(xx, yy)].building == Building::Road) {
                        auto q = tilePoint(float(xx), float(yy));
                        line(p.x, p.y, q.x, q.y, {225, 218, 179});
                    }
                }
            } else if (t.building != Building::None) {
                building(p, t.building, i);
                if (!t.connected) {
                    circle(p.x + 10 * zoom, p.y - 28 * zoom, 5 * zoom, Red);
                    line(p.x + 10 * zoom, p.y - 31 * zoom, p.x + 10 * zoom, p.y - 27 * zoom, Paper);
                }
            } else if (t.terrain == Terrain::Forest) {
                tree(p, i);
                if (i % 3 == 0)
                    tree({p.x + 9 * zoom, p.y + 3 * zoom}, i + 1);
            } else if (t.terrain == Terrain::Rock) {
                box({p.x, p.y + 2 * zoom}, 8, 4, 5, {159, 166, 144});
            } else if (i % 11 == 0) {
                line(p.x - 3 * zoom, p.y, p.x - 1 * zoom, p.y - 2 * zoom, {116, 146, 100});
            }
            if (t.building == Building::Road && t.connected && i % 4 == 0 &&
                sim.stats.population > 0) {
                float walk = float(std::sin(clock * .65 + i)) * 9 * zoom;
                circle(p.x + walk, p.y, 1.7f * zoom, Ink);
                circle(p.x + walk, p.y - 3 * zoom, 1.4f * zoom, {231, 213, 163});
            }
        }
    if (selected >= 0) {
        auto p = tilePoint(float(selected % MapSize), float(selected / MapSize));
        float a = 19 * zoom, b = 9.5f * zoom;
        line(p.x - a, p.y, p.x, p.y - b, Paper);
        line(p.x, p.y - b, p.x + a, p.y, Paper);
        line(p.x + a, p.y, p.x, p.y + b, Paper);
        line(p.x, p.y + b, p.x - a, p.y, Paper);
    }
    if (hover >= 0 && !menu && !help && !journal) {
        auto p = tilePoint(float(hover % MapSize), float(hover / MapSize));
        bool valid = tool >= 1 && tool <= 9
                         ? sim.canBuild(hover % MapSize, hover / MapSize, Building(tool)).ok
                         : true;
        diamond(p, 19 * zoom, 9.5f * zoom,
                valid ? Color{242, 233, 186, 105} : Color{222, 106, 77, 150});
        if (tool >= 2 && tool <= 9 && sim.tiles[hover].building == Building::None && valid)
            building(p, Building(tool), hover);
    }
    SDL_RenderSetClipRect(renderer, nullptr);
    text("ESTUARY  /  " + std::to_string(sim.seed), 272, 132, 12, {167, 192, 187});
    text(overlay ? "NEURAL APPEAL  •  red: lower   /   green: higher"
                 : "A place to put down roots.",
         272, 154, 14, {192, 210, 198});
    button(263, 771, 38, 32, "−", 301);
    button(306, 771, 38, 32, "+", 302);
    button(350, 771, 83, 32, "Centre", 303);
    button(882, 771, 200, 32, overlay ? "Neural overlay: ON" : "Neural overlay: OFF", 304, overlay);
}
void Game::panel() {
    rect(0, 112, 250, 788, Paper);
    rect(1100, 112, 300, 788, Paper);
    text("BUILD YOUR TOWN", 22, 132, 12, Muted);
    text("A good place to live", 22, 154, 19);
    text("Choose a tool, then a tile.", 22, 186, 13, Muted);
    static const char *keys[] = {"I", "1", "2", "3", "4", "5", "6", "7", "8", "9", "B"};
    for (int j = 0; j <= 10; ++j) {
        int y = 222 + j * 45;
        bool active = tool == j;
        button(16, y, 218, 40, "", 100 + j, active);
        Color c = active ? Paper : Ink;
        text(keys[j], 27, y + 10, 12, c);
        text(j == 10 ? "Demolish" : info(Building(j)).name, 51, y + 8, 14, c);
        if (j > 0 && j < 10)
            text(std::to_string(info(Building(j)).cost), 193, y + 10, 12, c);
    }
    line(22, 733, 227, 733, Line);
    text("PLANNER'S NOTE", 22, 751, 11, Muted);
    wrapped(
        tool == 10
            ? "Remove a building for a one-third refund. Residents in demolished homes leave town."
            : info(Building(tool)).description,
        22, 775, 206, 13, Muted);
    text("Drag streets · Right-click to inspect", 22, 864, 10, Muted);
    text("TOWN PULSE", 1122, 132, 12, Muted);
    int target = sim.stats.population < 40 ? 40 : sim.stats.population < 100 ? 100 : 180;
    text(sim.won         ? "An independent town"
         : target == 40  ? "Become a village"
         : target == 100 ? "Build a growing town"
                         : "Earn independence",
         1122, 154, 19);
    text(std::to_string(sim.stats.population) + " / " + std::to_string(target) + " residents", 1122,
         185, 13, Muted);
    rect(1122, 211, 254, 5, Line);
    rect(1122, 211, int(254 * std::min(1.0, sim.stats.population / double(target))), 5, Green);
    if (target == 180)
        text("Thriving: " + std::to_string(std::min(sim.stableDays, 10)) + " / 10 days", 1122, 224,
             12, Green);
    else
        text("Next charter includes a 250 coin grant", 1122, 224, 11, Muted);
    auto resource = [&](int y, std::string label, std::string value, double ratio) {
        text(label, 1122, y, 13, Muted);
        text(value, 1270, y, 13, ratio < 1 ? Red : Ink);
        rect(1122, y + 26, 254, 3, Line);
        rect(1122, y + 26, int(254 * std::clamp(ratio, 0.0, 1.0)), 3, ratio < 1 ? Gold : Green);
    };
    resource(256, "Water",
             std::to_string(int(sim.stats.water)) + " / " + std::to_string(sim.stats.population),
             sim.stats.water / std::max(1, sim.stats.population));
    resource(300, "Clean power",
             std::to_string(int(sim.stats.power)) + " / " + std::to_string(sim.stats.population),
             sim.stats.power / std::max(1, sim.stats.population));
    resource(344, "Jobs",
             std::to_string(sim.stats.jobs) + " / " + std::to_string(sim.stats.population),
             double(sim.stats.jobs) / std::max(1, sim.stats.population));
    text("Food / day", 1122, 390, 13, Muted);
    double balance = sim.stats.foodProduction - sim.stats.foodUse;
    text((balance >= 0 ? "+" : "") + std::to_string(int(balance)), 1328, 390, 14,
         balance < 0 ? Red : Green);
    text("Tax rate", 1122, 425, 13, Muted);
    button(1230, 419, 30, 30, "−", 401);
    text(std::to_string(sim.tax) + "%", 1271, 425, 13);
    button(1344, 419, 30, 30, "+", 402);
    line(1122, 467, 1376, 467, Line);
    text("NEIGHBOURHOOD INTELLIGENCE", 1122, 486, 11, Muted);
    button(1122, 515, 78, 28, "Families", 410, cohort == 0);
    button(1206, 515, 78, 28, "Workers", 411, cohort == 1);
    button(1290, 515, 86, 28, "Coastal", 412, cohort == 2);
    int tile = selected >= 0 ? selected : Simulation::index(12, 13);
    auto features = sim.features(tile, cohort);
    auto acts = sim.brain.activations(features);
    for (int a = 0; a < 12; ++a)
        for (int b = 0; b < 16; ++b) {
            double strength = std::abs(acts[b]);
            line(1136, 561 + a * 7, 1238, 553 + b * 6,
                 {Uint8(190 - 30 * strength), Uint8(207 - 25 * strength),
                  Uint8(190 - 20 * strength), 110});
        }
    for (int b = 0; b < 16; ++b) {
        line(1238, 553 + b * 6, 1343, 600, {169, 192, 173, 160});
        circle(1238, 553 + b * 6, 3, acts[b] > 0 ? Green : Gold);
    }
    for (int a = 0; a < 12; ++a)
        circle(1136, 561 + a * 7, 3, features[a] > .5 ? Green : Muted);
    circle(1343, 600, 13, Green);
    text(std::to_string(int(sim.appeal(tile, cohort) * 100)), 1334, 590, 12, Paper);
    text("12 inputs   /   16 hidden   /   appeal", 1122, 649, 11, Muted);
    text(std::to_string(sim.brain.samples - sim.initialTraining) + " local learning samples", 1122,
         673, 12, Green);
    text(std::to_string(sim.relocations) + " learned moves between homes", 1122, 694, 12, Muted);
    std::ostringstream loss;
    loss.setf(std::ios::fixed);
    loss.precision(4);
    loss << sim.brain.loss;
    text("Recent prediction error: " + loss.str(), 1122, 715, 11, Muted);
    line(1122, 749, 1376, 749, Line);
    if (selected >= 0) {
        auto &t = sim.tiles[selected];
        text(std::string(info(t.building).name) + "  /  " + std::to_string(selected % MapSize) +
                 ", " + std::to_string(selected / MapSize),
             1122, 765, 15);
        if (t.building == Building::Home)
            text(std::to_string(t.population()) + " residents · " +
                     std::to_string(int(t.happiness * 100)) + "% wellbeing",
                 1122, 791, 12, Muted);
        else
            text(t.building == Building::None
                     ? (t.terrain == Terrain::Water ? "Open water" : "Open land")
                 : t.connected ? "Connected to town hall"
                               : "Needs a connected street",
                 1122, 791, 12, t.connected ? Green : Muted);
        text("Predicted appeal: " + std::to_string(int(sim.appeal(selected, cohort) * 100)) + "%",
             1122, 817, 13, Green);
        text("Actual model inference for this tile", 1122, 843, 11, Muted);
    }
}
void Game::modal() {
    if (!menu && !help && !journal && !sim.lost)
        return;
    rect(0, 0, W, H, {15, 37, 40, 170});
    rect(370, 165, 660, 625, Paper);
    rect(370, 165, 660, 7, Green);
    if (journal) {
        text("THE TOWN JOURNAL", 407, 201, 12, Green);
        text("From a settlement to a home.", 407, 234, 27);
        text("Income: " + std::to_string(int(sim.stats.income)) + " / day", 407, 287, 15, Green);
        text("Upkeep: " + std::to_string(int(sim.stats.expenses)) + " / day", 700, 287, 15, Muted);
        text("POPULATION / LAST 180 DAYS", 407, 332, 11, Muted);
        rect(407, 365, 585, 100, Cream);
        int highest = 1;
        for (auto &h : sim.history)
            highest = std::max(highest, h.population);
        for (size_t i = 1; i < sim.history.size(); ++i) {
            float x1 = 407 + 585.f * (i - 1) / std::max(size_t(1), sim.history.size() - 1);
            float x2 = 407 + 585.f * i / std::max(size_t(1), sim.history.size() - 1);
            line(x1, 460 - 90.f * sim.history[i - 1].population / highest, x2,
                 460 - 90.f * sim.history[i].population / highest, Green);
        }
        int y = 490;
        auto first = sim.events.size() > 5 ? sim.events.size() - 5 : 0;
        for (size_t i = first; i < sim.events.size(); ++i) {
            y = wrapped(sim.events[i], 407, y, 580, 13, Ink) + 8;
        }
        button(407, 726, 585, 40, "Back to town", 510, true);
    } else if (help) {
        text("WELCOME TO TIDEMIND", 407, 201, 12, Green);
        text("A city that learns to live.", 407, 231, 31);
        wrapped("Build an estuary town that people want to call home. Your residents have "
                "different priorities, and a local neural network learns from their experience.",
                407, 288, 577, 16);
        text("01   Connect", 407, 371, 19);
        wrapped("Streets carry services from the town hall. New buildings must touch a connected "
                "street to work.",
                407, 405, 568, 14);
        text("02   Balance", 407, 461, 19);
        wrapped("Build homes, farms, jobs, water towers and windmills. Save food for winter. "
                "Gardens and clinics help nearby residents thrive.",
                407, 495, 568, 14);
        text("03   Grow", 407, 551, 19);
        wrapped("Reach 180 people and sustain food, services, a balanced budget and 65% wellbeing "
                "for 10 days. Keep building after you win.",
                407, 585, 568, 14);
        button(407, 663, 274, 44, "Let's build  /  start time", 501, true);
        button(698, 663, 294, 44, "Explore while paused", 502);
    } else {
        text(sim.lost ? "THE TOWN'S STORY ENDS" : "TAKE A BREATH", 407, 202, 12, Green);
        text(sim.lost ? "Time for a new beginning." : "Your town can wait.", 407, 236, 30);
        wrapped(sim.lost ? "Ten days in deep debt have closed the treasury. Start a new island, or "
                           "load your last save to try another plan."
                         : "Your town and its learned memories live on this computer. Save before "
                           "leaving, or pick up where you left off.",
                407, 295, 575, 16);
        button(407, 388, 275, 46, "Resume", 503, true);
        button(699, 388, 293, 46, "How to play", 504);
        button(407, 449, 275, 46, "Save town  /  F5", 505);
        button(699, 449, 293, 46,
               confirmLoad ? "Confirm: replace current town" : "Load town  /  F9", 506);
        button(407, 510, 585, 46,
               confirmNew ? "Confirm: discard this town and start a new island" : "New island",
               507);
        button(407, 571, 275, 46, "Save and quit", 508);
        button(699, 571, 293, 46,
               confirmQuit ? "Confirm: quit without saving" : "Quit without saving", 509);
        text("Move: WASD / arrows    Zoom: wheel    Pause: Space", 407, 646, 13, Muted);
        text("Tools: 1–9    Demolish: B    Inspect: I    Neural overlay: Tab", 407, 672, 13, Muted);
        text("Save: F5    Load: F9    Speed: + / −    Centre: Home", 407, 698, 13, Muted);
        if (toastTime > 0)
            wrapped(toast, 407, 736, 580, 12, Green);
    }
}
void Game::draw() {
    buttons.clear();
    color(Ocean);
    SDL_RenderClear(renderer);
    world();
    panel();
    rect(0, 0, W, 112, Paper);
    line(0, 111, W, 111, Line);
    text("tidemind", 22, 15, 33, Ink);
    text("A CITY THAT LEARNS TO LIVE", 24, 64, 10, Muted);
    auto metric = [&](int x, std::string label, std::string value, Color c = Ink) {
        text(label, x, 23, 11, Muted);
        text(value, x, 46, 25, c);
    };
    metric(279, "TREASURY", std::to_string(int(sim.money)), sim.money < 0 ? Red : Ink);
    metric(442, "RESIDENTS", std::to_string(sim.stats.population));
    metric(596, "WELLBEING", std::to_string(int(sim.stats.happiness * 100)) + "%",
           sim.stats.happiness >= .65 ? Green : Gold);
    metric(765, "FOOD RESERVE", std::to_string(int(sim.food)));
    metric(947, std::string(sim.season()) + " / DAY", std::to_string(sim.day));
    button(1119, 23, 65, 40, paused ? "Play" : "Pause", 201, paused);
    button(1191, 23, 60, 40, std::to_string(speed) + "×", 202);
    button(1258, 23, 120, 40, "Town menu", 203);
    text("Offline · learning locally", 1121, 77, 12, Green);
    double net = sim.stats.income - sim.stats.expenses;
    text((net >= 0 ? "+" : "") + std::to_string(int(net)) + " / day", 279, 82, 11,
         net >= 0 ? Green : Red);
    text(std::to_string(sim.stats.capacity) + " connected beds", 442, 82, 11, Muted);
    text("Goal: 65% or higher", 596, 82, 11, Muted);
    text(std::to_string(int(sim.stats.foodProduction)) + " grown / " +
             std::to_string(int(sim.stats.foodUse)) + " used",
         765, 82, 11, Muted);
    rect(250, 820, 850, 80, Cream);
    if (toastTime > 0) {
        text("TOWN NOTICE", 273, 834, 10, Green);
        wrapped(toast, 273, 854, 674, 14, Ink);
    } else {
        text(sim.won ? "INDEPENDENCE EARNED" : "COUNCIL BRIEFING", 273, 834, 10, Green);
        wrapped(sim.advice(), 273, 854, 674, 14, Ink);
    }
    if (paused && !menu && !help && !journal) {
        rect(602, 123, 144, 27, {29, 66, 77, 220});
        text("TIME IS PAUSED", 618, 128, 11, Paper);
    }
    button(968, 833, 113, 44, "Journal / J", 600);
    modal();
}
void Game::apply(int i) {
    if (i < 0)
        return;
    selected = i;
    if (tool == 0)
        return;
    if (tool == 10) {
        notify(sim.demolish(i % MapSize, i / MapSize));
        return;
    }
    notify(sim.build(i % MapSize, i / MapSize, Building(tool)));
}
void Game::action(int id) {
    if (id >= 100 && id <= 110) {
        tool = id - 100;
        return;
    }
    switch (id) {
    case 201:
        paused = !paused;
        elapsed = 0;
        break;
    case 202:
        speed = speed == 1 ? 2 : speed == 2 ? 4 : 1;
        break;
    case 203:
        menu = true;
        confirmNew = confirmLoad = false;
        break;
    case 301:
        zoom = std::max(.55f, zoom - .1f);
        break;
    case 302:
        zoom = std::min(2.2f, zoom + .1f);
        break;
    case 303:
        camX = 690;
        camY = 170;
        zoom = 1;
        break;
    case 304:
        overlay = !overlay;
        break;
    case 401:
        sim.tax = std::max(5, sim.tax - 1);
        sim.refresh();
        break;
    case 402:
        sim.tax = std::min(20, sim.tax + 1);
        sim.refresh();
        break;
    case 410:
    case 411:
    case 412:
        cohort = id - 410;
        break;
    case 501:
        help = false;
        menu = false;
        paused = false;
        elapsed = 0;
        break;
    case 502:
        help = false;
        menu = false;
        paused = true;
        break;
    case 503:
        menu = false;
        break;
    case 504:
        help = true;
        break;
    case 505:
        notify(sim.save(savePath));
        break;
    case 506:
        if (confirmLoad) {
            notify(sim.load(savePath));
            confirmLoad = false;
            elapsed = 0;
            paused = true;
        } else {
            confirmLoad = true;
            confirmNew = false;
        }
        break;
    case 507:
        if (confirmNew) {
            sim.reset(sim.seed + 1);
            selected = Simulation::index(12, 13);
            paused = true;
            elapsed = 0;
            menu = false;
            confirmNew = false;
            action(303);
            toast = "A new island awaits.";
            toastTime = 5;
        } else {
            confirmNew = true;
            confirmLoad = false;
        }
        break;
    case 509:
        if (confirmQuit)
            quit = true;
        else
            confirmQuit = true;
        break;
    case 510:
        journal = false;
        break;
    case 600:
        journal = true;
        break;
    case 508: {
        auto r = sim.save(savePath);
        notify(r);
        if (r.ok)
            quit = true;
        break;
    }
    }
}
void Game::handle(const SDL_Event &e) {
    if (e.type == SDL_QUIT) {
        auto r = sim.save(savePath);
        if (r.ok)
            quit = true;
        else {
            notify(r);
            menu = true;
        }
        return;
    }
    if (e.type == SDL_WINDOWEVENT && e.window.event == SDL_WINDOWEVENT_FOCUS_LOST) {
        painting = drag = false;
        lastPaint = -1;
    }
    if (e.type == SDL_KEYDOWN && !e.key.repeat) {
        auto k = e.key.keysym.sym;
        if (k == SDLK_ESCAPE) {
            if (journal) {
                journal = false;
                return;
            }
            if (help) {
                help = false;
                menu = true;
            } else
                menu = !menu;
            confirmNew = confirmLoad = confirmQuit = false;
            return;
        }
        if (k == SDLK_F5) {
            notify(sim.save(savePath));
            return;
        }
        if (k == SDLK_F9) {
            menu = true;
            help = false;
            action(506);
            return;
        }
        if (k == SDLK_j) {
            journal = !journal;
            return;
        }
        if (k == SDLK_h) {
            help = !help;
            return;
        }
        if (menu || help || journal || sim.lost)
            return;
        if (k == SDLK_SPACE)
            action(201);
        else if (k >= SDLK_1 && k <= SDLK_9)
            tool = int(k - SDLK_0);
        else if (k == SDLK_i)
            tool = 0;
        else if (k == SDLK_b)
            tool = 10;
        else if (k == SDLK_TAB)
            overlay = !overlay;
        else if (k == SDLK_HOME)
            action(303);
        else if (k == SDLK_EQUALS || k == SDLK_PLUS)
            action(202);
        else if (k == SDLK_MINUS)
            speed = speed == 4 ? 2 : 1;
    }
    if (e.type == SDL_MOUSEBUTTONDOWN) {
        if (e.button.button == SDL_BUTTON_LEFT) {
            SDL_Point p{e.button.x, e.button.y};
            // Modal buttons are drawn last and take precedence over the map.
            for (auto it = buttons.rbegin(); it != buttons.rend(); ++it)
                if (SDL_PointInRect(&p, &it->rect)) {
                    if (((menu || help || journal || sim.lost) && it->action < 500) ||
                        ((menu || help || journal || sim.lost) && it->action == 600))
                        return;
                    action(it->action);
                    return;
                }
            if (menu || help || journal || sim.lost)
                return;
            int i = pick(p.x, p.y);
            apply(i);
            painting = tool == 1;
            lastPaint = i;
        } else if (!menu && !help && !journal && e.button.button == SDL_BUTTON_RIGHT) {
            tool = 0;
            selected = pick(e.button.x, e.button.y);
        } else if (!menu && !help && !journal && e.button.button == SDL_BUTTON_MIDDLE)
            drag = true;
    }
    if (e.type == SDL_MOUSEBUTTONUP) {
        if (e.button.button == SDL_BUTTON_LEFT) {
            painting = false;
            lastPaint = -1;
        }
        if (e.button.button == SDL_BUTTON_MIDDLE)
            drag = false;
    }
    if (e.type == SDL_MOUSEMOTION) {
        hover = pick(e.motion.x, e.motion.y);
        if (menu || help || journal)
            return;
        if (drag) {
            camX += e.motion.xrel;
            camY += e.motion.yrel;
        }
        if (painting && hover >= 0 && hover != lastPaint) {
            // Motion events can skip tiles. Fill an orthogonal path so a fast
            // drag still creates a connected street, including diagonal drags.
            if (lastPaint >= 0) {
                int x = lastPaint % MapSize, y = lastPaint / MapSize;
                int tx = hover % MapSize, ty = hover / MapSize;
                while (x != tx || y != ty) {
                    if (std::abs(tx - x) >= std::abs(ty - y) && x != tx)
                        x += tx > x ? 1 : -1;
                    else
                        y += ty > y ? 1 : -1;
                    apply(Simulation::index(x, y));
                }
            } else
                apply(hover);
            lastPaint = hover;
        }
    }
    if (e.type == SDL_MOUSEWHEEL && !menu && !help && !journal) {
        zoom = std::clamp(zoom + e.wheel.y * .08f, .55f, 2.2f);
    }
}
void Game::screenshot(const std::filesystem::path &path) {
    int w, h;
    SDL_GetRendererOutputSize(renderer, &w, &h);
    SDL_Surface *s = SDL_CreateRGBSurfaceWithFormat(0, w, h, 32, SDL_PIXELFORMAT_ARGB8888);
    if (!s)
        throw std::runtime_error(SDL_GetError());
    if (SDL_RenderReadPixels(renderer, nullptr, s->format->format, s->pixels, s->pitch) != 0) {
        SDL_FreeSurface(s);
        throw std::runtime_error(SDL_GetError());
    }
    int result = SDL_SaveBMP(s, path.string().c_str());
    SDL_FreeSurface(s);
    if (result != 0)
        throw std::runtime_error(SDL_GetError());
}
void Game::smokeStep(int frame) {
    auto click = [&](int x, int y) {
        SDL_Event e{};
        e.type = SDL_MOUSEBUTTONDOWN;
        e.button.button = SDL_BUTTON_LEFT;
        e.button.x = x;
        e.button.y = y;
        SDL_PushEvent(&e);
        e.type = SDL_MOUSEBUTTONUP;
        SDL_PushEvent(&e);
    };
    auto key = [&](SDL_Keycode k) {
        SDL_Event e{};
        e.type = SDL_KEYDOWN;
        e.key.keysym.sym = k;
        SDL_PushEvent(&e);
    };
    if (frame == 1)
        click(810, 686); // explore paused
    if (frame == 3)
        key(SDLK_2);
    auto p = tilePoint(10, 13);
    if (frame == 5)
        click(int(p.x), int(p.y));
    if (frame == 7) {
        smokeBuilt = sim.tiles[Simulation::index(10, 13)].building == Building::Home;
        key(SDLK_F5);
    }
    if (frame == 9) {
        smokeSaved = std::filesystem::exists(savePath);
        key(SDLK_b);
    }
    if (frame == 11)
        click(int(p.x), int(p.y));
    if (frame == 13) {
        smokeDemo = sim.tiles[Simulation::index(10, 13)].building == Building::None;
        key(SDLK_F9);
    }
    if (frame == 15)
        click(815, 470);
    if (frame == 17) {
        smokeLoaded = sim.tiles[Simulation::index(10, 13)].building == Building::Home;
        key(SDLK_ESCAPE);
    }
    if (frame == 19) {
        key(SDLK_TAB);
        key(SDLK_SPACE);
    }
    if (frame == 22)
        key(SDLK_j);
    if (frame == 24) {
        smokeJournal = journal;
        key(SDLK_ESCAPE);
    }
    if (frame == 26) {
        key(SDLK_EQUALS);
        key(SDLK_EQUALS);
    }
    if (frame == 48)
        key(SDLK_SPACE);
    if (frame == 49)
        key(SDLK_1);
    if (frame == 50) {
        auto start = tilePoint(8, 12), end = tilePoint(10, 12);
        SDL_Event e{};
        e.type = SDL_MOUSEBUTTONDOWN;
        e.button.button = SDL_BUTTON_LEFT;
        e.button.x = int(start.x);
        e.button.y = int(start.y);
        SDL_PushEvent(&e);
        e = {};
        e.type = SDL_MOUSEMOTION;
        e.motion.x = int(end.x);
        e.motion.y = int(end.y);
        SDL_PushEvent(&e);
        e = {};
        e.type = SDL_MOUSEBUTTONUP;
        e.button.button = SDL_BUTTON_LEFT;
        SDL_PushEvent(&e);
    }
    if (frame == 52) {
        smokeRoads = true;
        for (int x = 8; x <= 10; ++x)
            smokeRoads &= sim.tiles[Simulation::index(x, 12)].building == Building::Road;
        key(SDLK_i);
    }
}
int Game::run() {
    if (SDL_Init(SDL_INIT_VIDEO | SDL_INIT_TIMER) != 0) {
        std::cerr << SDL_GetError() << '\n';
        return 1;
    }
    if (TTF_Init() != 0) {
        std::cerr << TTF_GetError() << '\n';
        return 1;
    }
    SDL_SetHint(SDL_HINT_RENDER_SCALE_QUALITY, "1");
    window = SDL_CreateWindow("Tidemind — a city that learns to live", SDL_WINDOWPOS_CENTERED,
                              SDL_WINDOWPOS_CENTERED, W, H,
                              SDL_WINDOW_RESIZABLE | SDL_WINDOW_ALLOW_HIGHDPI);
    if (!window) {
        std::cerr << SDL_GetError() << '\n';
        return 1;
    }
    SDL_SetWindowMinimumSize(window, 1000, 650);
    renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_ACCELERATED | SDL_RENDERER_PRESENTVSYNC);
    if (!renderer)
        renderer = SDL_CreateRenderer(window, -1, SDL_RENDERER_SOFTWARE);
    if (!renderer) {
        std::cerr << SDL_GetError() << '\n';
        return 1;
    }
    SDL_SetRenderDrawBlendMode(renderer, SDL_BLENDMODE_BLEND);
    SDL_RenderSetLogicalSize(renderer, W, H);
    char *base = SDL_GetBasePath();
    std::filesystem::path dir = base ? base : ".";
    SDL_free(base);
    for (auto p :
         {dir / "assets/fonts/NotoSans.ttf", dir / "../share/tidemind/assets/fonts/NotoSans.ttf",
          std::filesystem::path(TIDEMIND_ASSETS) / "fonts/NotoSans.ttf"})
        if (std::filesystem::exists(p)) {
            fontPath = p.string();
            break;
        }
    if (fontPath.empty()) {
        std::cerr << "Bundled font missing. Keep the assets folder beside tidemind.\n";
        return 1;
    }
    if (!options.savePath.empty())
        savePath = options.savePath;
    else if (options.smoke)
        savePath = std::filesystem::temp_directory_path() / "tidemind-ui-smoke.save";
    else {
        char *pref = SDL_GetPrefPath("Tidemind", "Tidemind");
        if (!pref) {
            std::cerr << "Cannot create save directory: " << SDL_GetError() << '\n';
            return 1;
        }
        savePath = std::filesystem::path(pref) / "town.save";
        SDL_free(pref);
    }
    if (!options.smoke && std::filesystem::exists(savePath)) {
        auto result = sim.load(savePath);
        notify(result);
        if (result.ok)
            help = false;
    }
    Uint64 previous = SDL_GetPerformanceCounter();
    int frame = 0;
    while (!quit) {
        Uint64 now = SDL_GetPerformanceCounter();
        double dt = options.smoke
                        ? .1
                        : std::min(.1, double(now - previous) / SDL_GetPerformanceFrequency());
        previous = now;
        clock += dt;
        if (options.smoke)
            smokeStep(frame);
        SDL_Event e;
        while (SDL_PollEvent(&e))
            handle(e);
        if (!paused && !menu && !help && !journal && !sim.lost) {
            elapsed += dt * speed;
            if (elapsed >= 1.5) {
                elapsed -= 1.5;
                std::string prior = sim.events.empty() ? "" : sim.events.back();
                sim.tick();
                if (!sim.events.empty() && sim.events.back() != prior)
                    notify({true, sim.events.back()});
                if (sim.day % 30 == 0) {
                    auto result = sim.save(savePath);
                    if (!result.ok)
                        notify(result);
                }
            }
        }
        if (!menu && !help && !journal) {
            auto keys = SDL_GetKeyboardState(nullptr);
            float amount = float(dt) * 270;
            camX += (keys[SDL_SCANCODE_A] || keys[SDL_SCANCODE_LEFT] ? amount : 0) -
                    (keys[SDL_SCANCODE_D] || keys[SDL_SCANCODE_RIGHT] ? amount : 0);
            camY += (keys[SDL_SCANCODE_W] || keys[SDL_SCANCODE_UP] ? amount : 0) -
                    (keys[SDL_SCANCODE_S] || keys[SDL_SCANCODE_DOWN] ? amount : 0);
        }
        camX = std::clamp(camX, -500.f, 1800.f);
        camY = std::clamp(camY, -900.f, 900.f);
        toastTime = std::max(0.0, toastTime - dt);
        draw();
        ++frame;
        bool lastFrame =
            (options.frames && frame >= options.frames) || (options.smoke && frame >= 54);
        if (lastFrame && !options.screenshot.empty())
            screenshot(options.screenshot);
        SDL_RenderPresent(renderer);
        if ((options.frames && frame >= options.frames) || (options.smoke && frame >= 54)) {
            if (options.smoke) {
                bool ok = smokeBuilt && smokeSaved && smokeDemo && smokeLoaded && smokeJournal &&
                          paused && sim.day > 1 && sim.brain.samples > sim.initialTraining;
                std::cout << "UI smoke: build=" << smokeBuilt << " save=" << smokeSaved
                          << " demolish=" << smokeDemo << " load=" << smokeLoaded
                          << " roads=" << smokeRoads << " journal=" << smokeJournal
                          << " timer day=" << sim.day
                          << " local samples=" << sim.brain.samples - sim.initialTraining << '\n';
                return ok ? 0 : 1;
            }
            break;
        }
        SDL_Delay(1);
    }
    return 0;
}
} // namespace
int runGame(const Options &options) {
    try {
        Game game(options);
        return game.run();
    } catch (const std::exception &e) {
        std::cerr << "Tidemind: " << e.what() << '\n';
        return 1;
    }
}
} // namespace tide
