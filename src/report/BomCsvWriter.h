#ifndef BOMCSVWRITER_H
#define BOMCSVWRITER_H
#include "core/SelectionTypes.h"
class QIODevice;
class BomCsvWriter {
public:
    bool write(const QString &path, const SelectionRequest &request, const QVector<SelectionResult> &results, QString *error = nullptr) const;
    bool writeToDevice(QIODevice *device, const SelectionRequest &request, const QVector<SelectionResult> &results, QString *error = nullptr) const;
};
#endif
