#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
    , isProcessing(false)
	, isRealtimeProcessing(false)
    , totalRealtimeSeconds(5)
{
    ui->setupUi(this);

    // По умолчанию скрываем элементы для режима реального времени
    ui->label_8->setVisible(false);
    ui->timeEdit->setVisible(false);

    // Устанавливаем минимальное время 1 секунда
    ui->timeEdit->setMinimumTime(QTime(0, 0, 1));
    ui->timeEdit->setTime(QTime(0, 0, 5)); // По умолчанию 5 секунд

    // Устанавливаем radioButton по умолчанию
    ui->radioButton->setChecked(true);
    ui->radioButton_4->setChecked(true);

    // --- Инициализация таймеров ---
	realtimeTimer = new QTimer(this);
    realtimeTimer->setSingleShot(false);  // Будет срабатывать периодически
    connect(realtimeTimer, &QTimer::timeout, this, &MainWindow::onRealtimeTimer);

    // Таймер для обновления прогресса в реальном времени
    progressTimer = new QTimer(this);
    progressTimer->setSingleShot(false);
    progressTimer->setInterval(1000); // Обновляем каждый заданный интервал
    connect(progressTimer, &QTimer::timeout, this, &MainWindow::updateRealtimeProgress);

    // Инициализация переменных
    isRealtimeMode = false;
    stopRequested = false;
    totalFiles = 0;
    currentFileIndex = 0;
    remainingSeconds = 0;

    // Настройка progressBar
    ui->progressBar->setRange(0, 100);
    ui->progressBar->setValue(0);

    // Инициализация статуса
    ui->label_status->setText("Готов к работе");
    ui->label_status_2->setText("Ожидание запуска");

    // Инициализация рабочего потока
    workerThread = nullptr;
    worker = nullptr;

    // Потом надо будет удалить (Инициализация переменных)
    // ui->lineEdit->setText("test.txt");
    // ui->lineEdit_2->setText("D:\\Projects\\Qt\\test\\Temp\\in");
    // ui->lineEdit_3->setText("D:\\Projects\\Qt\\test\\Temp\\out");
    ui->lineEdit_5->setText("0000000000000000");
}

MainWindow::~MainWindow()
{
	cleanupThread();
    // Останавливаем таймеры
    // if (timer->isActive()) timer->stop();
    if (progressTimer->isActive()) progressTimer->stop();

    delete ui;
}

void MainWindow::cleanupThread()
{
    if (workerThread) {
        if (workerThread->isRunning()) {
            workerThread->quit();
            workerThread->wait(5000);
        }
        delete workerThread;
        workerThread = nullptr;
    }
    if (worker) {
        delete worker;
        worker = nullptr;
    }
}

