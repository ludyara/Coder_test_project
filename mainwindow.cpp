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

void MainWindow::extracted(QDir &inputDir, QDir &outputDir, QStringList &files,
                           int &processedCount, QStringList &errors) {
    for (const QString &fileName : files) {
        QString inputFilePath = inputDir.filePath(fileName);
        QString outputFilePath = outputDir.filePath(fileName);

        // Открываем входной файл для чтения
        QFile inputFile(inputFilePath);
        if (!inputFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
            errors << fileName;
            continue;
        }

        // Читаем содержимое
        QTextStream in(&inputFile);
        QString content = in.readAll();
        inputFile.close();

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

        // Открываем выходной файл для записи
        QFile outputFile(outputFilePath);
        if (!outputFile.open(QIODevice::WriteOnly | QIODevice::Text)) {
            errors << fileName;
            continue;
        }

        // Записываем изменённое содержимое
        QTextStream out(&outputFile);
        out << modifiedContent;
        outputFile.close();

        processedCount++;
    }
}
void MainWindow::on_pushButton_start_clicked() {
    // 1. Получаем пути из полей ввода
    QString inputPath = ui->lineEdit_2->text().trimmed();
    QString outputPath = ui->lineEdit_3->text().trimmed();
    QString mask = ui->lineEdit->text().trimmed();

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

    // 3. Проверяем существование входной директории
    QDir inputDir(inputPath);
    if (!inputDir.exists()) {
        QMessageBox::warning(this, "Ошибка",
                             "Входная директория не существует:\n" + inputPath);
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

    // 5. Проверяем, что входная и выходная папки не совпадают
    if (QDir::cleanPath(inputPath) == QDir::cleanPath(outputPath)) {
        QMessageBox::warning(
            this, "Ошибка",
            "Входная и выходная директории совпадают.\n"
            "Укажите разные папки, чтобы не перезаписывать исходные файлы.");
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

    // Обрабатываем каждый файл
    int processedCount = 0;
    QStringList errors;

    extracted(inputDir, outputDir, files, processedCount, errors);

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
