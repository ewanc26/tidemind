#ifndef TIDEMIND_NEURAL_HPP
#define TIDEMIND_NEURAL_HPP
#include <array>
#include <cstdint>
#include <iosfwd>
#include <random>
namespace tide {
// Inputs: road, water, power, parks, jobs, pollution, tax, food, coast,
// followed by a one-hot encoding of three household preference cohorts.
class NeuralNetwork {
  public:
    static constexpr int Inputs = 12, Hidden = 16;
    using Features = std::array<double, Inputs>;
    explicit NeuralNetwork(uint32_t seed = 42);
    double predict(const Features &x) const;
    double train(const Features &x, double target, double rate = 0.035);
    void write(std::ostream &out) const;
    bool read(std::istream &in);
    uint64_t samples = 0;
    double loss = 0;
    std::array<double, Hidden> activations(const Features &x) const;
    static double experiencedUtility(const Features &x);

  private:
    std::array<std::array<double, Inputs>, Hidden> weights{};
    std::array<double, Hidden> biases{}, output{};
    double bias = 0;
};
} // namespace tide
#endif
