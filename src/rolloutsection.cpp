#include "rolloutsection.h"

#include <QHBoxLayout>
#include <QLabel>
#include <QString>
#include <QToolButton>
#include <QVariant>
#include <QVBoxLayout>

RolloutSection::RolloutSection(const QString &title, const QString &name, QWidget *parent) : QFrame(parent) {
    setObjectName(name);
    setProperty("title", title);
    setProperty("expanded", true);
    setFrameShape(QFrame::StyledPanel);
    setFrameShadow(QFrame::Plain);
    setLineWidth(1);

    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(0);

    auto *header = new QFrame(this);
    header->setObjectName("rolloutHeader");

    auto *headerLayout = new QHBoxLayout(header);
    headerLayout->setContentsMargins(4, 2, 7, 2);
    headerLayout->setSpacing(3);

    toggle_ = new QToolButton(header);
    toggle_->setObjectName(name + "Toggle");
    toggle_->setCheckable(true);
    toggle_->setChecked(true);
    toggle_->setArrowType(Qt::DownArrow);
    toggle_->setAutoRaise(true);
    toggle_->setFixedSize(22, 22);
    toggle_->setToolTip(tr("Свернуть раздел"));

    auto *titleLabel = new QLabel(title, header);
    titleLabel->setObjectName(name + "Title");
    titleLabel->setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
    QFont titleFont = titleLabel->font();
    titleFont.setBold(true);
    titleLabel->setFont(titleFont);

    headerLayout->addWidget(toggle_);
    headerLayout->addWidget(titleLabel);

    content_ = new QWidget(this);
    content_->setObjectName(name + "Content");

    outer->addWidget(header);
    outer->addWidget(content_);

    connect(toggle_, &QToolButton::toggled, this, [this](bool expanded) {
        content_->setVisible(expanded);
        toggle_->setArrowType(expanded ? Qt::DownArrow : Qt::RightArrow);
        toggle_->setToolTip(expanded ? tr("Свернуть раздел") : tr("Развернуть раздел"));
        setProperty("expanded", expanded);
        updateGeometry();
    });
}

QWidget *RolloutSection::contentWidget() const {
    return content_;
}
