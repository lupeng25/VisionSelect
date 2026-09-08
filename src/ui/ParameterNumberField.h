#ifndef PARAMETERNUMBERFIELD_H
#define PARAMETERNUMBERFIELD_H

#include <QJsonObject>
#include <QWidget>
#include <optional>

class QLabel;
class QLineEdit;
class QComboBox;

// 允许空值的数值字段；单位变更转换显示值，内部值始终采用基准单位。
class ParameterNumberField : public QWidget
{
    Q_OBJECT
public:
    ParameterNumberField(const QString &key, const QString &title, const QString &unit,
                         bool integer = false, QWidget *parent = nullptr);
    std::optional<double> value() const;
    void setValue(std::optional<double> value);
    void setReadOnly(bool readOnly);
    void setTitle(const QString &title);
    QString title() const;
    QString displayText() const;
    QJsonObject state() const;
    void restoreState(const QJsonObject &state);

signals:
    void changed();

private:
    QLabel *m_label;
    QLineEdit *m_edit;
    QComboBox *m_units = nullptr;
    QString m_unit;
    double m_scale = 1.0;
    bool m_restoring = false;
};

#endif
