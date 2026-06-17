#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    connect(ui->pushButton_start, &QPushButton::clicked,
            this, &MainWindow::on_pushButton_start_clicked);
}

MainWindow::~MainWindow()
{
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

    if (files.isEmpty()) {
        QMessageBox::information(this, "Информация",
                                 "Файлы по маске '" + mask +
                                     "' не найдены в папке:\n" + inputPath);
        return;
    }
    // Получаем состояние галочки "удалять исходные файлы"
    bool deleteSource = ui->checkBox->isChecked();

    // --- Обработка файлов ---
    int processedCount = 0;
    QStringList errors;

    for (int i = 0; i < files.size(); ++i) {
        const QString &fileName = files[i];
        QString inputFilePath = inputDir.filePath(fileName);
        QString outputFilePath = outputDir.filePath(fileName);

        QFile inputFile(inputFilePath);
        if (!inputFile.open(QIODevice::ReadOnly)) {
            errors << fileName + " (не удалось открыть для чтения)";
            continue;
        }

        QByteArray fileData = inputFile.readAll();
        inputFile.close();

        if (fileData.isEmpty()) {
            errors << fileName + " (файл пуст)";
            continue;
        }

        QByteArray modifiedData;
        modifiedData.reserve(fileData.size());

        for (int byteIndex = 0; byteIndex < fileData.size(); ++byteIndex) {
            int keyIndex = byteIndex % 8;
            char keyByte = keyBytes[keyIndex];
            char modifiedByte = fileData[byteIndex] ^ keyByte;
            modifiedData.append(modifiedByte);
        }

        QFile outputFile(outputFilePath);
        if (!outputFile.open(QIODevice::WriteOnly)) {
            errors << fileName + " (не удалось создать выходной файл)";
            continue;
        }

        qint64 bytesWritten = outputFile.write(modifiedData);
        outputFile.close();

        if (bytesWritten != modifiedData.size()) {
            errors << fileName + " (неполная запись)";
            continue;
        }

        if (deleteSource) {
            if (!inputFile.remove()) {
                errors << fileName + " (не удалось удалить исходный файл)";
            }
        }

        processedCount++;
    }
    // Показываем результат
    QString message = "Обработано файлов: " + QString::number(processedCount);
    if (!errors.isEmpty()) {
        message += "\nОшибки при обработке: " + errors.join(", ");
    }
    QMessageBox::information(this, "Готово", message);
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


