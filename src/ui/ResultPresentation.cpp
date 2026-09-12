#include "ui/ResultPresentation.h"

#include <QSet>
#include <QtMath>

QVector<ResultPresentation> buildResultPresentations(const QVector<SelectionResult> &results)
{
    double bestCompatibleScore = 0.0;
    for (const SelectionResult &source : results) {
        const SelectionResult result = localizedResult(source);
        if (result.hardConstraintsPassed)
            bestCompatibleScore = qMax(bestCompatibleScore, result.score.score);
    }

    QVector<ResultPresentation> presentations;
    presentations.reserve(results.size());
    for (const SelectionResult &source : results) {
        const SelectionResult result = localizedResult(source);
        ResultPresentation presentation;
        presentation.compatible = result.hardConstraintsPassed;
        presentation.needsConfirmation = result.checks.unknown();
        presentation.matchAvailable = presentation.compatible && bestCompatibleScore > 0.0;
        if (presentation.matchAvailable) {
            presentation.relativeMatchPercent = qBound(
                0, qRound(100.0 * qMax(0.0, result.score.score) / bestCompatibleScore), 100);
        }

        QSet<QString> seen;
        const auto appendUnique = [&presentation, &seen](const QStringList &values) {
            for (const QString &value : values) {
                const QString trimmed = value.trimmed();
                if (!trimmed.isEmpty() && !seen.contains(trimmed)) {
                    seen.insert(trimmed);
                    presentation.riskItems.append(trimmed);
                }
            }
        };
        appendUnique(candidateCheckMessages(result.checks, CandidateCheckState::Failed));
        appendUnique(result.hardFailures);
        appendUnique(candidateCheckMessages(result.checks, CandidateCheckState::Unknown));
        appendUnique(result.score.risks);
        presentation.riskLevel = !result.hardFailures.isEmpty()
            ? ResultRiskLevel::Error
            : (presentation.riskItems.isEmpty() ? ResultRiskLevel::None : ResultRiskLevel::Warning);
        presentations.append(presentation);
    }
    return presentations;
}