// Кнопка "Старт"
void MainWindow::on_pushButton_start_clicked() {

    // 1. Получаем пути из полей ввода
    QString inputPath = ui->lineEdit_2->text().trimmed();
    QString outputPath = ui->lineEdit_3->text().trimmed();
    QString mask = ui->lineEdit->text().trimmed();
    QString hexKey = ui->lineEdit_5->text().trimmed();
    QString cleanKey = hexKey;
    cleanKey.remove(' '); // Убираем возможные пробелы для проверки
	
    // 2. Проверяем, что все поля заполнены
    if (mask.isEmpty()) {
        QMessageBox::warning(this, "Ошибка", "Пожалуйста, введите маску файлов.");
        return;
    }

    if (inputPath.isEmpty()) {
        QMessageBox::warning(this, "Ошибка",
                             "Пожалуйста, укажите путь к входным файлам.");
        return;
    }

    if (outputPath.isEmpty()) {
        QMessageBox::warning(
            this, "Ошибка", "Пожалуйста, укажите путь для сохранения результатов.");
        return;
    }

    if (cleanKey.isEmpty()) {
        QMessageBox::warning(this, "Ошибка", "Пожалуйста, введите 8-байтный ключ в hex-формате.");
        return;
    }

    // 3. Проверяем существование входной директории
    QDir inputDir(inputPath);
    if (!inputDir.exists()) {
        QMessageBox::warning(this, "Ошибка",
                             "Входная директория не существует:\n" + inputPath);
        return;
    }

    // Проверяем, что длина ровно 8 байт
    if (cleanKey.length() != 16) {
        QMessageBox::warning(this, "Ошибка",
                             "Ключ должен содержать ровно 16 hex-символов\n"
                             "Пример: 1234567890ABCDEF\n"
                             "Текущая длина: " + QString::number(cleanKey.length()) + " символов");
        return;
    }

    // 4. Проверяем или создаём выходную директорию
    QDir outputDir(outputPath);
    if (!outputDir.exists()) {
        // Пробуем создать папку (включая все родительские)
        if (!outputDir.mkpath(".")) {
            QMessageBox::warning(this, "Ошибка",
                                 "Невозможно создать выходную директорию:\n" +
                                     outputPath);
            return;
        }
    }
    // Проверяем, что все символы - hex-цифры
    QRegularExpression hexRegex("^[0-9A-Fa-f]+$");
    if (!hexRegex.match(cleanKey).hasMatch()) {
        QMessageBox::warning(this, "Ошибка",
                             "Ключ содержит недопустимые символы.\n"
                             "Разрешены только цифры 0-9 и буквы A-F/a-f.");
        return;
    }

    // 5. Проверяем, что входная и выходная папки не совпадают
    if (QDir::cleanPath(inputPath) == QDir::cleanPath(outputPath)) {
        QMessageBox::warning(
            this, "Ошибка",
            "Входная и выходная директории совпадают.\n"
            "Укажите разные папки, чтобы не перезаписывать исходные файлы.");
        return;
    }
    // // Преобразуем hex-строку в массив байт
    QByteArray keyBytes = hexStringToByteArray(cleanKey);
    // Проверяем, что получилось ровно 8 байт
    if (keyBytes.size() != 8) {
        QMessageBox::warning(this, "Ошибка",
                             "Не удалось корректно преобразовать ключ.\n"
                             "Убедитесь, что введено ровно 16 hex-символов.");
        return;
    }

    // 6. Получаем список файлов во входной директории по маске
    QStringList files = inputDir.entryList(QStringList() << mask, QDir::Files);

    // Блокируем все элементы управления (если всё гуд)
    lockControls(true);

	
    if (ui->radioButton_3->isChecked()) {
        // Режим реального времени
        QTime time = ui->timeEdit->time();
        int seconds = time.hour() * 3600 + time.minute() * 60 + time.second();
        if (seconds < 1) seconds = 1; // Минимум 1 секунда

        // timer->start(seconds * 1000); // QTimer работает в миллисекундах
        realtimeTimer->start(seconds * 1000);

        // Запускаем таймер прогресса
        remainingSeconds = seconds;
        progressTimer->start(1000);

        // Режим реального времени
        startRealtimeMode();

    //     // Первый запуск сразу
    //     processRealtime();
    } else {
        // Разовый режим
        startProcessing();
    }
}

// Кнопка "Стоп"
void MainWindow::on_pushButton_stop_clicked()
{
    // Устанавливаем флаг остановки
    stopRequested = true;
    isRealtimeProcessing = false;
	
    // Останавливаем таймеры
    if (realtimeTimer && realtimeTimer->isActive()) {
        realtimeTimer->stop();
    }
    if (progressTimer && progressTimer->isActive()) {
        progressTimer->stop();
    }
    
    // Останавливаем рабочий поток
    if (worker) {
        worker->stop();
        QThread::msleep(200);  // Даём время на завершение
    }

    // Разблокируем остальные элементы управления
    lockControls(false);

    ui->label_status->setText("Остановлено пользователем");
    ui->progressBar->setValue(0);
}

void MainWindow::onStatusUpdated(const QString &status)
{
    ui->label_status->setText(status);
}

void MainWindow::on_pushButton_input_clicked()
{
    QString dir = QFileDialog::getExistingDirectory(this,
                                                    "Выберите входную папку",
                                                    ui->lineEdit_2->text());
    if (!dir.isEmpty()) {
        ui->lineEdit_2->setText(dir);
    }
}

void MainWindow::on_pushButton_output_clicked()
{
    QString dir = QFileDialog::getExistingDirectory(this,
                                                    "Выберите папку для сохранения",
                                                    ui->lineEdit_3->text());
    if (!dir.isEmpty()) {
        ui->lineEdit_3->setText(dir);
    }
}

void MainWindow::lockControls(bool locked)
{
    // Блокируем/разблокируем поля ввода
    ui->lineEdit->setDisabled(locked);        // Маска файлов
    ui->lineEdit_2->setDisabled(locked);      // Входной путь
    ui->lineEdit_3->setDisabled(locked);      // Выходной путь
    ui->lineEdit_5->setDisabled(locked);      // Hex-ключ

    // Блокируем/разблокируем радио-кнопки
    ui->radioButton->setDisabled(locked);     // Перезаписывать
    ui->radioButton_2->setDisabled(locked);   // Переименовывать
    ui->radioButton_3->setDisabled(locked);   // Реальное время
    ui->radioButton_4->setDisabled(locked);   // Разовый режим

    // Блокируем/разблокируем чекбокс
    ui->checkBox->setDisabled(locked);        // Удалять исходные

    // Блокируем/разблокируем timeEdit (если видим)
    ui->timeEdit->setDisabled(locked);

    // Блокируем/разблокируем кнопку "Запуск"
    ui->pushButton_start->setDisabled(locked);
}

