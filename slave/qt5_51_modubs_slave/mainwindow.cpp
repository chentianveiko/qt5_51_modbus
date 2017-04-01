#include "mainwindow.h"
#include "ui_mainwindow.h"

/*************************************************************************************
 * @brief    构造函数
 * @version  V0.0.0
 */
MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);

    m_serial_port = new QSerialPort(this);
    m_serial_port->setBaudRate(QSerialPort::Baud115200);
    m_serial_port->setStopBits(QSerialPort::OneStop);
    m_serial_port->setDataBits(QSerialPort::Data8);
    m_serial_port->setParity(QSerialPort::NoParity);
    m_serial_port->setFlowControl(QSerialPort::NoFlowControl);
    update_ser_port_list();

    MB_initSlave(0x01, Modbus_data_send);

    connect(m_serial_port,SIGNAL(readyRead()),this,SLOT(read_ser_port_data()));
    connect(ui->textBrowser_serial_data_dis,SIGNAL(textChanged()),this,SLOT(update_dis_message()));
    connect(ui->pushButton_port_sw,SIGNAL(clicked(bool)),this,SLOT(serial_port_switch()));
    connect(ui->pushButton_send_data,SIGNAL(clicked(bool)),this,SLOT(write_data_via_ser_port()));
}
/*************************************************************************************
 * @brief    析构函数
 * @version  V0.0.0
 */
MainWindow::~MainWindow()
{
    delete ui;
}
/*************************************************************************************
 * @brief    调用串口进行数据发送
 * @version  V0.0.0
 */
void MainWindow::Modbus_data_send(MB_BYTE_T *data, MB_BYTE_T length){
    MB_BYTE_T i;

    m_ser_tx_data.clear();
    for(i=0;i<length;i++){
        m_ser_tx_data.append(data[i]);
    }

    if(m_serial_port->isOpen() == true){

        qint64 bytesWritten = m_serial_port->write(m_ser_tx_data);

        if (bytesWritten == -1) {
            m_ser_std_utput << QObject::tr("Failed to write the data to port %1, error: %2").arg(m_serial_port->portName()).arg(m_serial_port->errorString()) << endl;
            QCoreApplication::exit(1);
        } else if (bytesWritten != m_ser_tx_data.size()) {
            m_ser_std_utput << QObject::tr("Failed to write all the data to port %1, error: %2").arg(m_serial_port->portName()).arg(m_serial_port->errorString()) << endl;
            QCoreApplication::exit(1);
        }
    }
}

/*************************************************************************************
 * @brief    更新可用的串口列表
 * @version  V0.0.0
 */
void MainWindow::update_ser_port_list(){
    ui->comboBox_port->clear();
    if(m_serial_port->isOpen() == true){
        m_serial_port->close();
        ui->pushButton_port_sw->setText(tr("打开串口"));
    }

    // 自动检测串口号
    for(int i=1;i<50;i++){
        m_serial_port->setPortName(tr("\\\\.\\COM%1").arg(i));

        if(m_serial_port->open(QIODevice::ReadWrite))
        {
            m_serial_port->close();
            ui->comboBox_port->addItem(tr("COM%1").arg(i));
        }
    }
}
/*************************************************************************************
 * @brief    发送串口数据
 * @version  V0.0.0
 */
void MainWindow::write_data_via_ser_port(void){
    if(m_serial_port->isOpen() == true){
        m_ser_tx_data.clear();
        m_ser_tx_data = ui->textBrowser_serial_data_send->toPlainText().toLocal8Bit();

        qint64 bytesWritten = m_serial_port->write(m_ser_tx_data);

        if (bytesWritten == -1) {
            m_ser_std_utput << QObject::tr("Failed to write the data to port %1, error: %2").arg(m_serial_port->portName()).arg(m_serial_port->errorString()) << endl;
            QCoreApplication::exit(1);
        } else if (bytesWritten != m_ser_tx_data.size()) {
            m_ser_std_utput << QObject::tr("Failed to write all the data to port %1, error: %2").arg(m_serial_port->portName()).arg(m_serial_port->errorString()) << endl;
            QCoreApplication::exit(1);
        }
    }
}
/*************************************************************************************
 * @brief    打开/关闭串口
 * @version  V0.0.0
 */
void MainWindow::serial_port_switch(void){
     QString port_name = "\\\\.\\" + ui->comboBox_port->currentText().toLower();

     if(ui->pushButton_port_sw->isChecked() == true){
         m_serial_port->setPortName(port_name);
         m_serial_port->open(QIODevice::ReadWrite);
         m_serial_port->setBaudRate(QSerialPort::Baud115200,QSerialPort::AllDirections);

         if(m_serial_port->isOpen())
         {
             ui->pushButton_port_sw->setText(tr("关闭串口"));
         }
         else
         {
             QMessageBox msgBox;
             msgBox.setButtonText(1,tr("关闭"));
             msgBox.setText(ui->comboBox_port->currentText()+tr("<font color=black>端口打开失败!<br>请确认后重试!</font>"));
             msgBox.setIcon(QMessageBox::Warning);
             msgBox.exec();
             ui->pushButton_port_sw->setChecked(false);
         }
     }else{
        m_serial_port->close();
        ui->pushButton_port_sw->setText(tr("打开串口"));
     }
}
/*************************************************************************************
 * @brief    接收串口数据
 * @version  V0.0.0
 */
void MainWindow::read_ser_port_data(){
    m_ser_rx_data.append(m_serial_port->readAll());
    ui->textBrowser_serial_data_dis->insertPlainText(m_ser_rx_data);
    m_ser_rx_data.clear();
}
/*************************************************************************************
 * @brief    更新显示
 * @version  V0.0.0
 */
void MainWindow::update_dis_message(){
    ui->textBrowser_serial_data_dis->moveCursor(QTextCursor::End);
}
