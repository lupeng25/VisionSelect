#ifndef RESULTSPAGE_H
#define RESULTSPAGE_H

#include "core/SelectionTypes.h"
#include "ui/ResultPresentation.h"

#include <QWidget>
#include <QVector>

class QLabel;
class QHBoxLayout;
class QEvent;
class QTableWidget;
class QTextEdit;
class QSplitter;

class ResultsPage : public QWidget
{
    Q_OBJECT

public:
    explicit ResultsPage(QWidget *parent = nullptr);
    ~ResultsPage() override;
    void setBusy(const SelectionRequest &request);
    void setError(const QString &message);
    void setResults(const QVector<SelectionResult> &results,
                    const SelectionRequest &request);

signals:
    void exportPdfRequested();
    void exportBomRequested();
    void inputRequested();
    void retryRequested();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QVector<SelectionResult> m_results;
    QVector<ResultPresentation> m_presentations;
    QLabel *m_summaryLabel = nullptr;
    QHBoxLayout *m_cardsLayout = nullptr;
    QTableWidget *m_table = nullptr;
    QTextEdit *m_details = nullptr;
    QSplitter *m_splitter = nullptr;
    void refreshCards(const SelectionRequest &request);
    void refreshTable(const SelectionRequest &request);
    void refreshDetails(int row);
    void selectCard(int sourceIndex);
};

#endif
