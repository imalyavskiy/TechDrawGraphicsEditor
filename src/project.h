#pragma once
#include <QImage>
#include <QPointF>
#include <QColor>
#include <QStringList>
#include <QVector>

struct DrawingState {
    QImage image;
    QPointF vanishing;
    double horizonY = 0;
    bool gridVisible = false;
    double rayStepDegrees = 10.0;
    QColor gridColor = QColor("#628ed1");
    int rayGap = 12;
    int rayStartOpacity = 10;
    int rayEndOpacity = 70;
    int rayFadeLength = 50;
    QColor horizonColor = QColor("#628ed1");
    int horizonOpacity = 70;
    double horizonWidth = 1.0;
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
