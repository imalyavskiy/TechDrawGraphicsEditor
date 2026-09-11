#pragma once
#include <QImage>
#include <QPointF>
#include <QColor>
#include <QStringList>
#include <QVector>

struct DrawingState {
    QImage image;
    QPointF vanishing;
    bool gridVisible = false;
    int rays = 16;
    QColor gridColor = QColor("#628ed1");
};

struct DrawingHistory {
    QVector<DrawingState> states;
    QStringList labels;
    int index = 0;
};

namespace Project {
bool validSize(QSize size);
bool save(const QString &path, const DrawingState &state, QString *error);
bool save(const QString &path, const DrawingHistory &history, QString *error);
bool load(const QString &path, DrawingState *state, QString *error);
bool load(const QString &path, DrawingHistory *history, QString *error);
bool loadPng(const QString &path, QImage *image, QString *error);
bool exportPng(const QString &path, const QImage &image, QString *error);
}
