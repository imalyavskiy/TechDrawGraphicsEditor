#pragma once
#include <QImage>
#include <QPointF>
#include <QColor>

struct DrawingState {
    QImage image;
    QPointF vanishing;
    bool gridVisible = false;
    int rays = 16;
    QColor gridColor = QColor("#628ed1");
};

namespace Project {
bool validSize(QSize size);
bool save(const QString &path, const DrawingState &state, QString *error);
bool load(const QString &path, DrawingState *state, QString *error);
bool loadPng(const QString &path, QImage *image, QString *error);
bool exportPng(const QString &path, const QImage &image, QString *error);
}
