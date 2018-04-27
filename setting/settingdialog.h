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

class SettingWindow : public QWidget
{
    Q_OBJECT

public:
    explicit SettingWindow(QWidget * parent = nullptr);
    ~SettingWindow();

    nlohmann::json settings() const { return settings_; }

signals:
    void snipShortcutChanged(const QKeySequence &);
    void pinImgShortcutChanged(const QKeySequence &);
    void gifShortcutChanged(const QKeySequence &);
    void videoShortcutChanged(const QKeySequence &);

    void snipBorderColorChanged(const QColor&);
    void snipBorderWidthChanged(int);
    void snipBorderStyleChanged(Qt::PenStyle);
    void snipMaskColorChanged(const QColor&);
    void snipDetectWindowChanged(bool);

    void recordBorderColorChanged(const QColor&);
    void recordBorderWidthChanged(int);
    void recordBorderStyleChanged(Qt::PenStyle);
    void recordMaskColorChanged(const QColor&);
    void recordDetectWindowChanged(bool);
    void recordFramerateChanged(int);

    void gifBorderColorChanged(const QColor&);
    void gifBorderWidthChanged(int);
    void gifBorderStyleChanged(Qt::PenStyle);
    void gifMaskColorChanged(const QColor&);
    void gifDetectWindowChanged(bool);
    void gifFPSChanged(int);

private slots:
    void setAutoRun(int);

private:
    void setupGeneralWidget();
    void setupAppearanceWidget();
    void setupSnipWidget();
    void setupRecordWidget();
    void setupGIFWidget();
    void setupHotkeyWidget();
    void setupAboutWidget();

    void config();

    QTabWidget * tabw_ = nullptr;

    QWidget * background_ = nullptr;

    QWidget * general_ = nullptr;
    QWidget * appearance_ = nullptr;
    QWidget * snip_ = nullptr;
    QWidget * record_ = nullptr;
    QWidget * gif_ = nullptr;
    QWidget * hotkey_ = nullptr;
    QWidget * about_ = nullptr;

    nlohmann::json settings_;

    QString config_dir_path_{};
    QString config_file_path_{};

    static QString default_settings_;
};

#endif // SETTINGDIALOG_H
