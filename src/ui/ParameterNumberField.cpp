#include "ui/ParameterNumberField.h"

#include "ui/UiHelpers.h"

#include <QComboBox>
#include <QDoubleValidator>
#include <QHBoxLayout>
#include <QIntValidator>
#include <QLabel>
#include <QLineEdit>
#include <QRegularExpression>
#include <QSignalBlocker>
#include <QVBoxLayout>
#include <cmath>
#include <limits>

ParameterNumberField::ParameterNumberField(const QString &key, const QString &title, const QString &unit,
                                         bool integer, QWidget *parent)
    : QWidget(parent), m_label(new QLabel(title)), m_edit(new QLineEdit), m_unit(unit)
{
    setObjectName(QStringLiteral("ParameterField"));
    auto *layout = new QVBoxLayout(this);
    layout->setContentsMargins(0, 0, 0, 0);
    layout->setSpacing(5);
    m_label->setWordWrap(true);
    m_label->setBuddy(m_edit);
    layout->addWidget(m_label);
    auto *row = new QHBoxLayout;
    row->setSpacing(4);
    m_edit->setObjectName(key);
    m_edit->setAccessibleName(title);
    m_edit->setPlaceholderText(UiHelpers::localizedText("未填写", "Not entered"));
    m_edit->setMinimumWidth(0);
    m_edit->setMaxLength(28);
    m_edit->setSizePolicy(QSizePolicy::Ignored, QSizePolicy::Fixed);
    if (integer) m_edit->setValidator(new QIntValidator(0, 200000, m_edit));
    else {
        auto *validator = new QDoubleValidator(0.0, 1e12, 8, m_edit);
        validator->setNotation(QDoubleValidator::StandardNotation);
        validator->setLocale(QLocale::c());
        m_edit->setValidator(validator);
    }
    row->addWidget(m_edit, 1);
    if (unit == QLatin1String("mm") || unit == QStringLiteral("μm")
        || unit == QStringLiteral("μs") || unit == QLatin1String("mm/s")) {
        m_units = new QComboBox;
        m_units->setObjectName(key + QStringLiteral(".unit"));
        m_units->setAccessibleName(title + UiHelpers::localizedText("单位", " unit"));
        m_units->addItem(unit, 1.0);
        if (unit == QLatin1String("mm")) m_units->addItem(QStringLiteral("cm"), 10.0);
        if (unit == QStringLiteral("μm")) m_units->addItem(QStringLiteral("mm"), 1000.0);
        if (unit == QStringLiteral("μs")) m_units->addItem(QStringLiteral("ms"), 1000.0);
        if (unit == QLatin1String("mm/s")) m_units->addItem(QStringLiteral("m/s"), 1000.0);
        m_units->setSizeAdjustPolicy(QComboBox::AdjustToContents);
        row->addWidget(m_units);
        connect(m_units, &QComboBox::currentIndexChanged, this, [this]() {
            const auto oldValue = value();
            m_scale = m_units->currentData().toDouble();
            if (!m_restoring && oldValue && std::isfinite(*oldValue)) setValue(oldValue);
            if (!m_restoring) emit changed();
        });
    } else if (!unit.isEmpty()) {
        auto *suffix = new QLabel(unit);
        suffix->setObjectName(QStringLiteral("FieldUnit"));
        row->addWidget(suffix);
    }
    layout->addLayout(row);
    connect(m_edit, &QLineEdit::textChanged, this, [this]() { if (!m_restoring) emit changed(); });
}

std::optional<double> ParameterNumberField::value() const
{
    if (m_edit->text().trimmed().isEmpty()) return {};
    bool ok = false;
    const double value = QLocale::c().toDouble(m_edit->text(), &ok);
    if (!ok || !m_edit->hasAcceptableInput()) return std::numeric_limits<double>::quiet_NaN();
    return value * m_scale;
}

void ParameterNumberField::setValue(std::optional<double> value)
{
    m_edit->setText(value && std::isfinite(*value)
        ? QString::number(*value / m_scale, 'f', 8).remove(QRegularExpression(QStringLiteral("\\.?0+$"))) : QString());
    // 零不能被小数尾零裁剪为空。
    if (value && *value == 0.0) m_edit->setText(QStringLiteral("0"));
}

void ParameterNumberField::setReadOnly(bool readOnly)
{
    m_edit->setReadOnly(readOnly);
    if (m_units) m_units->setEnabled(!readOnly);
}

void ParameterNumberField::setTitle(const QString &title)
{
    m_label->setText(title);
    m_edit->setAccessibleName(title);
}

QString ParameterNumberField::title() const { return m_label->text(); }
QString ParameterNumberField::displayText() const
{
    return m_edit->text().isEmpty() ? UiHelpers::localizedText("未填写", "Not entered")
        : m_edit->text() + QLatin1Char(' ') + (m_units ? m_units->currentText() : m_unit);
}
QJsonObject ParameterNumberField::state() const
{
    return {{QStringLiteral("text"), m_edit->text()},
            {QStringLiteral("unit"), m_units ? m_units->currentText() : m_unit}};
}
void ParameterNumberField::restoreState(const QJsonObject &state)
{
    m_restoring = true;
    if (m_units) {
        const int index = m_units->findText(state.value(QStringLiteral("unit")).toString());
        m_units->setCurrentIndex(index >= 0 ? index : 0);
        m_scale = m_units->currentData().toDouble();
    }
    m_edit->setText(state.value(QStringLiteral("text")).toString().left(28));
    m_restoring = false;
    emit changed();
}
