#include "ui/UiThemeManager.h"

#include "ui/UiSettings.h"

#include <QAccessibilityHints>
#include <QApplication>
#include <QFile>
#include <QStyle>
#include <QStyleFactory>
#include <QStyleHints>
#include <QWidget>

UiThemeManager &UiThemeManager::instance()
{
    static UiThemeManager manager;
    return manager;
}

UiThemeManager::UiThemeManager(QObject *parent)
    : QObject(parent)
{
}

void UiThemeManager::initialize(QApplication *application)
{
    m_application = application;
    if (!m_application)
        return;

    m_systemPalette = m_application->palette();
    m_application->setStyle(QStyleFactory::create(QStringLiteral("Fusion")));

    const QAccessibilityHints *hints = m_application->styleHints()->accessibility();
    connect(hints, &QAccessibilityHints::contrastPreferenceChanged,
            this, [this](Qt::ContrastPreference) { applyStyleSheet(); });
    connect(&UiSettings::instance(), &UiSettings::densityChanged,
            this, [this](UiDensity) { applyStyleSheet(); });
    applyStyleSheet();
}

bool UiThemeManager::highContrast() const
{
    return m_application
        && m_application->styleHints()->accessibility()->contrastPreference() == Qt::ContrastPreference::HighContrast;
}

QString UiThemeManager::loadStyle(const QString &path) const
{
    QFile file(path);
    return file.open(QIODevice::ReadOnly | QIODevice::Text) ? QString::fromUtf8(file.readAll()) : QString();
}

void UiThemeManager::applyStyleSheet()
{
    if (!m_application)
        return;
    const QString themePath = highContrast()
        ? QStringLiteral(":/styles/high-contrast.qss")
        : QStringLiteral(":/styles/light.qss");
    QPalette palette = m_systemPalette;
    if (!highContrast()) {
        palette.setColor(QPalette::Window, QColor("#f4f5f8"));
        palette.setColor(QPalette::Base, QColor("#ffffff"));
        palette.setColor(QPalette::Text, QColor("#303442"));
        palette.setColor(QPalette::WindowText, QColor("#303442"));
        palette.setColor(QPalette::Highlight, QColor("#5362c9"));
        palette.setColor(QPalette::HighlightedText, Qt::white);
        palette.setColor(QPalette::Link, QColor("#5362c9"));
    }
    m_application->setPalette(palette);
    m_application->setStyleSheet(loadStyle(QStringLiteral(":/styles/base.qss")) + QLatin1Char('\n') + loadStyle(themePath));
    for (QWidget *widget : m_application->topLevelWidgets())
        applyDensityProperty(widget);
    emit themeChanged();
}

void UiThemeManager::applyDensityProperty(QWidget *root) const
{
    if (!root)
        return;
    root->setProperty("density", UiSettings::instance().density() == UiDensity::Compact ? "compact" : "comfortable");
    root->style()->unpolish(root);
    root->style()->polish(root);
    root->update();
}
