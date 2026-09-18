#pragma once

#include <QFrame>

class QString;
class QToolButton;
class QWidget;

class RolloutSection final : public QFrame {
    Q_OBJECT

public:
    explicit RolloutSection(const QString &title, const QString &name, QWidget *parent = nullptr);

    QWidget *contentWidget() const;

private:
    QToolButton *toggle_ = nullptr;
    QWidget *content_ = nullptr;
};
