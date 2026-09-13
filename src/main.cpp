#include "ui.hpp"
#include <charconv>
#include <iostream>
#include <string>
int main(int argc, char **argv) {
    tide::Options o;
    for (int i = 1; i < argc; ++i) {
        std::string a = argv[i];
        if (a == "--help") {
            std::cout
                << "Tidemind — a city that learns to live\n\n"
                   "Usage: tidemind [--seed NUMBER] [--save PATH] [--smoke-test]\n"
                   "                [--frames NUMBER] [--screenshot PATH.bmp]\n\n"
                   "Mouse: choose a tool, click to build; drag streets; right-click inspect.\n"
                   "WASD/arrows: pan. Wheel: zoom. Space: pause. 1-9: building tools.\n"
                   "B: demolish. I: inspect. Tab: neural overlay. H: help. C: civilisations. Esc: "
                   "menu.\n"
                   "F5: save. F9: load. +/-: speed. Home: centre camera.\n";
            return 0;
        }
        if (a == "--smoke-test") {
            o.smoke = true;
            continue;
        }
        if ((a == "--seed" || a == "--frames" || a == "--save" || a == "--screenshot") &&
            i + 1 < argc) {
            std::string value = argv[++i];
            if (a == "--save")
                o.savePath = value;
            else if (a == "--screenshot")
                o.screenshot = value;
            else if (a == "--seed") {
                auto [p, ec] = std::from_chars(value.data(), value.data() + value.size(), o.seed);
                if (ec != std::errc{} || p != value.data() + value.size()) {
                    std::cerr << "Invalid seed\n";
                    return 2;
                }
            } else {
                auto [p, ec] = std::from_chars(value.data(), value.data() + value.size(), o.frames);
                if (ec != std::errc{} || p != value.data() + value.size() || o.frames < 1) {
                    std::cerr << "Invalid frame count\n";
                    return 2;
                }
            }
        } else {
            std::cerr << "Unknown or incomplete option: " << a << ". Use --help.\n";
            return 2;
        }
    }
    return tide::runGame(o);
}
