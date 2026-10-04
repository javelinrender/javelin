// Copyright (c) Maxwell Hunt and Alexander Kaminsky 2026. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license
// information.

#include "render_settings_model.h"

#include <limits>

#include <QVariantList>
#include <QVariantMap>
#include <string_view>
#include <utility>

namespace javelin::ui {

namespace {

QString toQString(const std::string_view value) {
    return QString::fromUtf8(value.data(), static_cast<qsizetype>(value.size()));
}

template<typename Enum>
QVariantMap parameterMap(const EnumParameter<Enum>& parameter) {
    QVariantList choices;
    QString selectedKey;

    for (const typename EnumParameter<Enum>::Choice& choice : parameter.choices) {
        QVariantMap choiceMap;
        choiceMap.insert("key", toQString(choice.key));
        choiceMap.insert("label", toQString(choice.label));
        choices.append(choiceMap);
        if (choice.value == parameter.value)
            selectedKey = toQString(choice.key);
    }

    return {
        {"name", toQString(parameter.name)},
        {"label", toQString(parameter.label)},
        {"type", "enum"},
        {"value", selectedKey},
        {"choices", choices},
    };
}

template<typename Integer>
QVariantMap parameterMap(const IntegerParameter<Integer>& parameter) {
    return {
        {"name", toQString(parameter.name)},
        {"label", toQString(parameter.label)},
        {"type", "integer"},
        {"value", parameter.value},
        {"minimum", parameter.min.value_or(std::numeric_limits<Integer>::lowest())},
        {"maximum", parameter.max.value_or(std::numeric_limits<Integer>::max())},
        {"step", parameter.step},
    };
}

QVariantMap parameterMap(const BoolParameter& parameter) {
    return {
        {"name", toQString(parameter.name)},
        {"label", toQString(parameter.label)},
        {"type", "bool"},
        {"value", parameter.value},
    };
}

QList<QVariantMap> parameterRows(const RenderSettings& settings) {
    QList<QVariantMap> result;
    settings.forEachParameter([&](const auto& parameter) {
        QVariantMap row = parameterMap(parameter);
        row.insert("visible", parameter.visible(settings));
        result.append(std::move(row));
    });
    return result;
}

} // namespace

RenderSettingsModel::RenderSettingsModel(RenderSettings& options, QObject* parent)
    : QAbstractListModel(parent), _options(options), _parameters(parameterRows(options)) {}

int RenderSettingsModel::rowCount(const QModelIndex& parent) const {
    return parent.isValid() ? 0 : static_cast<int>(_parameters.size());
}

QVariant RenderSettingsModel::data(const QModelIndex& index, int role) const {
    if (!index.isValid() || index.row() < 0 || index.row() >= _parameters.size()
        || role != ParameterRole)
        return {};
    return _parameters[index.row()];
}

QHash<int, QByteArray> RenderSettingsModel::roleNames() const {
    return {{ParameterRole, "parameter"}};
}

bool RenderSettingsModel::setValue(const QString& name, const QVariant& value) {
    const std::optional<std::string> error = setParameterValue(
        _options,
        name.toStdString(),
        value.toString().toStdString());

    if (error)
        return false;

    QList<QVariantMap> updated = parameterRows(_options);
    bool changed = false;
    for (int row = 0; row < rowCount(); ++row) {
        if (_parameters[row] != updated[row]) {
            changed = true;
            _parameters[row] = std::move(updated[row]);
            emit dataChanged(index(row, 0), index(row, 0), {ParameterRole});
        }
    }
    if (changed)
        emit settingsChanged();
    return true;
}

} // namespace javelin::ui
        