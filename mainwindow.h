#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QMessageBox>
#include <QFileDialog>
#include <QRegularExpression>
#include <QByteArray>
#include <QTimer>
#include <QDateTime>
#include <QMap>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;


private slots:
    void on_pushButton_start_clicked();
    void on_pushButton_stop_clicked();
    void on_radioButton_3_toggled(bool checked);
    void on_radioButton_4_toggled(bool checked);
    void processFiles();  // Основная функция обработки
    void processRealtime(); // Слот для таймера
    void on_pushButton_input_clicked();
    void on_pushButton_output_clicked();


private:
    Ui::MainWindow *ui;
    // Таймер для режима реального времени
    QTimer *timer;

    // Флаг работы в реальном времени
    bool isRealtimeMode;

    // Флаг остановки процесса
    bool stopRequested;

    // Хранилище для информации о прерванных файлах
    // Ключ: путь к файлу, Значение: смещение (позиция) для возобновления
    QMap<QString, qint64> pausedFiles;

    // Текущая обрабатываемая директория (для возобновления)
    QString currentInputPath;
    QString currentOutputPath;
    QString currentMask;
    QByteArray currentKeyBytes;
    bool currentDeleteSource;
    bool currentOverwriteExisting;
    bool currentAutoRename;
};
#endif // MAINWINDOW_H
