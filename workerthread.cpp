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
                                 const QMap<QString, ResumeInfo> &pausedFiles)
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
        QStringList files;
		QDirIterator it(m_inputPath, QStringList() << m_mask, 
						QDir::Files, 
						QDirIterator::Subdirectories);
		while (it.hasNext()) {
			QString filePath = it.next();
			// Получаем относительный путь для сохранения структуры папок
			QString relativePath = QDir(m_inputPath).relativeFilePath(filePath);
			files << relativePath;
        }

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

            const QString &relativePath = files[i];

            // Полный путь к входному файлу
            QString inputFilePath = QDir(m_inputPath).filePath(relativePath);

            // Полный путь к выходному файлу с сохранением структуры папок
            // QString outputFilePath;// = outputDir.filePath(fileName); !!!!!!!!!!!
            QString outputFilePath = QDir(m_outputPath).filePath(relativePath);

            // Создаём необходимые подпапки в выходной директории
            QFileInfo outputFileInfo(outputFilePath);
            QDir outputDirPath = outputFileInfo.absoluteDir();
            if (!outputDirPath.exists()) {
                if (!outputDirPath.mkpath(".")) {
                    errors << relativePath + " (не удалось создать папку)";
                    continue;
                }
            }

            emit statusUpdated("Обработка файла: " + relativePath);

            // Проверяем, есть ли файл в списке прерванных
            qint64 resumePosition = 0;
            if (m_pausedFiles.contains(inputFilePath)) {
                outputFilePath = m_pausedFiles[inputFilePath].outputFilePath;
                resumePosition = m_pausedFiles[inputFilePath].position;
                emit statusUpdated("Возобновление файла: " + relativePath +
                                   " с позиции " + QString::number(resumePosition));
            }
            else
            {
                outputFilePath = resolveFileName(relativePath, outputDir);
            }

            // Обрабатываем файл
            qint64 processedBytes = 0;
            int percent = 0;
            bool success = processFile(inputFilePath, outputFilePath,
                                       processedBytes, percent, i, totalFiles);

            if (!success) {
                errors << relativePath + " (ошибка обработки)";
                continue;
            }

            // Удаляем исходный файл
            if (m_deleteSource) {
                emit statusUpdated("Удаление исходного файла: " + relativePath);
                QFile inputFile(inputFilePath);
                if (!inputFile.remove()) {
                    errors << relativePath + " (не удалось удалить)";
                }
            }

            // Удаляем из списка прерванных
            if (success) {
                processedCount++;
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

    bool isResume = m_pausedFiles.contains(inputFilePath);
    qint64 resumePos = 0;
    
    QFile outputFile(outputFilePath);

	// Определяем нужно ли зоздавать новый файл
    if (isResume) { 
        resumePos = m_pausedFiles[inputFilePath].position; // m_pausedFiles[inputFilePath];
        // ----- добавлено 210626 1612
        if (!inputFile.seek(resumePos))
        {
            inputFile.close();
            outputFile.close();
            return false;
        }
        // if (resumePos > 0)
        // {
        //     inputFile.seek(resumePos);
        //     if (!outputFile.seek(resumePos)) {
        //         return false;
        //     }
        //     processedBytes = resumePos;
        // }
        // -----
        qint64 fileSize = inputFile.size();
        
        // Проверяем, что позиция валидна
        if (resumePos >= fileSize) {
            // Файл уже полностью обработан, начинаем с начала
            isResume = false;
            resumePos = 0;
            emit statusUpdated("Файл уже обработан. Начинаем с начала.");
        } else {
            // Открываем СУЩЕСТВУЮЩИЙ файл для дозаписи
            if (!outputFile.exists(outputFilePath)) {
                // Если файла нет, создаём новый
                isResume = false;
                resumePos = 0;
                emit statusUpdated("Файл не найден. Создаём новый.");
            } else {
                // Открываем существующий файл в режиме ReadWrite
                if (!outputFile.open(QIODevice::ReadWrite)) {
                    emit errorOccurred("Не удалось открыть существующий файл: " + outputFilePath);
                    inputFile.close();
                    return false;
                }
                
                // Перемещаем указатель на позицию возобновления
                if (!outputFile.seek(resumePos)) {
                    emit errorOccurred("Не удалось переместить указатель в файле: " + outputFilePath);
                    inputFile.close();
                    outputFile.close();
                    return false;
                }
                
                processedBytes = resumePos;
                emit statusUpdated("Возобновление с байта " + QString::number(resumePos) + 
                                  " (" + QString::number((resumePos * 100) / fileSize) + "%)");
            }
        }
    }
	else { // Если не возобновление, то создаём новый файл
        // QFile outputFile(outputFilePath);
        if (!outputFile.open(QIODevice::WriteOnly)) {
            inputFile.close();
            emit errorOccurred("Не удалось создать файл: " + outputFilePath);
            return false;
        }
    }
    qint64 fileSize = inputFile.size();

    // processedBytes = 0;

    // // Если есть сохранённая позиция, восстанавливаем
    // if (m_pausedFiles.contains(inputFilePath)) {
    //     qint64 resumePos = m_pausedFiles[inputFilePath];
    //     if (resumePos < fileSize) {
    //         inputFile.seek(resumePos);
    //         outputFile.seek(resumePos);
    //         processedBytes = resumePos;
    //         emit statusUpdated("Возобновлено с байта " + QString::number(resumePos));
    //     }

    while (true) {
        // Проверяем остановку
        if (m_stopRequested) {
            // Сохраняем позицию для возобновления
            qint64 currentPos = inputFile.pos();
            // m_pausedFiles[inputFilePath] = currentPos;
            ResumeInfo info;
            info.outputFilePath = outputFilePath;
            info.position = currentPos;
            m_pausedFiles[inputFilePath] = info;

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
    
    // Удаляем из списка прерванных
    if (m_pausedFiles.contains(inputFilePath)) {
        m_pausedFiles.remove(inputFilePath);
    }					
    return true;
}

QString WorkerThread::resolveFileName(const QString &relativePath, const QDir &outputPath)
{
    // Полный путь к файлу в выходной директории
    QString outputFilePath = QDir(outputPath).filePath(relativePath);
    QFile outputFile(outputFilePath);

    // Если файл есть в списке прерванных, используем его
    QString inputFilePath = QDir(m_inputPath).filePath(relativePath);
    if (m_pausedFiles.contains(inputFilePath)) {
        // Это файл, который нужно возобновить
        if (outputFile.exists()) {
            // Возвращаем существующий файл
            return outputFilePath;
        } else {
            // Файл был удалён, создаём новый
            // Но помечаем, что это не возобновление
            m_pausedFiles.remove(inputFilePath);
        }
    }

    // Если файл не существует или разрешена перезапись
    if (!outputFile.exists() || m_overwriteExisting) {
        return outputFilePath;
    }

    if (m_autoRename) {
        QFileInfo fileInfo(relativePath);
        QString baseName = fileInfo.baseName();
        QString suffix = fileInfo.suffix();
        QString path = fileInfo.path();
        QString newRelativePath;
        int counter = 2;

        do {
            QString newFileName = baseName + "_" + QString::number(counter);
            if (!suffix.isEmpty()) {
                newFileName += "." + suffix;
            }
            if (path != ".") {
                newRelativePath = path + "/" + newFileName;
            } else {
                newRelativePath = newFileName;
            }
            outputFilePath = QDir(outputPath).filePath(newRelativePath);
            outputFile.setFileName(outputFilePath);
            counter++;
        } while (outputFile.exists());

        emit statusUpdated("Файл переименован в: " + newRelativePath);
        return outputFilePath;
    }

    return outputFilePath;
}