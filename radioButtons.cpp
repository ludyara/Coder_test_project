#include "mainwindow.h"
#include "ui_mainwindow.h"

void MainWindow::on_radioButton_3_toggled(bool checked)
{
    if (checked) {
        // Режим реального времени
        isRealtimeMode = true;
        ui->label_8->setVisible(true);
        ui->timeEdit->setVisible(true);
    }
}

void MainWindow::on_radioButton_4_toggled(bool checked)
{
    if (checked) {
        // Разовый режим
        isRealtimeMode = false;
        ui->label_8->setVisible(false);
        ui->timeEdit->setVisible(false);

        // Останавливаем таймеры
        if (realtimeTimer && realtimeTimer->isActive()) {
            realtimeTimer->stop();
        }
        if (progressTimer && progressTimer->isActive()) {
            progressTimer->stop();
        }
    }
}