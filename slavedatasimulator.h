#pragma once

#include <QString>
#include <QVector>
#include <QtGlobal>

enum class SlaveBitMode {
    Random,
    Chase,
    Blink,
    Alternate,
    AllOn,
    AllOff
};

enum class SlaveRegisterMode {
    Random,
    Sine,
    Square,
    Triangle,
    Counter,
    Sawtooth,
    ThermalNoise,
    RandomWalk,
    Pulse,
    DampedSine
};

struct SlaveSimulationConfig {
    int startAddress = 0;
    int count = 20;
    quint16 minimum = 0;
    quint16 maximum = 1000;
    int stepsPerPeriod = 20;
    quint16 step = 1;
};

class SlaveDataSimulator final
{
public:
    static bool validate(const SlaveSimulationConfig &config,
                         QString *errorMessage = nullptr);
    static QVector<quint8> generateBits(SlaveBitMode mode,
                                        const SlaveSimulationConfig &config,
                                        quint64 tick, bool reverse = false,
                                        int onProbability = 50);
    static QVector<quint16> generateRegisters(SlaveRegisterMode mode,
                                              const SlaveSimulationConfig &config,
                                              quint64 tick,
                                              const QVector<quint16> &previousValues = {});
};
