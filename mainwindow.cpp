#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    // connect(ui->pushButton_start, &QPushButton::clicked,
    //         this, &MainWindow::on_pushButton_start_clicked);
    // Инициализация таймера
    timer = new QTimer(this);
    timer->setSingleShot(false); // Будет срабатывать периодически

    // Подключаем таймер к слоту обработки в реальном времени
    // connect(timer, &QTimer::timeout, this, &MainWindow::processRealtime);

    // Инициализация флагов
    isRealtimeMode = false;
    stopRequested = false;

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
    timer = new QTimer(this);
    timer->setSingleShot(false);
    connect(timer, &QTimer::timeout, this, &MainWindow::processRealtime);

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

    // Потом надо будет удалить (Инициализация переменных)
    ui->lineEdit->setText("test.txt");
    ui->lineEdit_2->setText("D:\\Projects\\Qt\\test\\Temp\\in");
    ui->lineEdit_3->setText("D:\\Projects\\Qt\\test\\Temp\\out");
    ui->lineEdit_5->setText("0000000000000001");
}

MainWindow::~MainWindow()
{
    // Останавливаем таймеры
    if (timer->isActive()) timer->stop();
    if (progressTimer->isActive()) progressTimer->stop();

    delete ui;
}


// Вспомогательный метод для преобразования hex-строки в QByteArray
QByteArray hexStringToByteArray(const QString &hexString)
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
    // Преобразуем hex-строку в массив байт
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

    // Сбрасываем флаг остановки
    stopRequested = false;

    // Сбрасываем прогресс
    ui->progressBar->setValue(0);
    ui->label_status->setText("Подготовка к обработке...");

    if (ui->radioButton_3->isChecked()) {
        // Режим реального времени
        QTime time = ui->timeEdit->time();
        int seconds = time.hour() * 3600 + time.minute() * 60 + time.second();
        if (seconds < 1) seconds = 1; // Минимум 1 секунда

        timer->start(seconds * 1000); // QTimer работает в миллисекундах

        // Запускаем таймер прогресса
        remainingSeconds = seconds;
        progressTimer->start(1000);

        // Первый запуск сразу
        processRealtime();
    } else {
        // Разовый режим
        processFiles();
    }
}

// Кнопка "Стоп"
void MainWindow::on_pushButton_stop_clicked()
{
    // Устанавливаем флаг остановки
    stopRequested = true;

    // Если таймер активен, останавливаем его
    if (timer->isActive()) {
        timer->stop();
    }

    // Останавливаем все таймеры
    if (timer->isActive()) {
        timer->stop();
    }
    if (progressTimer->isActive()) {
        progressTimer->stop();
    }

    // Блокируем кнопку запуска, чтобы предотвратить повторный запуск
    ui->pushButton_start->setEnabled(true);

    // Разблокируем остальные элементы управления
    lockControls(false);

    ui->label_status->setText("Остановлено пользователем");

    QMessageBox::information(this, "Остановка",
                             "Процесс остановлен.\n"
                             "Нажмите 'Запуск' для возобновления обработки.");
    ui->progressBar->setValue(0);
}


void MainWindow::processRealtime()
{
    // Проверяем, не был ли запрошен стоп
    if (stopRequested) {
        timer->stop();
        progressTimer->stop();
        lockControls(false);

        ui->label_status->setText("Остановлено пользователем");
        ui->progressBar->setValue(0);
        return;
    }

    // Обновляем статус перед обработкой
    ui->label_status->setText("Запуск опроса файлов...");

    // Запускаем обработку
    processFiles();

    // Сбрасываем прогресс-бар для режима реального времени
    if (!stopRequested) {
        // Сбрасываем прогресс-бар, так как он использовался для файлов
        ui->progressBar->setValue(0);

        // Обновляем статус
        int seconds = ui->timeEdit->time().second();
        if (seconds < 1) seconds = 1;
        ui->label_status->setText("Ожидание следующего опроса: " +
                                  QString::number(seconds) + " сек.");

        // Запускаем таймер прогресса для обратного отсчёта
        remainingSeconds = seconds;
        progressTimer->start(1000);
    }
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

void MainWindow::updateRealtimeProgress()
{
    if (remainingSeconds > 0) {
        remainingSeconds--;

        // Обновляем прогресс-бар
        int totalSeconds = ui->timeEdit->time().second();
        if (totalSeconds < 1) totalSeconds = 1;

        int progress = ((totalSeconds - remainingSeconds) * 100) / totalSeconds;
        ui->progressBar->setValue(progress);

        // Обновляем статус
        ui->label_status->setText("Следующий опрос через: " +
                                  QString::number(remainingSeconds) + " сек.");
    } else {
        // Если время вышло, останавливаем таймер прогресса
        progressTimer->stop();
    }
}