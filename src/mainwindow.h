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
class DrawingToolSettingsModel;
class ToolPropertiesPanel;

/// Главное окно связывает пользовательские команды, Canvas, файлы проекта и постоянные настройки.
class MainWindow : public QMainWindow {
    Q_OBJECT
public:
    /// Создаёт и синхронизирует все панели однодокументного редактора.
    explicit MainWindow(QWidget *parent = nullptr);
    /// Возвращает холст для интеграционных проверок и управляющего кода окна.
    Canvas *canvas() const {
        return canvas_;
    }
    /// Открывает `.drw` либо импортирует PNG по известному пути без файлового диалога.
    bool openPath(const QString &path);

protected:
    /// Перед закрытием окна предлагает сохранить изменённый документ.
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
    DrawingToolSettingsModel *toolSettings_ = nullptr;
    ToolPropertiesPanel *toolProperties_ = nullptr;
    QMenu *recentFilesMenu_ = nullptr;
    QStringList recentFiles_;
    bool coordinatePercent_ = true;
    bool rulerPercent_ = false;
    /// Задаёт свойства окна и последовательно создаёт основные части интерфейса.
    void initializeWindow();
    /// Создаёт меню, команды и две панели инструментов с общими действиями.
    void setupMenusAndToolbars();
    /// Создаёт dock-панель перспективы и её сворачиваемые секции.
    void setupPerspectivePanel();
    /// Связывает элементы панели перспективы с моделью Canvas и настройками.
    void connectPerspectiveControls();
    /// Создаёт команды масштаба, строку состояния и отображение координат курсора.
    void setupViewAndStatusBar();
    /// Запрашивает размер и создаёт новый документ после проверки несохранённых изменений.
    void newDocument();
    /// Показывает диалог выбора проекта или PNG и передаёт путь в `openPath()`.
    void openDocument();
    /// Сохраняет текущую историю в `.drw`, при необходимости запросив новый путь.
    bool saveDocument(bool saveAs = false);
    /// Экспортирует только растровое изображение документа в PNG.
    void exportImage();
    /// Возвращает разрешение продолжить после обработки несохранённых изменений.
    bool confirmDiscard();
    /// Обновляет заголовок и элементы управления из текущего состояния Canvas.
    void updateState();
    /// Обновляет образцы цветов Front и Back на панели команд.
    void updateColors();
    /// Переключает инструмент, его сохранённую ширину и видимое название.
    void activateTool(Canvas::Tool tool, const QString &name);
    /// Добавляет канонический путь в начало ограниченного списка недавних проектов.
    void addRecentFile(const QString &path);
    /// Перестраивает меню недавних файлов из постоянного списка.
    void updateRecentFilesMenu();
    /// Находит текущий индекс точки по устойчивому идентификатору после перестроения списка.
    int vanishingPointIndex(const QString &id) const;
    /// Показывает критическое сообщение с пользовательским названием приложения.
    void showError(const QString &error);
    /// Показывает вкладки настроек приложения и применяет изменённые параметры вида.
    void showSettings();
    /// Показывает значок и полное пользовательское название приложения.
    void showAbout();
    /// Переводит X изображения в выбранные пользователем единицы относительно центра.
    double displayedX(double imageX) const;
    /// Переводит направленную вверх Y-координату изображения в выбранные единицы.
    double displayedY(double imageY) const;
    /// Переводит введённую X-координату обратно в пиксели изображения.
    double imageX(double displayed) const;
    /// Переводит введённую Y-координату обратно в пиксели изображения.
    double imageY(double displayed) const;
    /// Настраивает диапазон, шаг и суффикс полей координат без перемещения объектов.
    void setCoordinateUnits(bool percent);
};
