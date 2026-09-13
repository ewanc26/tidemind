#ifndef TIDEMIND_UI_HPP
#define TIDEMIND_UI_HPP
#include <cstdint>
#include <filesystem>
namespace tide {
struct Options {
    uint32_t seed = 7261;
    bool smoke = false;
    int frames = 0;
    std::filesystem::path screenshot, savePath;
};
int runGame(const Options &options);
} // namespace tide
#endif
