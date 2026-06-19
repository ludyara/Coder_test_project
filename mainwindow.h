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
#include <QProgressBar>
#include <QThread>
#include "workerthread.h"
#include <QCloseEvent>

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
    // void processFiles();  // Основная функция обработки
    // void processRealtime(); // Слот для таймера
    void on_pushButton_input_clicked();
    void on_pushButton_output_clicked();
    // Слоты для обновления GUI из потока
    void onProgressUpdated(int current, int total);
    void onStatusUpdated(const QString &status);
    void onFileProgressUpdated(int percent);
    void onProcessingFinished(int processedCount, int errorCount, const QStringList &errors);
    void onErrorOccurred(const QString &error);
    void closeEvent(QCloseEvent *event);
    void updateRealtimeProgress();
    // QByteArray hexStringToByteArray(QString &hexString);

private:
    Ui::MainWindow *ui;
    // Таймер для режима реального времени
    QTimer *timer;
    QTimer *progressTimer;
    // Флаг работы в реальном времени
    bool isRealtimeMode;

    // Флаг остановки процесса
    bool stopRequested;
    bool isProcessing;
	
	// Данные для прогресса
    int totalFiles;
    int currentFileIndex;
    int remainingSeconds;
    
    // Рабочий поток
    QThread *workerThread;
    WorkerThread *worker;
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

    void lockControls(bool locked);
    void cleanupThread();
};
#endif // MAINWINDOW_H
