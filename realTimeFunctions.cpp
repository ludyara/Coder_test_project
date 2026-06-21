#include "mainwindow.h"
#include "ui_mainwindow.h"

void MainWindow::startRealtimeMode()
{
    // if (isRealtimeProcessing) {
    //     return; // Уже выполняется
    // }

    // Получаем интервал опроса
    QTime time = ui->timeEdit->time();
    int seconds = time.hour() * 3600 + time.minute() * 60 + time.second();
    if (seconds < 1) seconds = 1;

    totalRealtimeSeconds = seconds;
    remainingSeconds = seconds;

    // Запускаем таймер опроса
    realtimeTimer->start(seconds * 1000);

    // Запускаем таймер прогресса (обратный отсчёт)
    progressTimer->start(1000);

    ui->label_status->setText("Режим реального времени запущен. Опрос каждые " +
                              QString::number(seconds) + " сек.");
    ui->progressBar->setValue(0);

    isRealtimeProcessing = true;

    // Первый запуск сразу
    startProcessing();
}

void MainWindow::onRealtimeTimer()
{
    // Проверяем, не была ли запрошена остановка
    if (stopRequested) {
        realtimeTimer->stop();
        progressTimer->stop();
        ui->label_status->setText("Остановлено пользователем");
        ui->progressBar->setValue(0);
        return;
    }

    // Проверяем, не выполняется ли уже обработка
    if (isProcessing) {
        ui->label_status->setText("Предыдущая обработка ещё выполняется. Пропуск опроса.");
        return;
    }

    // Запускаем обработку
    ui->label_status->setText("Запуск опроса файлов...");
    ui->progressBar->setValue(0);

    startProcessing();
}

void MainWindow::updateRealtimeProgress()
{
    // Получаем оставшееся время таймера в миллисекундах
    int remainingMs = realtimeTimer->remainingTime();

    // Переводим миллисекунды в секунды
    int remainingSecondsFromTimer = remainingMs / 1000;

    if (remainingSeconds - remainingSecondsFromTimer > 0) {
        // remainingSeconds--;

        // Обновляем статус
        if (!isProcessing) {
            ui->label_status->setText("Следующий опрос через: " +
                                      QString::number(remainingSecondsFromTimer) + " сек.");
        }
    } else {
        // Если время вышло, останавливаем таймер прогресса
        // Он будет перезапущен при следующем опросе
        progressTimer->stop();
    }
}