#include "neural.hpp"
#include <algorithm>
#include <cmath>
#include <istream>
#include <ostream>
namespace tide {
NeuralNetwork::NeuralNetwork(uint32_t seed) {
    std::mt19937 rng(seed);
    std::uniform_real_distribution<double> d(-0.45, 0.45);
    for (auto &row : weights)
        for (auto &w : row)
            w = d(rng);
    for (auto &w : output)
        w = d(rng);
}
std::array<double, NeuralNetwork::Hidden> NeuralNetwork::activations(const Features &x) const {
    std::array<double, Hidden> h{};
    for (int j = 0; j < Hidden; ++j) {
        double z = biases[j];
        for (int i = 0; i < Inputs; ++i)
            z += weights[j][i] * x[i];
        h[j] = std::tanh(z);
    }
    return h;
}
double NeuralNetwork::predict(const Features &x) const {
    auto h = activations(x);
    double z = bias;
    for (int j = 0; j < Hidden; ++j)
        z += output[j] * h[j];
    return 1.0 / (1.0 + std::exp(-std::clamp(z, -30.0, 30.0)));
}
double NeuralNetwork::train(const Features &x, double target, double rate) {
    auto h = activations(x);
    double y = predict(x);
    double err = y - std::clamp(target, 0.0, 1.0);
    double delta = 2 * err * y * (1 - y);
    for (int j = 0; j < Hidden; ++j) {
        double dh = delta * output[j] * (1 - h[j] * h[j]);
        for (int i = 0; i < Inputs; ++i)
            weights[j][i] -= std::clamp(rate * dh * x[i], -0.1, 0.1);
        biases[j] -= rate * dh;
        output[j] -= rate * delta * h[j];
    }
    bias -= rate * delta;
    ++samples;
    loss = samples == 1 ? err * err : loss * 0.98 + err * err * 0.02;
    return err * err;
}
double NeuralNetwork::experiencedUtility(const Features &x) {
    double parks = 0.10 + 0.11 * x[9], jobs = 0.10 + 0.13 * x[10], coast = 0.04 + 0.13 * x[11];
    return std::clamp(0.12 + 0.12 * x[0] + 0.17 * x[1] + 0.10 * x[2] + parks * x[3] + jobs * x[4] -
                          0.20 * x[5] - 0.16 * x[6] + 0.14 * x[7] + coast * x[8],
                      0.02, 0.98);
}
void NeuralNetwork::write(std::ostream &out) const {
    out << samples << ' ' << loss << ' ' << bias << '\n';
    for (int j = 0; j < Hidden; ++j) {
        out << biases[j] << ' ' << output[j];
        for (auto w : weights[j])
            out << ' ' << w;
        out << '\n';
    }
}
bool NeuralNetwork::read(std::istream &in) {
    NeuralNetwork candidate;
    if (!(in >> candidate.samples >> candidate.loss >> candidate.bias))
        return false;
    auto valid = [](double n) { return std::isfinite(n) && std::abs(n) < 1000; };
    if (candidate.samples > 1000000000000ULL || !valid(candidate.loss) || candidate.loss < 0 ||
        !valid(candidate.bias))
        return false;
    for (int j = 0; j < Hidden; ++j) {
        if (!(in >> candidate.biases[j] >> candidate.output[j]) || !valid(candidate.biases[j]) ||
            !valid(candidate.output[j]))
            return false;
        for (auto &w : candidate.weights[j])
            if (!(in >> w) || !valid(w))
                return false;
    }
    *this = candidate;
    return true;
}
} // namespace tide
