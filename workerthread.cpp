#include "workerthread.h"
#include <QFileInfo>
#include <QDebug>

WorkerThread::WorkerThread(QObject *parent)
    : QObject(parent)
    , m_stopRequested(false)
{
}

void WorkerThread::setParameters(const QString &inputPath,
                                 const QString &outputPath,
                                 const QString &mask,
                                 const QByteArray &keyBytes,
                                 bool deleteSource,
                                 bool overwriteExisting,
                                 bool autoRename,
                                 const QMap<QString, qint64> &pausedFiles)
{
    m_inputPath = inputPath;
    m_outputPath = outputPath;
    m_mask = mask;
    m_keyBytes = keyBytes;
    m_deleteSource = deleteSource;
    m_overwriteExisting = overwriteExisting;
    m_autoRename = autoRename;
    m_pausedFiles = pausedFiles;
}

void WorkerThread::stop()
{
    m_stopRequested = true;
}

void WorkerThread::process()
{
    m_stopRequested = false;

    try {
        QDir inputDir(m_inputPath);
        QDir outputDir(m_outputPath);

        // Получаем список файлов
        QStringList files = inputDir.entryList(QStringList() << m_mask, QDir::Files);

        // Инициализация прогресса
        int totalFiles = files.size();
        int processedCount = 0;
        QStringList errors;

        for (int i = 0; i < files.size(); ++i) {
            if (m_stopRequested) {
                emit statusUpdated("Остановка на файле: " + files[i]);
                emit finished(processedCount, errors.size(), errors);
                return;
            }

            const QString &fileName = files[i];
            QString inputFilePath = inputDir.filePath(fileName);
            QString outputFilePath = outputDir.filePath(fileName);

            emit statusUpdated("Обработка файла: " + fileName);


            // Проверяем, есть ли файл в списке прерванных
            qint64 resumePosition = 0;
            if (m_pausedFiles.contains(inputFilePath)) {
                resumePosition = m_pausedFiles[inputFilePath];
                emit statusUpdated("Возобновление файла: " + fileName +
                                   " с позиции " + QString::number(resumePosition));
            }

            // Разрешаем конфликт имён
            outputFilePath = resolveFileName(fileName, outputDir);

            // Обрабатываем файл
            qint64 processedBytes = 0;
            int percent = 0;
            bool success = processFile(inputFilePath, outputFilePath,
                                       processedBytes, percent, i, totalFiles);

            if (!success) {
                errors << fileName + " (ошибка обработки)";
                continue;
            }

            // Удаляем исходный файл
            if (m_deleteSource) {
                emit statusUpdated("Удаление исходного файла: " + fileName);
                QFile inputFile(inputFilePath);
                if (!inputFile.remove()) {
                    errors << fileName + " (не удалось удалить)";
                }
            }

            processedCount++;

            // Удаляем из списка прерванных
            if (m_pausedFiles.contains(inputFilePath)) {
                m_pausedFiles.remove(inputFilePath);
            }
        }

        // Отправляем сигнал о завершении
        emit statusUpdated("Обработка завершена");
        emit finished(processedCount, errors.size(), errors);

    } catch (const std::exception &e) {
        emit errorOccurred("Критическая ошибка: " + QString(e.what()));
        emit finished(0, 0, QStringList());
    }
}

bool WorkerThread::processFile(const QString &inputFilePath,
                               const QString &outputFilePath,
                               qint64 &processedBytes,
                               int &percent,
                               int &current,
                               int &total)
{
    const qint64 CHUNK_SIZE = 1024 * 1024; // 1 MB чанк

    QFile inputFile(inputFilePath);
    if (!inputFile.open(QIODevice::ReadOnly)) {
        emit errorOccurred("Не удалось открыть файл: " + inputFilePath);
        return false;
    }

    QFile outputFile(outputFilePath);
    if (!outputFile.open(QIODevice::WriteOnly)) {
        inputFile.close();
        emit errorOccurred("Не удалось создать файл: " + outputFilePath);
        return false;
    }

    qint64 fileSize = inputFile.size();
    processedBytes = 0;

    // Если есть сохранённая позиция, восстанавливаем
    if (m_pausedFiles.contains(inputFilePath)) {
        qint64 resumePos = m_pausedFiles[inputFilePath];
        // Проверяем, что позиция валидна
        if (resumePos < fileSize) {
            // Перемещаем указатели на сохранённую позицию
            inputFile.seek(resumePos);
            outputFile.seek(resumePos);
            processedBytes = resumePos;

            emit statusUpdated("Возобновлено с байта " + QString::number(resumePos) +
                               " (" + QString::number((resumePos * 100) / fileSize) + "%)");
        } else {
            // Если позиция больше размера файла, начинаем с начала
            emit statusUpdated("Позиция возобновления больше размера файла. Начинаем с начала.");
        }
    }

    while (true) {
        // Проверяем остановку
        if (m_stopRequested) {
            // Сохраняем позицию для возобновления
            qint64 currentPos = inputFile.pos();
            m_pausedFiles[inputFilePath] = currentPos;
            emit statusUpdated("Остановка на байте " + QString::number(currentPos));
            inputFile.close();
            outputFile.close();
            return false;
        }

        // Читаем чанк
        QByteArray chunk = inputFile.read(CHUNK_SIZE);
        if (chunk.isEmpty()) {
            break; // Конец файла
        }

        // Обрабатываем чанк (XOR)
        QByteArray processedChunk;
        processedChunk.reserve(chunk.size());

        for (int byteIndex = 0; byteIndex < chunk.size(); ++byteIndex) {
            int keyIndex = (processedBytes + byteIndex) % 8;
            char keyByte = m_keyBytes[keyIndex];
            char modifiedByte = chunk[byteIndex] ^ keyByte;
            processedChunk.append(modifiedByte);
        }

        // Записываем чанк
        qint64 bytesWritten = outputFile.write(processedChunk);
        if (bytesWritten != processedChunk.size()) {
            emit errorOccurred("Ошибка записи в файл: " + outputFilePath);
            inputFile.close();
            outputFile.close();
            return false;
        }

        processedBytes += chunk.size();

        // Обновляем прогресс обработки текущего файла
        percent = (processedBytes * 100) / fileSize;
        // emit fileProgressUpdated(percent);

        emit progressUpdated(current, total, percent);
    }

    inputFile.close();
    outputFile.close();
    return true;
}

QString WorkerThread::resolveFileName(const QString &fileName, const QDir &outputDir)
{
    QString outputFilePath = outputDir.filePath(fileName);
    QFile outputFile(outputFilePath);

    if (!outputFile.exists() || m_overwriteExisting) {
        return outputFilePath;
    }

    if (m_autoRename) {
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

        emit statusUpdated("Файл переименован в: " + newFileName);
        return outputFilePath;
    }

    return outputFilePath;
}