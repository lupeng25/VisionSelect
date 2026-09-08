#ifndef PURECALCULATIONPAGE_H
#define PURECALCULATIONPAGE_H

#include "core/SelectionTypes.h"
#include "selection/ParameterCalculator.h"
#include "ui/ParameterWorkspaceState.h"

#include <QMap>
#include <QWidget>

class CatalogRepository;
class FovDiagram;
class ParameterNumberField;
class QBoxLayout;
class QCheckBox;
class QComboBox;
class QFrame;
class QGridLayout;
class QLabel;
class QLineEdit;
class QPushButton;
class QScrollArea;
class QStackedWidget;
class QTabBar;
class QTableWidget;
class QTextBrowser;

class PureCalculationPage : public QWidget
{
    Q_OBJECT
public:
    explicit PureCalculationPage(QWidget *parent = nullptr);
    ParameterWorkspaceState workspaceState() const;
    void restoreWorkspaceState(const ParameterWorkspaceState &state);
    void setCatalog(const CatalogRepository *catalog);
    void importCamera(const CameraSpec &camera);
    void importLens(const LensSpec &lens);
    void setTask(const QString &task);
    QString task() const;
    void saveSnapshot(int slot);

public slots:
    void refresh();
    void resetDefaults();

protected:
    void resizeEvent(QResizeEvent *event) override;

private:
    ParameterNumberField *addNumber(QGridLayout *grid, const QString &key, const QString &label,
                                   const QString &unit, int row, int column, bool integer = false);
    QComboBox *addChoice(QGridLayout *grid, const QString &key, const QString &label,
                        const QStringList &labels, const QStringList &values, int row, int column, int span = 1);
    QCheckBox *addFlag(QGridLayout *grid, const QString &key, const QString &label, int row);
    QLineEdit *addText(QGridLayout *grid, const QString &key, const QString &label, int row, int column, int span = 1);
    QGridLayout *makePanel(const QString &description);
    void buildPanels();
    void updateVisibility();
    void fieldChanged(const QString &key);
    QString cameraSignature() const;
    void invalidateMeasuredFov();
    void clearLensSpecifications();
    void clearCurrentTask();
    void copyReport();
    void updateSnapshots();
    void chooseCamera();
    void chooseLens(bool matched);
    void applyToCheck();
    void showRows(const QStringList &headers, const QVector<QStringList> &rows);
    void metric(int index, const QString &label, Parameters::Number value, const QString &unit);
    void metricText(int index, const QString &label, const QString &text);
    void setSource(const QString &scope, const QString &type, const QString &label = QString());
    QString sourceText(const QString &scope) const;
    Parameters::Number number(const QString &key) const;
    QString choice(const QString &key) const;
    void setNumber(const QString &key, Parameters::Number value);
    void setChoice(const QString &key, const QString &value);
    QPair<Parameters::Number, Parameters::Number> targetFov(const QString &prefix) const;
    Parameters::Sensor sensor() const;
    Parameters::OpticsInput opticsInput() const;
    Parameters::SystemInput systemInput() const;
    Parameters::TelecentricInput telecentricInput() const;
    SelectionRequest candidateRequest() const;

    const CatalogRepository *m_catalog = nullptr;
    QMap<QString, ParameterNumberField *> m_fields;
    QMap<QString, QComboBox *> m_choices;
    QMap<QString, QCheckBox *> m_flags;
    QMap<QString, QLineEdit *> m_texts;
    QMap<QString, QWidget *> m_wrappers;
    QJsonObject m_provenance;
    QJsonArray m_snapshots;
    QString m_cameraMount;
    QString m_lastCameraSignature;
    bool m_loading = false;
    QTabBar *m_tasks;
    QFrame *m_cameraPanel;
    QLabel *m_cameraSource;
    QLabel *m_taskSource;
    QStackedWidget *m_inputs;
    QBoxLayout *m_columns;
    QScrollArea *m_scroll;
    QPushButton *m_importCamera;
    QPushButton *m_importLens;
    QPushButton *m_matchLens;
    QPushButton *m_applyCheck;
    QLabel *m_resultStatus;
    QLabel *m_resultNote;
    QLabel *m_metricLabels[3];
    QLabel *m_metricValues[3];
    QTableWidget *m_results;
    FovDiagram *m_diagram;
    QTextBrowser *m_compare;
    QCheckBox *m_compareToggle;
    QVector<double> m_comparisonValues;
    QString m_comparisonKind;
    QString m_report;
};

#endif
