#ifndef REPORTPAGE_H
#define REPORTPAGE_H

#include "core/SelectionTypes.h"

#include <QVector>
#include <QWidget>

class QTextEdit;

class ReportPage : public QWidget
{
    Q_OBJECT

public:
    explicit ReportPage(QWidget *parent = nullptr);
    void setReportData(const SelectionRequest &request,
                       const QVector<SelectionResult> &results);

signals:
    void exportPdfRequested();
    void exportBomRequested();
    void recalculateRequested();

private:
    QTextEdit *m_reportPreview = nullptr;
};

#endif
