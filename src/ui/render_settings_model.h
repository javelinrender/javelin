// Copyright (c) Maxwell Hunt and Alexander Kaminsky 2026. All rights reserved.
// Licensed under the MIT License. See LICENSE in the project root for license
// information.

#pragma once

#include <QAbstractListModel>
#include <QList>
#include <QString>
#include <QVariant>
#include <QVariantMap>

#include "render_settings.h"

namespace javelin::ui {

class RenderSettingsModel : public QAbstractListModel {
    Q_OBJECT

public:
    enum Role {
        ParameterRole = Qt::UserRole + 1,
    };

    explicit RenderSettingsModel(RenderSettings& options, QObject* parent = nullptr);

    int rowCount(const QModelIndex& parent = {}) const override;
    QVariant data(const QModelIndex& index, int role = Qt::DisplayRole) const override;
    QHash<int, QByteArray> roleNames() const override;

    Q_INVOKABLE bool setValue(const QString& name, const QVariant& value);

signals:
    void settingsChanged();

private:
    RenderSettings& _options;
    QList<QVariantMap> _parameters;
};

} // namespace javelin::ui
