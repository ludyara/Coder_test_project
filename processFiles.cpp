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

// void MainWindow::processFiles()
// {
//     QString inputPath = ui->lineEdit_2->text().trimmed();
//     QString outputPath = ui->lineEdit_3->text().trimmed();
//     QString mask = ui->lineEdit->text().trimmed();
//     QString hexKey = ui->lineEdit_5->text().trimmed();
//     QString cleanKey = hexKey;
//     cleanKey.remove(' ');
//     QRegularExpression hexRegex("^[0-9A-Fa-f]+$");
//     QByteArray keyBytes = hexStringToByteArray(cleanKey);
//     QDir inputDir(inputPath);
//     QDir outputDir(outputPath);

//     // Определение режима обработки конфликтов
//     bool overwriteExisting = false;
//     bool autoRename = false;

//     if (ui->radioButton->isChecked()) {
//         overwriteExisting = true;
//         autoRename = false;
//     } else if (ui->radioButton_2->isChecked()) {
//         overwriteExisting = false;
//         autoRename = true;
//     } else {
//         overwriteExisting = false;
//         autoRename = true;
//     }

//     // Сохраняем параметры для возможного возобновления
//     currentInputPath = inputPath;
//     currentOutputPath = outputPath;
//     currentMask = mask;
//     currentKeyBytes = keyBytes;
//     currentDeleteSource = ui->checkBox->isChecked();
//     currentOverwriteExisting = overwriteExisting;
//     currentAutoRename = autoRename;

//     // Получение списка файлов
//     QStringList files = inputDir.entryList(QStringList() << mask, QDir::Files);

//     if (files.isEmpty()) {
//         ui->label_status->setText("Файлы по маске '" + mask + "' не найдены в папке:\n" + inputPath);
//         ui->progressBar->setValue(0);
//         lockControls(false);
//         return;
//     }

//     // Инициализация прогресса
//     totalFiles = files.size();
//     currentFileIndex = 0;

//     // Настраиваем progressBar для разового режима
//     ui->progressBar->setRange(0, 100);
//     ui->progressBar->setValue(0);

//     // Обработка файлов
//     bool deleteSource = ui->checkBox->isChecked();
//     int processedCount = 0;
//     QStringList errors;

//     for (int i = 0; i < files.size(); ++i) {
//         // Проверяем, не был ли запрошен стоп
//         if (stopRequested) {
//             // Сохраняем прогресс для возобновления
//             // В данном случае мы не можем возобновить с середины файла,
//             // но можем запомнить последний обработанный файл
//             ui->label_status->setText("Обработка прервана на файле: " + files[i]);
//             return;
//         }
//         ui->label_status->setText("Обработка файла: " + files[i]);

//         const QString &fileName = files[i];
//         QString inputFilePath = inputDir.filePath(fileName);
//         QString outputFilePath = outputDir.filePath(fileName);

//         // Обработка конфликта имён
//         QFile outputFile(outputFilePath);
//         bool fileExists = outputFile.exists();

//         if (fileExists && !overwriteExisting && autoRename) {
//             ui->label_status->setText("Поиск свободного имени для: " + fileName);
//             QFileInfo fileInfo(fileName);
//             QString baseName = fileInfo.baseName();
//             QString suffix = fileInfo.suffix();
//             QString newFileName;
//             int counter = 2;

//             do {
//                 newFileName = baseName + "_" + QString::number(counter);
//                 if (!suffix.isEmpty()) {
//                     newFileName += "." + suffix;
//                 }
//                 outputFilePath = outputDir.filePath(newFileName);
//                 outputFile.setFileName(outputFilePath);
//                 counter++;
//             } while (outputFile.exists());
//             ui->label_status->setText("Файл переименован в: " + newFileName);
//         }


//         // Чтение и обработка файла
//         ui->label_status->setText("Чтение файла: " + fileName);
//         QFile inputFile(inputFilePath);
//         if (!inputFile.open(QIODevice::ReadOnly)) {
//             ui->label_status->setText("Ошибка чтения: " + fileName);
//             errors << fileName + " (не удалось открыть для чтения)";
//             continue;
//         }

//         ui->label_status->setText("Обработка файла: " + fileName);

//         if (!outputFile.open(QIODevice::WriteOnly))
//         {
//             ui->label_status->setText("Ошибка записи: " + fileName);
//             errors << fileName + " (не удалось создать выходной файл)";
//             continue;
//         }

//         const qint64 CHUNK_SIZE = 4 * 1024 * 1024; // 4 MB

//         qint64 fileSize = inputFile.size();
//         qint64 processedBytes = 0;
//         while (!inputFile.atEnd())
//         {
//             if (stopRequested)
//             {
//                 outputFile.close();
//                 inputFile.close();
//                 ui->label_status->setText("Обработка остановлена");
//                 return;
//             }

//             QByteArray chunk =
//                 inputFile.read(CHUNK_SIZE);

//             if (chunk.isEmpty())
//                 break;

//             for (int i = 0; i < chunk.size(); ++i)
//             {
//                 chunk[i] = chunk[i] ^ keyBytes[(processedBytes + i) % 8];
//             }
//             qint64 written = outputFile.write(chunk);

//             if (written != chunk.size())
//             {
//                 errors << fileName + " (ошибка записи)";
//                 break;
//             }
//             processedBytes += chunk.size();
//             int percent = static_cast<int>(processedBytes * 100 / fileSize);
//             ui->progressBar->setValue(percent);
//             qApp->processEvents();
//         }

//         inputFile.close();
//         outputFile.close();

//         // if (bytesWritten != modifiedData.size()) {
//         //     errors << fileName + " (неполная запись)";
//         //     continue;
//         // }

//         // Удаление исходного файла
//         if (deleteSource) {
//             ui->label_status->setText("Удаление исходного файла: " + fileName);
//             if (!inputFile.remove()) {
//                 errors << fileName + " (не удалось удалить исходный файл)";
//             }
//         }

//         processedCount++;
//         currentFileIndex = i + 1;

//         // --- Обновляем прогресс ---
//         ui->progressBar->setValue(currentFileIndex);

//         // Если файл успешно обработан, удаляем его из списка приостановленных
//         pausedFiles.remove(inputFilePath);
//     }

//     // Показываем результат
// 	if (errors.isEmpty()) {
//         ui->label_status_2->setText("Обработано файлов: " + QString::number(processedCount) + " из " + QString::number(totalFiles));
// 	} else {
// 		ui->label_status_2->setText("Обработано файлов: " + QString::number(processedCount) + "\n\nОшибки при обработке:\n" + errors.join("\n"));
// 	}

//     ui->progressBar->setValue(100);

//     // Разблокировка элементов
//     lockControls(false);

//     // Сбрасываем флаг остановки
//     stopRequested = false;
// }