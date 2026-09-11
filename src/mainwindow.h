#pragma once
#include "canvas.h"
#include <QMainWindow>

class QLabel;
class QSpinBox;
class QDoubleSpinBox;
class QCheckBox;
class QPushButton;
class QDockWidget;
class QMenu;
class QListWidget;
class QComboBox;

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
    QDoubleSpinBox *zoom_, *rayStep_, *rayAngleOffset_, *rayWidth_, *horizonWidth_, *horizonPosition_, *pointX_, *pointY_;
    QSpinBox *strokeWidth_, *rayGap_, *rayStartOpacity_, *rayEndOpacity_, *rayFadeLength_, *horizonOpacity_;
    QCheckBox *gridVisible_, *selectedPointVisible_, *horizonVisible_, *axesVisible_, *markersVisible_, *symmetricPoints_;
    QPushButton *frontButton_, *backButton_, *gridColorButton_, *horizonColorButton_, *savePerspectiveDefaultsButton_, *removePointButton_;
    QListWidget *vanishingPointsList_;
    QComboBox *horizonUnits_, *pointUnits_, *pointAttachment_, *rayPattern_;
    QColor front_ = QColor("#2c3441"), back_ = Qt::white;
    QDockWidget *perspectiveDock_;
    QAction *perspectiveAction_;
    QMenu *recentFilesMenu_;
    QStringList recentFiles_;
    QVector<int> toolWidths_{3,3,3};
    bool coordinatePercent_ = true, rulerPercent_ = false;
    void newDocument();
    void openDocument();
    bool saveDocument(bool saveAs = false);
    void exportImage();
    bool confirmDiscard();
    void updateState();
    void updateColors();
    void activateTool(Canvas::Tool tool,const QString &name);
    void addRecentFile(const QString &path);
    void updateRecentFilesMenu();
    void showError(const QString &error);
    void showSettings();
    double displayedX(double imageX) const;
    double displayedY(double imageY) const;
    double imageX(double displayed) const;
    double imageY(double displayed) const;
    void setCoordinateUnits(bool percent);
};
