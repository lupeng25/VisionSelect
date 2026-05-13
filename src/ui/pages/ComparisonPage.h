#ifndef COMPARISONPAGE_H
#define COMPARISONPAGE_H

#include "core/SelectionTypes.h"

#include <QWidget>
#include <QVector>

class QTableWidget;
class QTextEdit;

class ComparisonPage : public QWidget
{
    Q_OBJECT

public:
    explicit ComparisonPage(QWidget *parent = nullptr);
    void setResults(const QVector<SelectionResult> &results);

signals:
    void recalculateRequested();
    void exportBomRequested();

private:
    QVector<SelectionResult> m_results;
    QTableWidget *m_table = nullptr;
    QTextEdit *m_details = nullptr;
    void refreshDetails(int row);
};

#endif
