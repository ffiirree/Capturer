#ifndef SETTINGDIALOG_H
#define SETTINGDIALOG_H

#include <QDialog>
#include <QTableWidget>
#include <QVBoxLayout>
#include <QGridLayout>
#include <QCheckBox>
#include <QSettings>
#include <QDir>
#include <QCoreApplication>
#include <QLabel>
#include <QLineEdit>
#include "json.hpp"

#define JSON_QSTR(x)    QString::fromStdString((x).get<std::string>())
#define GET_SETTING(X)  JSON_QSTR(settings()X)

class SettingDialog : public QFrame
{
    Q_OBJECT

public:
    explicit SettingDialog(QWidget * parent = nullptr);
    ~SettingDialog();

    nlohmann::json settings() const { return settings_; }

signals:
    void snipShortcutChanged(const QKeySequence &);
    void fixImgShortcutChanged(const QKeySequence &);
    void gifShortcutChanged(const QKeySequence &);
    void videoShortcutChanged(const QKeySequence &);

private slots:
    void setAutoRun(int);

private:
    void setupGeneralWidget();
    void setupAppearanceWidget();
    void setupHotkeyWidget();
    void setupAboutWidget();

    void config();

    QTabWidget * tabw_ = nullptr;

    QWidget * background_ = nullptr;

    QWidget * general_ = nullptr;
    QWidget * appearance_ = nullptr;
    QWidget * hotkey_ = nullptr;
    QWidget * about_ = nullptr;

    nlohmann::json settings_;

    QString config_dir_path_{};
    QString config_file_path_{};

    static QString default_settings_;
};

#endif // SETTINGDIALOG_H
