#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QtSerialPort/QSerialPort>
#include <QtSerialPort/QSerialPortInfo>
#include <QTextStream>
#include <QMessageBox>

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    QSerialPort *m_serial_port;     // 串口端口对象指针
    QString      m_ser_port_name;   // 用于存入临时串口名称
    QByteArray   m_ser_rx_data;     // 用于串口接收数据
    QByteArray   m_ser_tx_data;     // 用于串口发送数据
    QTextStream  m_ser_std_utput;   // 用于输出串口相关提示信息




public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();

    void update_ser_port_list(void);

private:
    Ui::MainWindow *ui;

private slots:
    void read_ser_port_data(void);
    void write_data_via_ser_port(void);
    void update_dis_message(void);
    void serial_port_switch(void);
};

#endif // MAINWINDOW_H
