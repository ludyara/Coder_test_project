#include "mainwindow.h"
#include "ui_mainwindow.h"

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
    , ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    connect(ui->pushButton_start, &QPushButton::clicked,
            this, &MainWindow::onProcessButtonClicked);
}

MainWindow::~MainWindow()
{
    delete ui;
}

void MainWindow::onProcessButtonClicked()
{
    // Получаем маску из line edit
    QString mask = ui->lineEdit->text().trimmed();

    // Проверяем, что маска не пустая
    if (mask.isEmpty()) {
        QMessageBox::warning(this, "Ошибка", "Введите маску файлов");
        return;
    }

    // Получаем список файлов по маске в текущей директории
    QDir currentDir = QDir::current();
    QStringList files = currentDir.entryList(QStringList() << mask, QDir::Files);

    // Проверяем, есть ли файлы
    if (files.isEmpty()) {
        QMessageBox::information(this, "Информация",
                                 "Файлы по маске '" + mask + "' не найдены в текущей папке");
        return;
    }

    // Обрабатываем каждый файл
    int processedCount = 0;
    QStringList errors;

    for (const QString &fileName : files) {
        QString filePath = currentDir.filePath(fileName);

        // Открываем файл для чтения
        QFile file(filePath);
        if (!file.open(QIODevice::ReadOnly | QIODevice::Text)) {
            errors << fileName;
            continue;
        }

        // Читаем все содержимое
        QTextStream in(&file);
        QString content = in.readAll();
        file.close();

        // Меняем 1 на 0 и 0 на 1
        QString modifiedContent;
        for (int i = 0; i < content.size(); ++i) {
            QChar ch = content[i];
            if (ch == '1') {
                modifiedContent.append('0');
            } else if (ch == '0') {
                modifiedContent.append('1');
            } else {
                modifiedContent.append(ch);
            }
        }

        // Открываем файл для записи
        if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
            errors << fileName;
            continue;
        }

        // Записываем измененное содержимое
        QTextStream out(&file);
        out << modifiedContent;
        file.close();

        processedCount++;
    }

    // Показываем результат
    QString message = "Обработано файлов: " + QString::number(processedCount);
    if (!errors.isEmpty()) {
        message += "\nОшибки при обработке: " + errors.join(", ");
    }
    QMessageBox::information(this, "Готово", message);
}
