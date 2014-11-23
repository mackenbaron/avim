#pragma once

#include <QWidget>
#include <QThread>
#include "ui_avim.h"
#include "ini.h"
#include "login_dialog.h"

#include <boost/asio.hpp>

#include <avjackif.hpp>
#include <avproto.hpp>

namespace avui {

class recvThread : public QThread
{
	Q_OBJECT
public:
	recvThread(boost::asio::io_service& ios, avkernel& avk, std::string keyfile, std::string certfile) :
		QThread(), io_service_(ios), avcore_(avk), m_keyfile(keyfile), m_certfile(certfile){}
	~recvThread();
	void run();
	void recv_msg(boost::asio::yield_context);
private:
	boost::asio::io_service& io_service_;
	avkernel& avcore_;
	std::string m_keyfile;
	std::string m_certfile;
signals:
	void recvReady(const QString &, const QString &);
};

class avim : public QWidget {
Q_OBJECT

public:
  avim(QWidget *parent = 0);

  ~avim();

  QString getMessage();

  bool init(const std::string &, const std::string &);

  void set_avim_key(const std::string &v_) {
    cur_avim_key = v_;
  }

  void set_avim_cert(const std::string &v_) {
    cur_avim_cert = v_;
  }

protected:
  void closeEvent(QCloseEvent *);

private:
  Ui::avimClass ui;
  boost::asio::io_service io_service_;
  avkernel avcore_;
  //recvThread rv_thread_;
  std::string current_chat_target;
  std::string cur_avim_key;
  std::string cur_avim_cert;

private slots:

  void on_sendButton_clicked();

//      void on_exitButton_clicked();

  void on_chatTarget_clicked();

  void recvHandle(const QString &, const QString &);
};

} // namespace avim