#include "mainmenu.h"
#include <QPushButton>
#include <QIcon>
#include <QPropertyAnimation>
#include <QStyle>

MainMenu::MainMenu(QWidget* parent)
    : QFrame(parent)
{
    setCursor(Qt::ArrowCursor);

    const int HEIGHT = 30;
    setFixedHeight(HEIGHT);

    layout_ = new QHBoxLayout();
    layout_->setSpacing(0);
    layout_->setMargin(0);
    this->setLayout(layout_);

    ///
    QPushButton * rectangle_btn = new QPushButton();
    rectangle_btn->setObjectName("rectangle_btn");
    rectangle_btn->setIcon(QIcon(":/icon/res/box.png"));
    rectangle_btn->setIconSize(QSize(22, 22));
    rectangle_btn->setFixedSize(HEIGHT, HEIGHT);
    connect(rectangle_btn, &QPushButton::clicked, [=]() {
        rectangle_btn == selected_btn_
                ? emit END_PAINT_RECTANGLE()
                : emit START_PAINT_RECTANGLE();

        this->click(rectangle_btn);
    });
    layout_->addWidget(rectangle_btn);

    QPushButton * circle_btn = new QPushButton();
    circle_btn->setObjectName("circle_btn");
    circle_btn->setIcon(QIcon(":/icon/res/circle.png"));
    circle_btn->setIconSize(QSize(20, 20));
    circle_btn->setFixedSize(HEIGHT, HEIGHT);
    connect(circle_btn, &QPushButton::clicked, [=]() {
        circle_btn == selected_btn_
                ? emit END_PAINT_CIRCLE()
                : emit START_PAINT_CIRCLE();

        this->click(circle_btn);
    });
    layout_->addWidget(circle_btn);

    QPushButton * arrow_btn = new QPushButton();
    arrow_btn->setObjectName("arrow_btn");
    arrow_btn->setIcon(QIcon(":/icon/res/arrow-up-right2.png"));
    arrow_btn->setIconSize(QSize(22, 22));
    arrow_btn->setFixedSize(HEIGHT, HEIGHT);
    connect(arrow_btn, &QPushButton::clicked, [=]() {
        arrow_btn == selected_btn_
                ? emit END_PAINT_ARROW()
                : emit START_PAINT_ARROW();
        this->click(arrow_btn);
    });
    layout_->addWidget(arrow_btn);

    QPushButton * line_btn = new QPushButton();
    line_btn->setObjectName("pen_btn");
    line_btn->setIcon(QIcon(":/icon/res/line.png"));
    line_btn->setIconSize(QSize(20, 20));
    line_btn->setFixedSize(HEIGHT, HEIGHT);
    connect(line_btn, &QPushButton::clicked, [=]() {
        line_btn == selected_btn_
                ? emit END_PAINT_LINE()
                : emit START_PAINT_LINE();

        this->click(line_btn);
    });
    layout_->addWidget(line_btn);

    QPushButton * pen_btn = new QPushButton();
    pen_btn->setObjectName("pen_btn");
    pen_btn->setIcon(QIcon(":/icon/res/feather.png"));
    pen_btn->setIconSize(QSize(22, 22));
    pen_btn->setFixedSize(HEIGHT, HEIGHT);
    connect(pen_btn, &QPushButton::clicked, [=]() {
        pen_btn == selected_btn_ ? emit END_PAINT_CURVES() : emit START_PAINT_CURVES();

        this->click(pen_btn);
    });
    layout_->addWidget(pen_btn);

    QPushButton * text_btn = new QPushButton();
    text_btn->setObjectName("text_btn");
    text_btn->setIcon(QIcon(":/icon/res/text.png"));
    text_btn->setIconSize(QSize(22, 22));
    text_btn->setFixedSize(HEIGHT, HEIGHT);
    connect(text_btn, &QPushButton::clicked, [=]() {
        text_btn == selected_btn_ ? emit END_PAINT_TEXT() : emit START_PAINT_TEXT();

        this->click(text_btn);
    });
    layout_->addWidget(text_btn);

    QFrame * frame_01 = new QFrame();
    frame_01->setMaximumHeight(20);
    frame_01->setFrameStyle(QFrame::Raised);
    frame_01->setFrameShape(QFrame::VLine);
    layout_->addWidget(frame_01);

    ///
    QPushButton * undo_btn = new QPushButton();
    undo_btn->setObjectName("undo_btn");
    undo_btn->setIcon(QIcon(":/icon/res/undo.png"));
    undo_btn->setIconSize(QSize(22, 22));
    undo_btn->setFixedSize(HEIGHT, HEIGHT);
    connect(undo_btn, &QPushButton::clicked, this, &MainMenu::UNDO);
    connect(undo_btn, &QPushButton::clicked, [=]() { this->unselectAll(); });
    layout_->addWidget(undo_btn);

    QPushButton * redo_btn = new QPushButton();
    redo_btn->setObjectName("redo_btn");
    redo_btn->setIcon(QIcon(":/icon/res/redo.png"));
    redo_btn->setIconSize(QSize(22, 22));
    redo_btn->setFixedSize(HEIGHT, HEIGHT);
    connect(redo_btn, &QPushButton::clicked, this, &MainMenu::REDO);
    connect(redo_btn, &QPushButton::clicked, [=]() { this->unselectAll(); });
    layout_->addWidget(redo_btn);

    QFrame * frame = new QFrame();
    frame->setMaximumHeight(20);
    frame->setFrameStyle(QFrame::Raised);
    frame->setFrameShape(QFrame::VLine);
    layout_->addWidget(frame);

    ///
    QPushButton * close_btn = new QPushButton();
    close_btn->setObjectName("close_btn");
    close_btn->setIcon(QIcon(":/icon/res/close.png"));
    close_btn->setIconSize(QSize(22, 22));
    close_btn->setFixedSize(HEIGHT, HEIGHT);
    connect(close_btn, &QPushButton::clicked, this, &MainMenu::EXIT_CAPTURE);
    connect(close_btn, &QPushButton::clicked, [=]() { this->unselectAll(); });
    layout_->addWidget(close_btn);

    QPushButton * fix_btn = new QPushButton();
    fix_btn->setObjectName("fixed_btn");
    fix_btn->setIcon(QIcon(":/icon/res/fixed.png"));
    fix_btn->setIconSize(QSize(20, 20));
    fix_btn->setFixedSize(HEIGHT, HEIGHT);
    connect(fix_btn, &QPushButton::clicked, this, &MainMenu::FIX_IMAGE);
    connect(fix_btn, &QPushButton::clicked, [=]() { this->unselectAll(); });
    layout_->addWidget(fix_btn);

    QPushButton * save_btn = new QPushButton();
    save_btn->setIcon(QIcon(":/icon/res/save.png"));
    save_btn->setObjectName("");
    save_btn->setIconSize(QSize(22, 22));
    save_btn->setFixedSize(HEIGHT, HEIGHT);
    connect(save_btn, &QPushButton::clicked, this, &MainMenu::SAVE_IMAGE);
    connect(save_btn, &QPushButton::clicked, [=]() { this->unselectAll(); });
    layout_->addWidget(save_btn);

    QPushButton * copy_btn = new QPushButton();
    copy_btn->setIcon(QIcon(":/icon/res/copy"));
    copy_btn->setObjectName("copy_btn");
    copy_btn->setIconSize(QSize(22, 22));
    copy_btn->setFixedSize(HEIGHT, HEIGHT);
    connect(copy_btn, &QPushButton::clicked, this, &MainMenu::COPY_TO_CLIPBOARD);
    connect(copy_btn, &QPushButton::clicked, [=]() { this->unselectAll(); });
    layout_->addWidget(copy_btn);
}


MainMenu::~MainMenu()
{
    delete layout_;
}

void MainMenu::click(QPushButton* btn)
{
    if(selected_btn_ != nullptr) {
        selected_btn_->setProperty("selected", false);
        selected_btn_->style()->unpolish(selected_btn_);
        selected_btn_->style()->polish(selected_btn_);
        selected_btn_->update();
    }

    if(btn != selected_btn_) {
        btn->setProperty("selected", true);
        btn->style()->unpolish(btn);
        btn->style()->polish(btn);
        btn->update();
        selected_btn_ = btn;
    }
    else {
        selected_btn_ = nullptr;
    }
}

void MainMenu::unselectAll()
{
    if(!selected_btn_) return;

    selected_btn_->setProperty("selected", false);
    selected_btn_->style()->unpolish(selected_btn_);
    selected_btn_->style()->polish(selected_btn_);
    selected_btn_->update();

    selected_btn_ = nullptr;
}
