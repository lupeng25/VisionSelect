#ifndef FOVDIAGRAM_H
#define FOVDIAGRAM_H

#include <QSizeF>
#include <QWidget>

class FovDiagram : public QWidget
{
public:
    explicit FovDiagram(QWidget *parent = nullptr);
    void setFields(QSizeF target, QSizeF actual, bool circular = false);
protected:
    void paintEvent(QPaintEvent *event) override;
private:
    QSizeF m_target;
    QSizeF m_actual;
    bool m_circular = false;
};

#endif
