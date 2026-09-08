#ifndef PARAMETERWORKSPACESTATE_H
#define PARAMETERWORKSPACESTATE_H

#include <QJsonArray>
#include <QJsonObject>
#include <optional>

// 字段名和选项标识为稳定键；不依赖控件创建顺序或界面语言。
struct ParameterWorkspaceState {
    QString task = QStringLiteral("optics");
    QJsonObject fields;
    QJsonObject choices;
    QJsonObject flags;
    QJsonObject texts;
    QJsonObject provenance;
    QJsonArray snapshots;

    QJsonObject toJson() const {
        return {{QStringLiteral("version"), 1}, {QStringLiteral("task"), task},
                {QStringLiteral("fields"), fields}, {QStringLiteral("choices"), choices},
                {QStringLiteral("flags"), flags}, {QStringLiteral("texts"), texts},
                {QStringLiteral("provenance"), provenance}, {QStringLiteral("snapshots"), snapshots}};
    }
    static std::optional<ParameterWorkspaceState> fromJson(const QJsonObject &json) {
        if (json.value(QStringLiteral("version")).toInt() != 1) return {};
        ParameterWorkspaceState state;
        state.task = json.value(QStringLiteral("task")).toString(QStringLiteral("optics"));
        state.fields = json.value(QStringLiteral("fields")).toObject();
        state.choices = json.value(QStringLiteral("choices")).toObject();
        state.flags = json.value(QStringLiteral("flags")).toObject();
        state.texts = json.value(QStringLiteral("texts")).toObject();
        state.provenance = json.value(QStringLiteral("provenance")).toObject();
        const auto snapshots = json.value(QStringLiteral("snapshots")).toArray();
        for (int i = 0; i < snapshots.size() && i < 2; ++i) state.snapshots.append(snapshots.at(i));
        return state;
    }
};

#endif
