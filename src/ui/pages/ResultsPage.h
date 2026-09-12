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
class QPushButton;
class QFrame;

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
    void setRequestOutdated(bool outdated);

signals:
    void exportPdfRequested();
    void exportBomRequested();
    void inputRequested();
    void retryRequested();

protected:
    bool eventFilter(QObject *watched, QEvent *event) override;

private:
    QVector<SelectionResult> m_results;
    SelectionRequest m_resultRequest;
    QVector<ResultPresentation> m_presentations;
    QLabel *m_summaryLabel = nullptr;
    QHBoxLayout *m_cardsLayout = nullptr;
    QTableWidget *m_table = nullptr;
    QTextEdit *m_details = nullptr;
    QSplitter *m_splitter = nullptr;
    QFrame *m_cards = nullptr;
    QFrame *m_staleBanner = nullptr;
    QLabel *m_countLabel = nullptr;
    QLabel *m_selectionSummary = nullptr;
    QPushButton *m_compareButton = nullptr;
    QPushButton *m_detailsButton = nullptr;
    int m_selectedSourceIndex = -1;
    void refreshCards(const SelectionRequest &request);
    void refreshTable(const SelectionRequest &request);
    void refreshDetails(int row);
    void selectCard(int sourceIndex);
};

#endif
