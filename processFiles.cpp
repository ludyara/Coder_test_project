#include "mainwindow.h"
#include "ui_mainwindow.h"

QByteArray hexStringToByteArray(const QString &hexString);

void MainWindow::processFiles()
{
    QString inputPath = ui->lineEdit_2->text().trimmed();
    QString outputPath = ui->lineEdit_3->text().trimmed();
    QString mask = ui->lineEdit->text().trimmed();
    QString hexKey = ui->lineEdit_5->text().trimmed();
    QString cleanKey = hexKey;
    cleanKey.remove(' ');
    QRegularExpression hexRegex("^[0-9A-Fa-f]+$");
    QByteArray keyBytes = hexStringToByteArray(cleanKey);
    QDir inputDir(inputPath);
    QDir outputDir(outputPath);

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

    // Получение списка файлов
    QStringList files = inputDir.entryList(QStringList() << mask, QDir::Files);

    if (files.isEmpty()) {
        QMessageBox::information(this, "Информация",
                                 "Файлы по маске '" + mask + "' не найдены в папке:\n" + inputPath);
        return;
    }

    // Обработка файлов
    bool deleteSource = ui->checkBox->isChecked();
    int processedCount = 0;
    QStringList errors;

    for (int i = 0; i < files.size(); ++i) {
        // Проверяем, не был ли запрошен стоп
        if (stopRequested) {
            // Сохраняем прогресс для возобновления
            // В данном случае мы не можем возобновить с середины файла,
            // но можем запомнить последний обработанный файл
            QMessageBox::information(this, "Остановка",
                                     "Обработка прервана на файле: " + files[i]);
            return;
        }

        const QString &fileName = files[i];
        QString inputFilePath = inputDir.filePath(fileName);
        QString outputFilePath = outputDir.filePath(fileName);

        // --- Обработка конфликта имён ---
        QFile outputFile(outputFilePath);
        bool fileExists = outputFile.exists();

        if (fileExists && !overwriteExisting && autoRename) {
            QFileInfo fileInfo(fileName);
            QString baseName = fileInfo.baseName();
            QString suffix = fileInfo.suffix();
            QString newFileName;
            int counter = 2;

            do {
                newFileName = baseName + "_" + QString::number(counter);
                if (!suffix.isEmpty()) {
                    newFileName += "." + suffix;
                }
                outputFilePath = outputDir.filePath(newFileName);
                outputFile.setFileName(outputFilePath);
                counter++;
            } while (outputFile.exists());
        }

        // --- Чтение и обработка файла ---
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

        // Применяем XOR
        QByteArray modifiedData;
        modifiedData.reserve(fileData.size());

        for (int byteIndex = 0; byteIndex < fileData.size(); ++byteIndex) {
            // Проверяем стоп каждые 1024 байта для отзывчивости
            if (byteIndex % 1024 == 0 && stopRequested) {
                // Сохраняем позицию для возобновления
                pausedFiles[inputFilePath] = byteIndex;
                QMessageBox::information(this, "Остановка",
                                         "Обработка файла '" + fileName +
                                             "' прервана на байте " + QString::number(byteIndex));
                return;
            }

            int keyIndex = byteIndex % 8;
            char keyByte = keyBytes[keyIndex];
            char modifiedByte = fileData[byteIndex] ^ keyByte;
            modifiedData.append(modifiedByte);
        }

        // --- Запись выходного файла ---
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

        // --- Удаление исходного файла ---
        if (deleteSource) {
            if (!inputFile.remove()) {
                errors << fileName + " (не удалось удалить исходный файл)";
            }
        }

        processedCount++;

        // Если файл успешно обработан, удаляем его из списка приостановленных
        pausedFiles.remove(inputFilePath);
    }

    // --- Показываем результат ---
    QString message = "Обработано файлов: " + QString::number(processedCount);
    // message += "\nВходная папка: " + inputPath;
    // message += "\nВыходная папка: " + outputPath;
    // message += "\nИспользован ключ: " + cleanKey;

    if (overwriteExisting) {
        message += "\nРежим: перезапись существующих файлов";
    } else if (autoRename) {
        message += "\nРежим: автоматическое переименование (_2, _3, ...)";
    }

    if (deleteSource) {
        message += "\nИсходные файлы удалены";
    }

    if (!errors.isEmpty()) {
        message += "\n\nОшибки при обработке:\n" + errors.join("\n");
    }

    QMessageBox::information(this, "Готово", message);

    // Сбрасываем флаг остановки
    stopRequested = false;
}