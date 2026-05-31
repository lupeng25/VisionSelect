#ifndef UIHELPERS_H
#define UIHELPERS_H

#include "core/SelectionTypes.h"

#include <QString>

class QComboBox;
class QDoubleSpinBox;
class QFrame;
class QLabel;
class QPushButton;
class QSpinBox;
class QTableWidget;
class QTableWidgetItem;
class QStringList;
class QWidget;

namespace UiHelpers {

QLabel *pageTitle(const QString &text, const QString &subtitle = QString());
QWidget *pageHeader(const QString &title, const QString &subtitle = QString(), QWidget *actions = nullptr);
QFrame *metricCard(const QString &label, const QString &value, const QString &detail = QString(), const QString &state = QString());
QLabel *statusBadge(const QString &text, const QString &state = QString());
QPushButton *actionButton(const QString &text, const QString &iconPath = QString(), bool secondary = false);
QTableWidgetItem *item(const QString &text);
QTableWidgetItem *indexedItem(const QString &text, int sourceIndex);
int rowSourceIndex(const QTableWidget *table, int row);
void copySelectionToClipboard(QTableWidget *table);
void installTableCopyShortcut(QTableWidget *table);
void setupTable(QTableWidget *table);
void setWidgetState(QWidget *widget, const QString &state);
QString number(double value, int decimals = 2);
QString productLabel(const QString &manufacturer, const QString &model);
QString compatibilityText(const SelectionResult &result);
QString riskSummary(const SelectionResult &result);
QString exposureText(double exposureUs);
QString localizedText(const char *zhUtf8, const char *enUtf8);
QDoubleSpinBox *makeSpin(double min, double max, double value, const QString &suffix, int decimals = 2);
QDoubleSpinBox *dialogSpin(double min, double max, double value, const QString &suffix = QString(), int decimals = 3);
QSpinBox *dialogIntSpin(int min, int max, int value, const QString &suffix = QString());
void setComboText(QComboBox *combo, const QString &text);
QComboBox *editableCombo(const QStringList &items, const QString &value);

}

#endif