void MainWindow::onProgressUpdated(int current, int total, int percent)
{
    ui->label_status->setText("Файл " + QString::number(current) +
                                " из " + QString::number(total));
    ui->progressBar->setRange(0, 100);
    int progress;
    if (total > 0) progress = (current  * 100 + percent) / total;
    else progress = 0;

    ui->progressBar->setValue(progress);


}

void MainWindow::onProcessingFinished(int processedCount, int errorCount, const QStringList &errors)
{
    isProcessing = false;

    // Получаем обновлённый список прерванных файлов из потока
    if (worker) {
        pausedFiles = worker->getPausedFiles();
    }




    // Если есть прерванные файлы, показываем это в статусе
    if (!pausedFiles.isEmpty()) {
        ui->label_status->setText("Обработка прервана. " +
                                  QString::number(pausedFiles.size()) +
                                  " файлов ожидают возобновления.");

        // Показываем список прерванных файлов
        QStringList fileNames;
        for (auto it = pausedFiles.begin(); it != pausedFiles.end(); ++it) {
            QFileInfo fileInfo(it.key());
            fileNames << fileInfo.fileName() + " (" + QString::number(it.value().position) + " байт)";
        }
        ui->label_status_2->setText("Прерваны: " + fileNames.join(", "));
    } else {
        if (errorCount == 0) {
            ui->label_status->setText("Обработка завершена успешно");
        } else {
            ui->label_status->setText("Обработка завершена с ошибками");
        }
        ui->label_status_2->setText("Обработано: " + QString::number(processedCount) +
                                    " файлов" + (errorCount > 0 ?
                                                     " (ошибок: " + QString::number(errorCount) + ")" +
                                                         "\nОшибки при обработке:\n" + errors.join("\n") : "")
                                    );
    }


    ui->progressBar->setValue(100);

    // Если ошибок нет или все файлы обработаны, очищаем pausedFiles
    if (processedCount > 0 && errors.isEmpty()) {
        // Проверяем, остались ли файлы в pausedFiles
        if (pausedFiles.isEmpty()) {
            ui->label_status->setText("Все файлы обработаны успешно");
            pausedFiles.clear();
        }
    }
    
    lockControls(false);
    
    // Если режим реального времени, обновляем статус ожидания
    if (isRealtimeMode && !stopRequested) {
        QTime time = ui->timeEdit->time();
        int seconds = time.hour() * 3600 + time.minute() * 60 + time.second();
        if (seconds < 1) seconds = 1;
        remainingSeconds = seconds;
        totalRealtimeSeconds = seconds;
        progressTimer->start(1000);
        // ui->label_status->setText("Ожидание следующего опроса: " +
        //                          QString::number(seconds) + " сек.");
    }
}

void MainWindow::onErrorOccurred(const QString &error)
{
    // QMessageBox::critical(this, "Ошибка", error);
    ui->label_status_2->setText("Ошибка: " + error);
    isProcessing = false;
    lockControls(false);
}

// Вспомогательный метод для преобразования hex-строки в QByteArray
QByteArray MainWindow::hexStringToByteArray(QString &hexString)
{
    QByteArray result;
    QString cleanHex = hexString.trimmed().toUpper();

    // Убираем возможные пробелы
    cleanHex.remove(' ');

    // Проходим по строке попарно
    for (int i = 0; i < cleanHex.length(); i += 2) {
        if (i + 1 < cleanHex.length()) {
            QString byteStr = cleanHex.mid(i, 2);
            bool ok;
            int byteValue = byteStr.toInt(&ok, 16);
            if (ok) {
                result.append(static_cast<char>(byteValue));
            }
        }
    }

    return result;
}

// Обработка закрытия окна
void MainWindow::closeEvent(QCloseEvent *event)
{
    if (isProcessing) {
        QMessageBox::StandardButton reply;
        reply = QMessageBox::question(this, "Подтверждение", 
                                     "Идёт обработка. Прервать и закрыть?",
                                     QMessageBox::Yes | QMessageBox::No);
        if (reply == QMessageBox::Yes) {
            if (worker) {
                worker->stop();
                cleanupThread();
            }
            event->accept();
        } else {
            event->ignore();
        }
    } else {
        event->accept();
    }
}

