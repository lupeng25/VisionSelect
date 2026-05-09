#ifndef PDFREPORTWRITER_H
#define PDFREPORTWRITER_H

#include "core/SelectionTypes.h"

#include <QString>
#include <QVector>

class PdfReportWriter
{
public:
    bool write(const QString &filePath,
               const SelectionRequest &request,
               const QVector<SelectionResult> &results,
               QString *errorMessage = nullptr) const;
};

#endif
