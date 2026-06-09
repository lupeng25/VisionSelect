#ifndef RESULTSPAGE_H
#define RESULTSPAGE_H

#include "core/SelectionTypes.h"

#include <QWidget>
#include <QVector>

class QLabel;
class QHBoxLayout;
class QTableWidget;
class QTextEdit;

class ResultsPage : public QWidget
{
    Q_OBJECT

public:
    explicit ResultsPage(QWidget *parent = nullptr);
    void setResults(const QVector<SelectionResult> &results,
                    const SelectionRequest &request);

signals:
    void exportPdfRequested();
    void exportBomRequested();

private:
    QVector<SelectionResult> m_results;
    QLabel *m_summaryLabel = nullptr;
    QHBoxLayout *m_cardsLayout = nullptr;
    QTableWidget *m_table = nullptr;
    QTextEdit *m_details = nullptr;
    void refreshCards(const SelectionRequest &request);
    void refreshTable(const SelectionRequest &request);
    void refreshDetails(int row);
};

#endif
