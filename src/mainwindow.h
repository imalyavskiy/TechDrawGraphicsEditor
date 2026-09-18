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
class QToolBar;

class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    Canvas *canvas() const {
        return canvas_;
    }
    bool openPath(const QString &path);

protected:
    void closeEvent(QCloseEvent *event) override;

private:
    Canvas *canvas_ = nullptr;
    QString path_;
    QLabel *toolLabel_ = nullptr;
    QLabel *sizeLabel_ = nullptr;
    QLabel *positionLabel_ = nullptr;
    QDoubleSpinBox *zoom_ = nullptr;
    QDoubleSpinBox *rayStep_ = nullptr;
    QDoubleSpinBox *rayAngleOffset_ = nullptr;
    QDoubleSpinBox *rayWidth_ = nullptr;
    QDoubleSpinBox *horizonWidth_ = nullptr;
    QDoubleSpinBox *horizonPosition_ = nullptr;
    QDoubleSpinBox *verticalWidth_ = nullptr;
    QDoubleSpinBox *verticalPosition_ = nullptr;
    QDoubleSpinBox *pointX_ = nullptr;
    QDoubleSpinBox *pointY_ = nullptr;
    QSpinBox *strokeWidth_ = nullptr;
    QSpinBox *rayGap_ = nullptr;
    QSpinBox *rayStartOpacity_ = nullptr;
    QSpinBox *rayEndOpacity_ = nullptr;
    QSpinBox *rayFadeLength_ = nullptr;
    QSpinBox *horizonOpacity_ = nullptr;
    QSpinBox *verticalOpacity_ = nullptr;
    QCheckBox *gridVisible_ = nullptr;
    QCheckBox *selectedPointVisible_ = nullptr;
    QCheckBox *selectedPointLocked_ = nullptr;
    QCheckBox *horizonVisible_ = nullptr;
    QCheckBox *horizonLocked_ = nullptr;
    QCheckBox *horizonSymmetry_ = nullptr;
    QCheckBox *verticalVisible_ = nullptr;
    QCheckBox *verticalLocked_ = nullptr;
    QCheckBox *verticalSymmetry_ = nullptr;
    QCheckBox *axesVisible_ = nullptr;
    QCheckBox *markersVisible_ = nullptr;
    QPushButton *frontButton_ = nullptr;
    QPushButton *backButton_ = nullptr;
    QPushButton *gridColorButton_ = nullptr;
    QPushButton *horizonColorButton_ = nullptr;
    QPushButton *verticalColorButton_ = nullptr;
    QPushButton *savePerspectiveDefaultsButton_ = nullptr;
    QPushButton *resetPerspectiveDefaultsButton_ = nullptr;
    QPushButton *addPointButton_ = nullptr;
    QPushButton *removePointButton_ = nullptr;
    QListWidget *vanishingPointsList_ = nullptr;
    QComboBox *horizonUnits_ = nullptr;
    QComboBox *verticalUnits_ = nullptr;
    QComboBox *pointUnits_ = nullptr;
    QComboBox *pointAttachment_ = nullptr;
    QComboBox *rayPattern_ = nullptr;
    QColor front_ = QColor("#2c3441");
    QColor back_ = Qt::white;
    QDockWidget *perspectiveDock_ = nullptr;
    QAction *perspectiveAction_ = nullptr;
    QMenu *viewMenu_ = nullptr;
    QMenu *toolsMenu_ = nullptr;
    QMenu *helpMenu_ = nullptr;
    QToolBar *mainToolbar_ = nullptr;
    QToolBar *toolsToolbar_ = nullptr;
    QMenu *recentFilesMenu_ = nullptr;
    QStringList recentFiles_;
    QVector<int> toolWidths_{3, 3, 3};
    bool coordinatePercent_ = true;
    bool rulerPercent_ = false;
    void initializeWindow();
    void setupMenusAndToolbars();
    void setupPerspectivePanel();
    void connectPerspectiveControls();
    void setupViewAndStatusBar();
    void newDocument();
    void openDocument();
    bool saveDocument(bool saveAs = false);
    void exportImage();
    bool confirmDiscard();
    void updateState();
    void updateColors();
    void activateTool(Canvas::Tool tool, const QString &name);
    void addRecentFile(const QString &path);
    void updateRecentFilesMenu();
    int vanishingPointIndex(const QString &id) const;
    void showError(const QString &error);
    void showSettings();
    void showAbout();
    double displayedX(double imageX) const;
    double displayedY(double imageY) const;
    double imageX(double displayed) const;
    double imageY(double displayed) const;
    void setCoordinateUnits(bool percent);
};
