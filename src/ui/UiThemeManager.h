#ifndef UITHEMEMANAGER_H
#define UITHEMEMANAGER_H

#include <QObject>

class QApplication;
class QWidget;

class UiThemeManager : public QObject
{
    Q_OBJECT

public:
    static UiThemeManager &instance();
    void initialize(QApplication *application);
    void applyDensityProperty(QWidget *root) const;
    bool highContrast() const;

signals:
    void themeChanged();

private:
    explicit UiThemeManager(QObject *parent = nullptr);
    void applyStyleSheet();
    QString loadStyle(const QString &path) const;

    QApplication *m_application = nullptr;
};

#endif
