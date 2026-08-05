#include "slavedatasimulator.h"

#include <QRandomGenerator>

#include <cmath>

namespace {
constexpr double kPi = 3.14159265358979323846;

quint16 interpolate(const SlaveSimulationConfig &config, double ratio)
{
    const double bounded = qBound(0.0, ratio, 1.0);
    const double value = config.minimum
                         + (config.maximum - config.minimum) * bounded;
    return static_cast<quint16>(qRound(value));
}
}

bool SlaveDataSimulator::validate(const SlaveSimulationConfig &config,
                                  QString *errorMessage)
{
    QString error;
    if (config.startAddress < 0 || config.startAddress > 65535) {
        error = QStringLiteral("仿真起始地址必须在 0 到 65535 之间");
    } else if (config.count < 1
               || static_cast<quint32>(config.startAddress) + config.count > 65536u) {
        error = QStringLiteral("仿真数量超出有效地址范围");
    } else if (config.minimum > config.maximum) {
        error = QStringLiteral("最小值不能大于最大值");
    } else if (config.stepsPerPeriod < 2) {
        error = QStringLiteral("波形周期至少需要两个更新点");
    } else if (config.step < 1) {
        error = QStringLiteral("递增步长必须大于 0");
    }
    if (errorMessage)
        *errorMessage = error;
    return error.isEmpty();
}

QVector<quint8> SlaveDataSimulator::generateBits(
    SlaveBitMode mode, const SlaveSimulationConfig &config, quint64 tick,
    bool reverse, int onProbability)
{
    QVector<quint8> result(config.count, 0);
    if (config.count <= 0)
        return result;

    const int probability = qBound(0, onProbability, 100);
    switch (mode) {
    case SlaveBitMode::Random:
        for (quint8 &value : result)
            value = QRandomGenerator::global()->bounded(100) < probability ? 1 : 0;
        break;
    case SlaveBitMode::Chase: {
        const int offset = static_cast<int>(tick % static_cast<quint64>(config.count));
        result[reverse ? config.count - 1 - offset : offset] = 1;
        break;
    }
    case SlaveBitMode::Blink:
        result.fill((tick % 2) ? 1 : 0);
        break;
    case SlaveBitMode::Alternate:
        for (int index = 0; index < result.size(); ++index)
            result[index] = ((static_cast<quint64>(index) + tick) % 2 == 0) ? 1 : 0;
        break;
    case SlaveBitMode::AllOn:
        result.fill(1);
        break;
    case SlaveBitMode::AllOff:
        result.fill(0);
        break;
    }
    return result;
}

QVector<quint16> SlaveDataSimulator::generateRegisters(
    SlaveRegisterMode mode, const SlaveSimulationConfig &config, quint64 tick,
    const QVector<quint16> &previousValues)
{
    QVector<quint16> result(config.count, config.minimum);
    if (config.count <= 0)
        return result;

    const quint64 period = static_cast<quint64>(qMax(2, config.stepsPerPeriod));
    const quint64 phaseTick = tick % period;
    const double phase = static_cast<double>(phaseTick) / static_cast<double>(period);
    for (int index = 0; index < result.size(); ++index) {
        switch (mode) {
        case SlaveRegisterMode::Random: {
            const quint32 span = static_cast<quint32>(config.maximum)
                                 - config.minimum + 1u;
            result[index] = static_cast<quint16>(
                config.minimum + QRandomGenerator::global()->bounded(span));
            break;
        }
        case SlaveRegisterMode::Sine: {
            const double addressPhase = result.size() > 1
                ? static_cast<double>(index) / result.size() * 0.12
                : 0.0;
            const double ratio = (std::sin(2.0 * kPi * (phase + addressPhase)) + 1.0) / 2.0;
            result[index] = interpolate(config, ratio);
            break;
        }
        case SlaveRegisterMode::Square:
            result[index] = phase < 0.5 ? config.minimum : config.maximum;
            break;
        case SlaveRegisterMode::Triangle:
            result[index] = interpolate(config,
                phase < 0.5 ? phase * 2.0 : (1.0 - phase) * 2.0);
            break;
        case SlaveRegisterMode::Counter: {
            const quint32 span = static_cast<quint32>(config.maximum)
                                 - config.minimum + 1u;
            const quint64 offset = (tick * config.step
                                    + static_cast<quint64>(index) * config.step) % span;
            result[index] = static_cast<quint16>(config.minimum + offset);
            break;
        }
        case SlaveRegisterMode::Sawtooth:
            result[index] = interpolate(config, phase);
            break;
        case SlaveRegisterMode::ThermalNoise: {
            const double midpoint = (static_cast<double>(config.minimum)
                                     + config.maximum) / 2.0;
            const double deviation = (config.maximum - config.minimum) / 6.0;
            double uniformA = QRandomGenerator::global()->generateDouble();
            double uniformB = QRandomGenerator::global()->generateDouble();
            uniformA = qMax(uniformA, 1.0e-12);
            uniformB = qMax(uniformB, 1.0e-12);
            const double gaussian = std::sqrt(-2.0 * std::log(uniformA))
                                    * std::cos(2.0 * kPi * uniformB);
            result[index] = interpolate(config,
                (midpoint + gaussian * deviation - config.minimum)
                / qMax(1.0, static_cast<double>(config.maximum - config.minimum)));
            break;
        }
        case SlaveRegisterMode::RandomWalk: {
            const quint16 previous = previousValues.size() == result.size()
                ? previousValues.at(index)
                : static_cast<quint16>((config.minimum + config.maximum) / 2);
            const int direction = QRandomGenerator::global()->bounded(2) == 0 ? -1 : 1;
            const int next = static_cast<int>(previous)
                             + direction * static_cast<int>(config.step);
            result[index] = static_cast<quint16>(qBound(
                static_cast<int>(config.minimum), next,
                static_cast<int>(config.maximum)));
            break;
        }
        case SlaveRegisterMode::Pulse: {
            const quint64 pulseTicks = qMax<quint64>(1, period / 10);
            result[index] = phaseTick < pulseTicks ? config.maximum : config.minimum;
            break;
        }
        case SlaveRegisterMode::DampedSine: {
            const double envelope = std::exp(-4.0 * phase);
            const double ratio = 0.5 + 0.5 * envelope
                                 * std::cos(2.0 * kPi * phase);
            result[index] = interpolate(config, ratio);
            break;
        }
        }
    }
    return result;
}
