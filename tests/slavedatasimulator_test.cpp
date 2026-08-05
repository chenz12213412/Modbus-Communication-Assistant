#include "slavedatasimulator.h"

#include <QCoreApplication>
#include <QDebug>

#include <cmath>

namespace {
bool expect(bool condition, const char *message)
{
    if (!condition)
        qCritical().noquote() << message;
    return condition;
}

bool allWithin(const QVector<quint16> &values, quint16 minimum, quint16 maximum)
{
    for (quint16 value : values) {
        if (value < minimum || value > maximum)
            return false;
    }
    return true;
}
}

int main(int argc, char *argv[])
{
    QCoreApplication application(argc, argv);
    bool passed = true;

    SlaveSimulationConfig config;
    QString error;
    config.startAddress = 65530;
    config.count = 10;
    passed &= expect(!SlaveDataSimulator::validate(config, &error),
                     "range beyond 65535 must be rejected");
    config.startAddress = 20;
    config.count = 8;
    config.minimum = 100;
    config.maximum = 900;
    config.stepsPerPeriod = 8;
    passed &= expect(SlaveDataSimulator::validate(config, &error),
                     "valid simulation configuration must pass");

    auto bits = SlaveDataSimulator::generateBits(
        SlaveBitMode::Chase, config, 0, false);
    passed &= expect(bits == QVector<quint8>({1, 0, 0, 0, 0, 0, 0, 0}),
                     "forward chase must start at the first address");
    bits = SlaveDataSimulator::generateBits(SlaveBitMode::Chase, config, 1, true);
    passed &= expect(bits == QVector<quint8>({0, 0, 0, 0, 0, 0, 1, 0}),
                     "reverse chase must move from the last address");
    bits = SlaveDataSimulator::generateBits(SlaveBitMode::Blink, config, 1);
    passed &= expect(bits == QVector<quint8>(8, 1),
                     "blink must turn the full range on for odd ticks");
    bits = SlaveDataSimulator::generateBits(SlaveBitMode::Alternate, config, 1);
    passed &= expect(bits.at(0) == 0 && bits.at(1) == 1,
                     "alternate mode must swap parity each tick");
    bits = SlaveDataSimulator::generateBits(SlaveBitMode::AllOn, config, 0);
    passed &= expect(bits == QVector<quint8>(8, 1), "all-on must output ones");
    bits = SlaveDataSimulator::generateBits(SlaveBitMode::AllOff, config, 0);
    passed &= expect(bits == QVector<quint8>(8, 0), "all-off must output zeros");
    bits = SlaveDataSimulator::generateBits(
        SlaveBitMode::Random, config, 0, false, 100);
    passed &= expect(bits == QVector<quint8>(8, 1),
                     "100 percent random probability must output ones");

    const auto sineLow = SlaveDataSimulator::generateRegisters(
        SlaveRegisterMode::Sine, config, 6);
    const auto sineHigh = SlaveDataSimulator::generateRegisters(
        SlaveRegisterMode::Sine, config, 2);
    passed &= expect(allWithin(sineLow, 100, 900) && allWithin(sineHigh, 100, 900),
                     "sine values must stay within configured bounds");
    passed &= expect(sineLow.first() < sineHigh.first(),
                     "sine phase must move between low and high values");

    const auto squareLow = SlaveDataSimulator::generateRegisters(
        SlaveRegisterMode::Square, config, 0);
    const auto squareHigh = SlaveDataSimulator::generateRegisters(
        SlaveRegisterMode::Square, config, 4);
    passed &= expect(squareLow.first() == 100 && squareHigh.first() == 900,
                     "square wave must switch between min and max");

    const auto triangleStart = SlaveDataSimulator::generateRegisters(
        SlaveRegisterMode::Triangle, config, 0);
    const auto trianglePeak = SlaveDataSimulator::generateRegisters(
        SlaveRegisterMode::Triangle, config, 4);
    passed &= expect(triangleStart.first() == 100 && trianglePeak.first() == 900,
                     "triangle wave must reach configured endpoints");

    config.step = 125;
    const auto counter = SlaveDataSimulator::generateRegisters(
        SlaveRegisterMode::Counter, config, 7);
    passed &= expect(counter.first() >= 100 && counter.first() <= 900,
                     "counter must wrap inside configured bounds");

    const auto sawStart = SlaveDataSimulator::generateRegisters(
        SlaveRegisterMode::Sawtooth, config, 0);
    const auto sawEnd = SlaveDataSimulator::generateRegisters(
        SlaveRegisterMode::Sawtooth, config, 7);
    passed &= expect(sawStart.first() == 100 && sawEnd.first() > sawStart.first(),
                     "sawtooth must rise during a period");

    const auto randomRegisters = SlaveDataSimulator::generateRegisters(
        SlaveRegisterMode::Random, config, 0);
    passed &= expect(allWithin(randomRegisters, 100, 900),
                     "random registers must stay within configured bounds");

    const auto thermalNoise = SlaveDataSimulator::generateRegisters(
        SlaveRegisterMode::ThermalNoise, config, 0);
    passed &= expect(allWithin(thermalNoise, 100, 900),
                     "thermal noise must stay within configured bounds");

    const QVector<quint16> previous(8, 500);
    const auto randomWalk = SlaveDataSimulator::generateRegisters(
        SlaveRegisterMode::RandomWalk, config, 0, previous);
    for (quint16 value : randomWalk) {
        passed &= expect(value >= 375 && value <= 625,
                         "random walk must move within one configured step");
    }

    const auto pulseHigh = SlaveDataSimulator::generateRegisters(
        SlaveRegisterMode::Pulse, config, 0);
    const auto pulseLow = SlaveDataSimulator::generateRegisters(
        SlaveRegisterMode::Pulse, config, 2);
    passed &= expect(pulseHigh.first() == 900 && pulseLow.first() == 100,
                     "pulse must return a short high signal at period start");

    const auto dampedStart = SlaveDataSimulator::generateRegisters(
        SlaveRegisterMode::DampedSine, config, 0);
    const auto dampedLater = SlaveDataSimulator::generateRegisters(
        SlaveRegisterMode::DampedSine, config, 10);
    passed &= expect(dampedStart.first() > dampedLater.first()
                         && std::abs(static_cast<int>(dampedLater.first()) - 500)
                             < std::abs(static_cast<int>(dampedStart.first()) - 500),
                     "damped sine amplitude must decrease over time");

    return passed ? 0 : 1;
}
