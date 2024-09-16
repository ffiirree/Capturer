#include "path-edit.h"

#include <QFileDialog>
#include <QHBoxLayout>
#include <QLineEdit>
#include <QPushButton>

PathEdit::PathEdit(QWidget *parent)
    : PathEdit("", parent)
{}

PathEdit::PathEdit(const QString& path, bool file, QWidget *parent)
    : QWidget(parent)
{
    const auto hbox = new QHBoxLayout();
    hbox->setSpacing(5);
    hbox->setContentsMargins({});
    setLayout(hbox);

    path_ = new QLineEdit(path);
    path_->setReadOnly(true);
    path_->setContextMenuPolicy(Qt::NoContextMenu);

    browse_ = new QPushButton(tr("Browse"));

    hbox->addWidget(path_);
    hbox->addWidget(browse_);

    connect(browse_, &QPushButton::clicked, [=, this] {
        if (file) {
            const auto dir = QFileDialog::getOpenFileName(nullptr, {}, path_->text());
            if (!dir.isEmpty()) {
                path_->setText(dir);
                emit changed(dir);
            }
        }
        else {
            const auto dir = QFileDialog::getExistingDirectory(nullptr, {}, path_->text());
            if (!dir.isEmpty()) {
                path_->setText(dir);
                emit changed(dir);
            }
        }
    });
}

QString PathEdit::path() const { return path_->text(); }