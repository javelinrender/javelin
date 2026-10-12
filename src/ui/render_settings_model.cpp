// Copyright (c) Maxwell Hunt and Alexander Kaminsky 2026. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license
// information.

#include "render_settings_model.h"

#include <limits>

#include <QUrl>
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
QVariantMap parameterMap(
        const EnumParameter<Enum>& parameter,
        const RenderSettings& settings) {
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
        {"visible", parameter.visible(settings)},
        {"value", selectedKey},
        {"choices", choices},
    };
}

template<typename Integer>
QVariantMap parameterMap(
        const IntegerParameter<Integer>& parameter,
        const RenderSettings& settings) {
    return {
        {"name", toQString(parameter.name)},
        {"label", toQString(parameter.label)},
        {"type", "integer"},
        {"visible", parameter.visible(settings)},
        {"value", parameter.value},
        {"minimum", parameter.min.value_or(std::numeric_limits<Integer>::lowest())},
        {"maximum", parameter.max.value_or(std::numeric_limits<Integer>::max())},
        {"step", parameter.step},
    };
}

QVariantMap parameterMap(
        const BoolParameter& parameter,
        const RenderSettings& settings) {
    return {
        {"name", toQString(parameter.name)},
        {"label", toQString(parameter.label)},
        {"type", "bool"},
        {"visible", parameter.visible(settings)},
        {"value", parameter.value},
    };
}

QVariantMap parameterMap(
        const FilePathParameter& parameter,
        const RenderSettings& settings) {
    QStringList filters;
    for (const std::string_view extension : parameter.extensions)
        filters.append(QStringLiteral("*%1").arg(toQString(extension)));
    const QString nameFilter = filters.isEmpty()
        ? QStringLiteral("All files (*)")
        : QStringLiteral("%1 (%2)").arg(
            toQString(parameter.label),
            filters.join(' '));
    return {
        {"name", toQString(parameter.name)},
        {"label", toQString(parameter.label)},
        {"type", "file_path"},
        {"visible", parameter.visible(settings)},
        {"value", toQString(parameter.value)},
        {"name_filters", QStringList{nameFilter}},
    };
}

template<typename Fields>
QVariantMap parameterMap(
        const ParameterGroup<Fields>& parameter,
        const RenderSettings& settings) {
    QVariantList children;
    std::apply(
        [&](const auto&... child) {
            (children.append(parameterMap(child, settings)), ...);
        },
        parameter.parameters());
    return {
        {"name", toQString(parameter.name)},
        {"label", toQString(parameter.label)},
        {"type", "group"},
        {"visible", parameter.visible(settings)},
        {"children", children},
    };
}

QList<QVariantMap> parameterRows(const RenderSettings& settings) {
    QList<QVariantMap> result;
    settings.forEachParameter([&](const auto& parameter) {
        result.append(parameterMap(parameter, settings));
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
        (value.metaType().id() == QMetaType::QUrl
            ? value.toUrl().toLocalFile()
            : value.toString()).toStdString());

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
        