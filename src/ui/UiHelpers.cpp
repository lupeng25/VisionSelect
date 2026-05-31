#include "ui/UiHelpers.h"

#include "i18n/LanguageManager.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QCoreApplication>
#include <QDoubleSpinBox>
#include <QHeaderView>
#include <QKeySequence>
#include <QLabel>
#include <QList>
#include <QShortcut>
#include <QSpinBox>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>

namespace UiHelpers {

QLabel *pageTitle(const QString &text, const QString &subtitle)
{
    QWidget *unused = nullptr;
    Q_UNUSED(unused)
    QLabel *label = new QLabel(text);
    label->setObjectName(QStringLiteral("PageTitle"));
    if (!subtitle.isEmpty())
        label->setToolTip(subtitle);
    return label;
}

QTableWidgetItem *item(const QString &text)
{
    QTableWidgetItem *tableItem = new QTableWidgetItem(text);
    tableItem->setFlags(tableItem->flags() ^ Qt::ItemIsEditable);
    tableItem->setToolTip(text);
    return tableItem;
}

QTableWidgetItem *indexedItem(const QString &text, int sourceIndex)
{
    QTableWidgetItem *tableItem = item(text);
    tableItem->setData(Qt::UserRole, sourceIndex);
    return tableItem;
}

int rowSourceIndex(const QTableWidget *table, int row)
{
    if (!table || row < 0 || row >= table->rowCount())
        return -1;
    const QTableWidgetItem *first = table->item(row, 0);
    if (!first)
        return -1;
    return first->data(Qt::UserRole).toInt();
}

void copySelectionToClipboard(QTableWidget *table)
{
    if (!table)
        return;

    QList<QTableWidgetSelectionRange> ranges = table->selectedRanges();
    if (ranges.isEmpty() && table->currentRow() >= 0)
        ranges.append(QTableWidgetSelectionRange(table->currentRow(), 0, table->currentRow(), table->columnCount() - 1));
    if (ranges.isEmpty())
        return;

    QStringList lines;
    for (const QTableWidgetSelectionRange &range : ranges) {
        for (int row = range.topRow(); row <= range.bottomRow(); ++row) {
            QStringList cells;
            for (int column = range.leftColumn(); column <= range.rightColumn(); ++column) {
                if (table->isColumnHidden(column))
                    continue;
                QTableWidgetItem *cell = table->item(row, column);
                cells.append(cell ? cell->text() : QString());
            }
            lines.append(cells.join(QLatin1Char('\t')));
        }
    }
    QApplication::clipboard()->setText(lines.join(QLatin1Char('\n')));
}

void installTableCopyShortcut(QTableWidget *table)
{
    QShortcut *copyShortcut = new QShortcut(QKeySequence::Copy, table);
    QObject::connect(copyShortcut, &QShortcut::activated, table, [table]() {
        copySelectionToClipboard(table);
    });
}

void setupTable(QTableWidget *table)
{
    table->setAlternatingRowColors(true);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->verticalHeader()->setVisible(false);
    table->horizontalHeader()->setStretchLastSection(true);
    table->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    table->setSortingEnabled(true);
    installTableCopyShortcut(table);
}

QString number(double value, int decimals)
{
    return QString::number(value, 'f', decimals);
}

QString productLabel(const QString &manufacturer, const QString &model)
{
    if (manufacturer.trimmed().isEmpty())
        return model;
    return manufacturer.trimmed() + QLatin1Char(' ') + model;
}

QString compatibilityText(const SelectionResult &result)
{
    return result.hardConstraintsPassed
        ? QCoreApplication::translate("UiHelpers", "Compatible")
        : QCoreApplication::translate("UiHelpers", "Not compatible");
}

QString riskSummary(const SelectionResult &result)
{
    QStringList risks = result.score.risks;
    const QString separator = QCoreApplication::translate("UiHelpers", "; ");
    if (!result.hardFailures.isEmpty())
        risks.prepend(QCoreApplication::translate("UiHelpers", "Hard mismatch: %1")
            .arg(result.hardFailures.join(separator)));
    return risks.isEmpty()
        ? QCoreApplication::translate("UiHelpers", "No major risk")
        : risks.join(separator);
}

QString exposureText(double exposureUs)
{
    return exposureUs > 0.0
        ? QStringLiteral("%1 us").arg(exposureUs, 0, 'f', 1)
        : QCoreApplication::translate("UiHelpers", "No motion constraint");
}

QString localizedText(const char *zhUtf8, const char *enUtf8)
{
    return LanguageManager::instance().currentLanguage() == QLatin1String("en_US")
        ? QString::fromUtf8(enUtf8)
        : QString::fromUtf8(zhUtf8);
}

QDoubleSpinBox *makeSpin(double min, double max, double value, const QString &suffix, int decimals)
{
    QDoubleSpinBox *spin = new QDoubleSpinBox;
    spin->setRange(min, max);
    spin->setValue(value);
    spin->setDecimals(decimals);
    spin->setSuffix(suffix);
    spin->setKeyboardTracking(false);
    return spin;
}

QDoubleSpinBox *dialogSpin(double min, double max, double value, const QString &suffix, int decimals)
{
    QDoubleSpinBox *spin = new QDoubleSpinBox;
    spin->setRange(min, max);
    spin->setDecimals(decimals);
    spin->setValue(value);
    spin->setSuffix(suffix);
    spin->setKeyboardTracking(false);
    return spin;
}

QSpinBox *dialogIntSpin(int min, int max, int value, const QString &suffix)
{
    QSpinBox *spin = new QSpinBox;
    spin->setRange(min, max);
    spin->setValue(value);
    spin->setSuffix(suffix);
    spin->setKeyboardTracking(false);
    return spin;
}

void setComboText(QComboBox *combo, const QString &text)
{
    const int index = combo->findText(text, Qt::MatchFixedString);
    if (index >= 0) {
        combo->setCurrentIndex(index);
    } else {
        combo->setEditText(text);
    }
}

QComboBox *editableCombo(const QStringList &items, const QString &value)
{
    QComboBox *combo = new QComboBox;
    combo->setEditable(true);
    combo->addItems(items);
    setComboText(combo, value);
    return combo;
}

}
