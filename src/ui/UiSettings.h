#ifndef UISETTINGS_H
#define UISETTINGS_H

#include <QObject>
#include <QString>

class QHeaderView;
class QMainWindow;
class QSplitter;
class QVariant;

enum class UiDensity {
    Comfortable,
    Compact
};

class UiSettings : public QObject
{
    Q_OBJECT

public:
    static UiSettings &instance();

    void initialize();

    UiDensity density() const;
    void setDensity(UiDensity density);
    bool preferredSidebarExpanded() const;
    void setPreferredSidebarExpanded(bool expanded);

    void restoreWindow(QMainWindow *window) const;
    void saveWindow(const QMainWindow *window) const;
    void restoreSplitter(const QString &id, QSplitter *splitter) const;
    void saveSplitter(const QString &id, const QSplitter *splitter) const;
    void restoreHeader(const QString &id, QHeaderView *header) const;
    void saveHeader(const QString &id, const QHeaderView *header) const;

    bool boolValue(const QString &key, bool fallback) const;
    int intValue(const QString &key, int fallback) const;
    void setValue(const QString &key, const QVariant &value) const;

    static int controlHeight();
    static int tableRowHeight();
    static int spacing();

signals:
    void densityChanged(UiDensity density);
    void preferredSidebarExpandedChanged(bool expanded);

private:
    explicit UiSettings(QObject *parent = nullptr);
};

#endif
