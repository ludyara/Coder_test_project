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

struct ResumeInfo
{
    QString outputFilePath;
    qint64 position;
};

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
                       const QMap<QString, ResumeInfo> &pausedFiles);

public slots:
    void process();  // Основная функция обработки
    void stop();     // Остановка обработки
    QMap<QString, ResumeInfo> getPausedFiles() const { return m_pausedFiles; }

signals:
    void progressUpdated(int current, int total, int percent);      // Прогресс (количество файлов и определённый файл)
    void statusUpdated(const QString &status);         // Статус (имя файла, действие)
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
    QMap<QString, ResumeInfo> m_pausedFiles;
    QMap<QString, bool> m_isNewFile; // флаг создан ли файл

    bool m_stopRequested;

    // Вспомогательные методы
    // QByteArray hexStringToByteArray(QString &hexString);
    bool processFile(const QString &inputFilePath,
                     const QString &outputFilePath,
                     qint64 &processedBytes,
                     int &percent,
                     int &current,
                     int &total);
    QString resolveFileName(const QString &fileName, const QDir &outputDir);
};

#endif // WORKERTHREAD_H