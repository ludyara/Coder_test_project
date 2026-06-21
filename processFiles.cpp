#include "mainwindow.h"
#include "ui_mainwindow.h"

// QByteArray hexStringToByteArray(const QString &hexString);
// // void MainWindow::lockControls(bool locked);

void MainWindow::startProcessing()
{
    if (isProcessing) {
        return;
    }

    // 1. Получаем пути из полей ввода
    QString inputPath = ui->lineEdit_2->text().trimmed();
    QString outputPath = ui->lineEdit_3->text().trimmed();
    QString mask = ui->lineEdit->text().trimmed();
    QString hexKey = ui->lineEdit_5->text().trimmed();
    QString cleanKey = hexKey;
    cleanKey.remove(' '); // Убираем возможные пробелы для проверки
    // Преобразуем hex-строку в массив байт
    QByteArray keyBytes = hexStringToByteArray(cleanKey);

    // Определение режима обработки конфликтов
    bool overwriteExisting = false;
    bool autoRename = false;

    if (ui->radioButton->isChecked()) {
        overwriteExisting = true;
        autoRename = false;
    } else if (ui->radioButton_2->isChecked()) {
        overwriteExisting = false;
        autoRename = true;
    } else {
        overwriteExisting = false;
        autoRename = true;
    }

    // Сохраняем параметры для возможного возобновления
    currentInputPath = inputPath;
    currentOutputPath = outputPath;
    currentMask = mask;
    currentKeyBytes = keyBytes;
    currentDeleteSource = ui->checkBox->isChecked();
    currentOverwriteExisting = overwriteExisting;
    currentAutoRename = autoRename;

    // Сбрасываем флаг остановки
    stopRequested = false;
    isProcessing = true;

    // Сбрасываем прогресс
    ui->progressBar->setValue(0);
    ui->label_status->setText("Подготовка к обработке...");

    isProcessing = true;

    // Очищаем старый поток, если есть
    cleanupThread();

    // Создаём новый поток
    workerThread = new QThread(this);
    worker = new WorkerThread();
    worker->moveToThread(workerThread);

    // Подключаем сигналы
    connect(workerThread, &QThread::started, worker, &WorkerThread::process);
    connect(workerThread, &QThread::finished, workerThread, &QThread::deleteLater);

    connect(worker, &WorkerThread::progressUpdated, this, &MainWindow::onProgressUpdated);
    connect(worker, &WorkerThread::statusUpdated, this, &MainWindow::onStatusUpdated);
    connect(worker, &WorkerThread::finished, this, &MainWindow::onProcessingFinished);
    connect(worker, &WorkerThread::errorOccurred, this, &MainWindow::onErrorOccurred);

    // Передаём параметры в рабочий поток
    worker->setParameters(
        currentInputPath,
        currentOutputPath,
        currentMask,
        currentKeyBytes,
        currentDeleteSource,
        currentOverwriteExisting,
        currentAutoRename,
        pausedFiles
        );

    // Запускаем поток
    workerThread->start();
}
