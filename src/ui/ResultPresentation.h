#ifndef RESULTPRESENTATION_H
#define RESULTPRESENTATION_H

#include "core/SelectionTypes.h"

#include <QVector>

enum class ResultRiskLevel {
    None,
    Warning,
    Error
};

struct ResultPresentation {
    bool compatible = false;
    bool needsConfirmation = false;
    bool matchAvailable = false;
    int relativeMatchPercent = 0;
    QStringList riskItems;
    ResultRiskLevel riskLevel = ResultRiskLevel::None;
};

QVector<ResultPresentation> buildResultPresentations(const QVector<SelectionResult> &results);

#endif
