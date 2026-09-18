#pragma once

#include <QFrame>

class QString;
class QToolButton;
class QWidget;

/// Сворачиваемая секция панели, сохраняющая своё состояние между запусками.
class RolloutSection final : public QFrame {
    Q_OBJECT

public:
    /// Создаёт секцию с пользовательским заголовком и устойчивым ключом настроек.
    explicit RolloutSection(const QString &title, const QString &name, QWidget *parent = nullptr);

    /// Возвращает контейнер, в который владелец добавляет элементы содержимого секции.
    QWidget *contentWidget() const;

private:
    QToolButton *toggle_ = nullptr;
    QWidget *content_ = nullptr;
};
