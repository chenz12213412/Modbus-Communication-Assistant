#pragma once

#include <QString>
#include <QtGlobal>

namespace InternalChannel {
bool validateName(const QString &name, QString *errorMessage = nullptr);
quint16 portForName(const QString &name);
}
