#pragma once
#include "canvas.h"
#include <QMainWindow>

class QLabel;
class QSpinBox;
class QDoubleSpinBox;
class QCheckBox;
class QPushButton;
class QDockWidget;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    Canvas *canvas() const { return canvas_; }
    bool openPath(const QString &path);
protected:
    void closeEvent(QCloseEvent *event) override;
private:
    Canvas *canvas_;
    QString path_;
    QLabel *toolLabel_, *sizeLabel_, *positionLabel_;
    QDoubleSpinBox *zoom_;
    QSpinBox *strokeWidth_, *rays_;
    QCheckBox *gridVisible_;
    QPushButton *frontButton_, *backButton_, *gridColorButton_;
    QColor front_ = QColor("#2c3441"), back_ = Qt::white;
    QDockWidget *perspectiveDock_;
    QAction *perspectiveAction_;
    void newDocument();
    void openDocument();
    bool saveDocument(bool saveAs = false);
    void exportImage();
    bool confirmDiscard();
    void updateState();
    void updateColors();
    void showError(const QString &error);
};
