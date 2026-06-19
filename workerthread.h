#ifndef WORKERTHREAD_H
#define WORKERTHREAD_H

#include <QObject>
#include <QThread>
#include <QDir>
#include <QFile>
#include <QByteArray>
#include <QMap>
#include <QRegularExpression>
#include <QMessageBox>

class WorkerThread : public QObject
{
    Q_OBJECT

public:
    explicit WorkerThread(QObject *parent = nullptr);

    // Настройка параметров обработки
    void setParameters(const QString &inputPath,
                       const QString &outputPath,
                       const QString &mask,
                       const QByteArray &keyBytes,
                       bool deleteSource,
                       bool overwriteExisting,
                       bool autoRename,
                       const QMap<QString, qint64> &pausedFiles);

public slots:
    void process();  // Основная функция обработки
    void stop();     // Остановка обработки

signals:
    void progressUpdated(int current, int total);      // Прогресс (количество файлов)
    void statusUpdated(const QString &status);         // Статус (имя файла, действие)
    void fileProgressUpdated(int percent);              // Прогресс обработки текущего файла
    void finished(int processedCount, int errorCount,  // Завершено
                  const QStringList &errors);
    void errorOccurred(const QString &error);          // Ошибка

private:
    QString m_inputPath;
    QString m_outputPath;
    QString m_mask;
    QByteArray m_keyBytes;
    bool m_deleteSource;
    bool m_overwriteExisting;
    bool m_autoRename;
    QMap<QString, qint64> m_pausedFiles;

    bool m_stopRequested;

    // Вспомогательные методы
    // QByteArray hexStringToByteArray(QString &hexString);
    bool processFile(const QString &inputFilePath,
                     const QString &outputFilePath,
                     qint64 &processedBytes);
    QString resolveFileName(const QString &fileName, const QDir &outputDir);
};

#endif // WORKERTHREAD_H