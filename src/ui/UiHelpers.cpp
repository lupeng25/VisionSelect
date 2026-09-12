#include "ui/UiHelpers.h"

#include "core/Localization.h"
#include "i18n/LanguageManager.h"
#include "ui/UiSettings.h"
#include "ui/UiThemeManager.h"

#include <QAbstractItemView>
#include <QApplication>
#include <QClipboard>
#include <QComboBox>
#include <QCoreApplication>
#include <QDoubleSpinBox>
#include <QFrame>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QIcon>
#include <QKeySequence>
#include <QLabel>
#include <QList>
#include <QPushButton>
#include <QPainter>
#include <QPainterPath>
#include <QSize>
#include <QSizePolicy>
#include <QShortcut>
#include <QSpinBox>
#include <QStringList>
#include <QTableWidget>
#include <QTableWidgetItem>
#include <QVBoxLayout>
#include <QWidget>

namespace UiHelpers {

QIcon uiIcon(const QString &name, const QColor &color)
{
    // 统一使用矢量线形图标，并提供高分屏像素密度。
    QPixmap pixmap(48, 48);
    pixmap.setDevicePixelRatio(2);
    pixmap.fill(Qt::transparent);
    QPainter p(&pixmap);
    p.setRenderHint(QPainter::Antialiasing);
    const QColor ink = UiThemeManager::instance().highContrast()
        ? QApplication::palette().color(QPalette::ButtonText) : color;
    p.setPen(QPen(ink, 1.6, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
    p.setBrush(Qt::NoBrush);
    if (name.contains(QLatin1String("requirement"))) {
        p.drawRoundedRect(QRectF(5, 3, 14, 18), 2, 2);
        p.drawLine(9, 8, 15, 8);
        p.drawLine(9, 12, 15, 12);
        p.drawLine(9, 16, 13, 16);
    } else if (name.contains(QLatin1String("camera3d"))) {
        QPainterPath path;
        path.moveTo(12, 2); path.lineTo(21, 7); path.lineTo(21, 17);
        path.lineTo(12, 22); path.lineTo(3, 17); path.lineTo(3, 7); path.closeSubpath();
        path.moveTo(3, 7); path.lineTo(12, 12); path.lineTo(21, 7);
        path.moveTo(12, 12); path.lineTo(12, 22);
        p.drawPath(path);
    } else if (name.contains(QLatin1String("catalog"))) {
        p.drawEllipse(QRectF(4, 3, 16, 6));
        p.drawLine(4, 6, 4, 18); p.drawLine(20, 6, 20, 18);
        p.drawArc(QRectF(4, 9, 16, 6), 180 * 16, 180 * 16);
        p.drawArc(QRectF(4, 15, 16, 6), 180 * 16, 180 * 16);
    } else if (name.contains(QLatin1String("results"))) {
        p.drawRoundedRect(QRectF(3, 3, 18, 18), 3, 3);
        p.drawLine(7, 16, 7, 12); p.drawLine(12, 16, 12, 8); p.drawLine(17, 16, 17, 10);
    } else if (name.contains(QLatin1String("assistant"))) {
        p.drawEllipse(QRectF(4, 3, 13, 13));
        p.drawLine(15, 15, 21, 21);
        p.drawLine(8, 9, 13, 9); p.drawLine(10, 7, 10, 12);
    } else if (name.contains(QLatin1String("reset"))) {
        p.drawArc(QRectF(4, 4, 16, 16), 45 * 16, -285 * 16);
        p.drawLine(3, 4, 3, 10); p.drawLine(3, 10, 9, 10);
    } else if (name.contains(QLatin1String("calculate"))) {
        p.drawRoundedRect(QRectF(4, 2, 16, 20), 2, 2);
        p.drawLine(8, 7, 16, 7);
        for (int y : {12, 17}) for (int x : {8, 12, 16}) p.drawPoint(x, y);
    } else if (name.contains(QLatin1String("export"))) {
        p.drawLine(12, 3, 12, 15); p.drawLine(8, 11, 12, 15); p.drawLine(16, 11, 12, 15);
        QPainterPath path;
        path.moveTo(4, 15); path.lineTo(4, 21); path.lineTo(20, 21); path.lineTo(20, 15);
        p.drawPath(path);
    } else {
        p.drawEllipse(QRectF(3, 3, 18, 18));
        p.drawLine(12, 11, 12, 17); p.drawPoint(12, 7);
    }
    p.end();
    return QIcon(pixmap);
}

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

QWidget *pageHeader(const QString &title, const QString &subtitle, QWidget *actions)
{
    QWidget *header = new QWidget;
    header->setObjectName(QStringLiteral("PageHeader"));
    QHBoxLayout *layout = new QHBoxLayout(header);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(12);

    QWidget *copy = new QWidget(header);
    copy->setProperty("headerSurface", true);
    QVBoxLayout *copyLayout = new QVBoxLayout(copy);
    copyLayout->setContentsMargins(0, 0, 0, 0);
    copyLayout->setSpacing(4);
    QLabel *titleLabel = pageTitle(title);
    copyLayout->addWidget(titleLabel);
    if (!subtitle.isEmpty()) {
        QLabel *subtitleLabel = new QLabel(subtitle);
        subtitleLabel->setObjectName(QStringLiteral("PageSubtitle"));
        subtitleLabel->setWordWrap(true);
        subtitleLabel->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        copyLayout->addWidget(subtitleLabel);
    }
    layout->addWidget(copy, 1);
    if (actions) {
        actions->setProperty("headerSurface", true);
        layout->addWidget(actions, 0, Qt::AlignRight | Qt::AlignVCenter);
    }
    return header;
}

QFrame *metricCard(const QString &label, const QString &value, const QString &detail, const QString &state)
{
    QFrame *card = new QFrame;
    card->setObjectName(QStringLiteral("MetricCard"));
    setWidgetState(card, state);
    QVBoxLayout *layout = new QVBoxLayout(card);
    layout->setContentsMargins(14, 12, 14, 12);
    layout->setSpacing(4);

    QLabel *labelWidget = new QLabel(label);
    labelWidget->setObjectName(QStringLiteral("MetricLabel"));
    labelWidget->setWordWrap(true);
    QLabel *valueWidget = new QLabel(value);
    valueWidget->setObjectName(QStringLiteral("MetricValue"));
    valueWidget->setWordWrap(true);
    valueWidget->setTextInteractionFlags(Qt::TextSelectableByMouse);
    layout->addWidget(labelWidget);
    layout->addWidget(valueWidget);
    if (!detail.isEmpty()) {
        QLabel *detailWidget = new QLabel(detail);
        detailWidget->setObjectName(QStringLiteral("MetricDetail"));
        detailWidget->setWordWrap(true);
        detailWidget->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Preferred);
        layout->addWidget(detailWidget);
    }
    return card;
}

QLabel *statusBadge(const QString &text, const QString &state)
{
    QLabel *badge = new QLabel(text);
    badge->setObjectName(QStringLiteral("StatusBadge"));
    badge->setAlignment(Qt::AlignCenter);
    badge->setWordWrap(false);
    badge->setToolTip(text);
    setWidgetState(badge, state);
    return badge;
}

QPushButton *actionButton(const QString &text, const QString &iconPath, bool secondary)
{
    QPushButton *button = new QPushButton(text);
    button->setObjectName(secondary ? QStringLiteral("SecondaryButton") : QStringLiteral("PrimaryButton"));
    button->setCursor(Qt::PointingHandCursor);
    if (!iconPath.isEmpty()) {
        button->setIcon(uiIcon(iconPath, secondary ? QColor("#707d9a") : QColor("#ffffff")));
        button->setIconSize(QSize(16, 16));
        QObject::connect(&UiThemeManager::instance(), &UiThemeManager::themeChanged, button,
            [button, iconPath, secondary]() {
                button->setIcon(uiIcon(iconPath, secondary ? QColor("#707d9a") : QColor("#ffffff")));
            });
    }
    return button;
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

namespace {
class NumericTableItem : public QTableWidgetItem {
public:
    NumericTableItem(const QString &text, std::optional<double> value)
        : QTableWidgetItem(text), m_value(value) {}
    bool operator<(const QTableWidgetItem &other) const override
    {
        const auto *numeric = dynamic_cast<const NumericTableItem *>(&other);
        if (!numeric) return QTableWidgetItem::operator<(other);
        if (m_value.has_value() != numeric->m_value.has_value()) {
            const bool descending = tableWidget()
                && tableWidget()->horizontalHeader()->sortIndicatorOrder() == Qt::DescendingOrder;
            return m_value ? !descending : descending;
        }
        if (m_value && *m_value != *numeric->m_value) return *m_value < *numeric->m_value;
        return QTableWidgetItem::operator<(other);
    }
private:
    std::optional<double> m_value;
};
}

QTableWidgetItem *numericItem(const QString &text, std::optional<double> value)
{
    auto *tableItem = new NumericTableItem(text, value);
    tableItem->setFlags(tableItem->flags() & ~Qt::ItemIsEditable);
    tableItem->setToolTip(text);
    return tableItem;
}

QString candidateStatusText(const CandidateChecks &checks, bool hardPassed)
{
    if (!hardPassed || checks.failed()) return localizedText("不满足", "Failed");
    if (checks.unknown()) return localizedText("待确认", "Pending");
    return localizedText("初筛通过", "Screened");
}

void decorateCandidateStatus(QTableWidgetItem *item, const CandidateChecks &checks, bool hardPassed)
{
    QFont font = item->font();
    font.setBold(true);
    item->setFont(font);
    if (!UiThemeManager::instance().highContrast())
        item->setForeground(QColor(!hardPassed || checks.failed() ? "#b42318" : checks.unknown() ? "#966000" : "#176641"));
}

QString candidateChecksHtml(const CandidateChecks &checks)
{
    QString html = QStringLiteral("<table cellspacing='4' cellpadding='2'>");
    for (const auto state : {CandidateCheckState::Failed, CandidateCheckState::Unknown, CandidateCheckState::Passed}) {
        for (size_t i = 0; i < checks.states.size(); ++i) {
            if (checks.states[i] != state) continue;
            html += QStringLiteral("<tr><td><b>%1</b></td><td>%2</td></tr>")
                .arg(candidateCheckStateLabel(state).toHtmlEscaped(),
                     candidateCheckLabel(static_cast<CandidateCheck>(i)).toHtmlEscaped());
        }
    }
    return html + QStringLiteral("</table>");
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
    table->setShowGrid(false);
    table->setAlternatingRowColors(true);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->verticalHeader()->setVisible(false);
    table->verticalHeader()->setDefaultSectionSize(UiSettings::tableRowHeight());
    table->horizontalHeader()->setStretchLastSection(true);
    table->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    table->setHorizontalScrollMode(QAbstractItemView::ScrollPerPixel);
    table->setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);
    table->setTextElideMode(Qt::ElideRight);
    table->setWordWrap(false);
    table->setSortingEnabled(true);
    installTableCopyShortcut(table);
}

void setWidgetState(QWidget *widget, const QString &state)
{
    if (!widget)
        return;
    widget->setProperty("state", state);
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
    return candidateStatusText(result.checks, result.hardConstraintsPassed);
}

QString riskSummary(const SelectionResult &source)
{
    const SelectionResult result = localizedResult(source);
    QStringList risks = candidateCheckMessages(result.checks, CandidateCheckState::Failed)
        + result.hardFailures + candidateCheckMessages(result.checks, CandidateCheckState::Unknown) + result.score.risks;
    risks.removeDuplicates();
    return risks.isEmpty()
        ? localizedText("无主要风险", "No major risk")
        : risks.join(localizedText("；", "; "));
}

QString exposureText(double exposureUs)
{
    return exposureUs > 0.0
        ? QStringLiteral("%1 us").arg(exposureUs, 0, 'f', 1)
        : QCoreApplication::translate("UiHelpers", "No motion constraint");
}

QString localizedText(const char *zhUtf8, const char *enUtf8)
{
    return CoreI18n::localizedText(zhUtf8, enUtf8);
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
