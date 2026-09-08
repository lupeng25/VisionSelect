#include "ui/UiSettings.h"

#include <QGuiApplication>
#include <QHeaderView>
#include <QMainWindow>
#include <QScreen>
#include <QSettings>
#include <QSplitter>
#include <QVariant>

namespace {
constexpr int kLayoutVersion = 3;
const char *kLayoutVersionKey = "ui/layoutVersion";
const char *kDensityKey = "ui/density";
const char *kSidebarExpandedKey = "ui/sidebar/preferredExpanded";

QString splitterKey(const QString &id)
{
    return QStringLiteral("ui/splitters/") + id;
}

QString headerKey(const QString &id)
{
    return QStringLiteral("ui/headers/") + id;
}

bool isGeometryVisible(const QRect &geometry)
{
    if (!geometry.isValid() || geometry.width() <= 0 || geometry.height() <= 0)
        return false;

    qint64 visibleArea = 0;
    for (QScreen *screen : QGuiApplication::screens()) {
        const QRect intersection = geometry.intersected(screen->availableGeometry());
        visibleArea += static_cast<qint64>(intersection.width()) * intersection.height();
    }
    const qint64 totalArea = static_cast<qint64>(geometry.width()) * geometry.height();
    return totalArea > 0 && visibleArea * 10 >= totalArea * 3;
}
}

UiSettings &UiSettings::instance()
{
    static UiSettings settings;
    return settings;
}

UiSettings::UiSettings(QObject *parent)
    : QObject(parent)
{
}

void UiSettings::initialize()
{
    QSettings settings;
    if (settings.value(QString::fromLatin1(kLayoutVersionKey), 0).toInt() == kLayoutVersion)
        return;

    settings.remove(QStringLiteral("ui/window"));
    settings.remove(QStringLiteral("ui/sidebar"));
    settings.remove(QStringLiteral("ui/density"));
    settings.remove(QStringLiteral("ui/splitters"));
    settings.remove(QStringLiteral("ui/headers"));
    settings.remove(QStringLiteral("ui/threeD"));
    settings.setValue(QString::fromLatin1(kLayoutVersionKey), kLayoutVersion);
}

UiDensity UiSettings::density() const
{
    QSettings settings;
    return settings.value(QString::fromLatin1(kDensityKey), QStringLiteral("comfortable")).toString()
                   == QLatin1String("compact")
        ? UiDensity::Compact
        : UiDensity::Comfortable;
}

void UiSettings::setDensity(UiDensity density)
{
    if (density == this->density())
        return;
    QSettings settings;
    settings.setValue(QString::fromLatin1(kDensityKey),
                      density == UiDensity::Compact ? QStringLiteral("compact") : QStringLiteral("comfortable"));
    emit densityChanged(density);
}

bool UiSettings::preferredSidebarExpanded() const
{
    QSettings settings;
    return settings.value(QString::fromLatin1(kSidebarExpandedKey), false).toBool();
}

void UiSettings::setPreferredSidebarExpanded(bool expanded)
{
    if (expanded == preferredSidebarExpanded())
        return;
    QSettings settings;
    settings.setValue(QString::fromLatin1(kSidebarExpandedKey), expanded);
    emit preferredSidebarExpandedChanged(expanded);
}

void UiSettings::restoreWindow(QMainWindow *window) const
{
    if (!window)
        return;
    QSettings settings;
    const QByteArray geometry = settings.value(QStringLiteral("ui/window/geometry")).toByteArray();
    const QByteArray state = settings.value(QStringLiteral("ui/window/state")).toByteArray();
    if (!geometry.isEmpty() && !window->restoreGeometry(geometry))
        settings.remove(QStringLiteral("ui/window/geometry"));
    if (!state.isEmpty() && !window->restoreState(state))
        settings.remove(QStringLiteral("ui/window/state"));

    if (isGeometryVisible(window->frameGeometry()))
        return;
    settings.remove(QStringLiteral("ui/window/geometry"));
    QScreen *screen = QGuiApplication::primaryScreen();
    if (!screen)
        return;
    const QRect available = screen->availableGeometry();
    const QSize size(qMin(window->width(), available.width()), qMin(window->height(), available.height()));
    window->resize(size);
    window->move(available.center() - QPoint(size.width() / 2, size.height() / 2));
}

void UiSettings::saveWindow(const QMainWindow *window) const
{
    if (!window)
        return;
    QSettings settings;
    settings.setValue(QStringLiteral("ui/window/geometry"), window->saveGeometry());
    settings.setValue(QStringLiteral("ui/window/state"), window->saveState());
}

void UiSettings::restoreSplitter(const QString &id, QSplitter *splitter) const
{
    if (!splitter)
        return;
    QSettings settings;
    const QByteArray state = settings.value(splitterKey(id)).toByteArray();
    if (!state.isEmpty() && !splitter->restoreState(state))
        settings.remove(splitterKey(id));
}

void UiSettings::saveSplitter(const QString &id, const QSplitter *splitter) const
{
    if (!splitter)
        return;
    QSettings settings;
    settings.setValue(splitterKey(id), splitter->saveState());
}

void UiSettings::restoreHeader(const QString &id, QHeaderView *header) const
{
    if (!header)
        return;
    QSettings settings;
    const QByteArray state = settings.value(headerKey(id)).toByteArray();
    if (!state.isEmpty() && !header->restoreState(state))
        settings.remove(headerKey(id));
}

void UiSettings::saveHeader(const QString &id, const QHeaderView *header) const
{
    if (!header)
        return;
    QSettings settings;
    settings.setValue(headerKey(id), header->saveState());
}

bool UiSettings::boolValue(const QString &key, bool fallback) const
{
    return QSettings().value(key, fallback).toBool();
}

int UiSettings::intValue(const QString &key, int fallback) const
{
    return QSettings().value(key, fallback).toInt();
}

void UiSettings::setValue(const QString &key, const QVariant &value) const
{
    QSettings().setValue(key, value);
}

int UiSettings::controlHeight()
{
    return instance().density() == UiDensity::Compact ? 30 : 40;
}

int UiSettings::tableRowHeight()
{
    return instance().density() == UiDensity::Compact ? 32 : 42;
}

int UiSettings::spacing()
{
    return instance().density() == UiDensity::Compact ? 8 : 12;
}
