#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QDir>
#include <QFile>
#include <QTextStream>
#include <QMessageBox>
#include <QFileDialog>

QT_BEGIN_NAMESPACE
namespace Ui {
class MainWindow;
}
QT_END_NAMESPACE

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;


private slots:
    void extracted(QDir &inputDir, QDir &outputDir, QStringList &files,
                   int &processedCount, QStringList &errors);
    void on_pushButton_start_clicked();
    void on_pushButton_input_clicked();
    void on_pushButton_output_clicked();
    // bool validatePaths(QString &errorMessage);


private:
    Ui::MainWindow *ui;
};
#endif // MAINWINDOW_H
