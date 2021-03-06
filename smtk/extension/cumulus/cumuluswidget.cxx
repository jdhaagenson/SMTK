//=========================================================================
//  Copyright (c) Kitware, Inc.
//  All rights reserved.
//  See LICENSE.txt for details.
//
//  This software is distributed WITHOUT ANY WARRANTY; without even
//  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE.  See the above copyright notice for more information.
//=========================================================================

#include "cumuluswidget.h"
#include "cumulusproxy.h"
#include "job.h"
#include "jobtablemodel.h"
#include "ui_cumuluswidget.h"

#include <QDesktopWidget>
#include <QList>
#include <QMessageBox>
#include <QNetworkReply>
#include <QTimer>

namespace
{
static constexpr int timer_period_msec = 10 * 1000;
}

namespace cumulus
{

CumulusWidget::CumulusWidget(QWidget* parentObject)
  : QWidget(parentObject)
  , m_ui(new Ui::CumulusWidget)
  , m_jobTableModel(new JobTableModel(this))
  , m_cumulusProxy(new CumulusProxy(this))
  , m_timer(NULL)
  , m_loginDialog(this)
{
  m_ui->setupUi(this);

  this->createJobTable();

  connect(&m_loginDialog, SIGNAL(entered(QString, QString)), m_cumulusProxy,
    SLOT(authenticateNewt(QString, QString)));
  connect(m_cumulusProxy, SIGNAL(authenticationFinished()), this, SLOT(startJobFetchLoop()));
  connect(m_cumulusProxy, SIGNAL(jobsUpdated(QList<Job>)), m_jobTableModel,
    SLOT(jobsUpdated(QList<Job>)));
  connect(m_cumulusProxy, SIGNAL(error(QString, QNetworkReply*)), this,
    SLOT(handleError(QString, QNetworkReply*)));
  connect(m_cumulusProxy, SIGNAL(newtAuthenticationError(QString)), this,
    SLOT(displayAuthError(QString)));
  connect(m_cumulusProxy, SIGNAL(info(QString)), this, SIGNAL(info(QString)));
  connect(m_cumulusProxy, SIGNAL(jobDownloaded(cumulus::Job, const QString&)), this,
    SLOT(handleDownloadResult(cumulus::Job, const QString&)));
  connect(
    m_jobTableModel, &JobTableModel::finishTimeChanged, m_cumulusProxy, &CumulusProxy::patchJobs);

  // Instantiate polling timer (but don't start)
  m_timer = new QTimer(this);
  m_timer->setInterval(timer_period_msec);
  connect(m_timer, SIGNAL(timeout()), m_cumulusProxy, SLOT(fetchJobs()));
  m_ui->jobTableWidget->setEnabled(false);
}

CumulusWidget::~CumulusWidget()
{
  delete m_ui;
}

void CumulusWidget::girderUrl(const QString& url)
{
  m_cumulusProxy->girderUrl(url);
}

bool CumulusWidget::isGirderRunning() const
{
  return m_cumulusProxy->isGirderRunning();
}

void CumulusWidget::showLoginDialog()
{
  m_loginDialog.show();
}

void CumulusWidget::addContextMenuAction(const QString& status, QAction* action)
{
  m_ui->jobTableWidget->addContextMenuAction(status, action);
}

void CumulusWidget::createJobTable()
{
  m_ui->jobTableWidget->setModel(m_jobTableModel);
  m_ui->jobTableWidget->setCumulusProxy(m_cumulusProxy);
}

void CumulusWidget::startJobFetchLoop()
{
  m_ui->jobTableWidget->setEnabled(true);
  m_cumulusProxy->fetchJobs();
  m_timer->start();
}

void CumulusWidget::displayAuthError(const QString& msg)
{
  m_loginDialog.setErrorMessage(msg);
  m_loginDialog.show();
}

void CumulusWidget::handleError(const QString& msg, QNetworkReply* networkReply)
{
  if (networkReply)
  {
    int statusCode = networkReply->attribute(QNetworkRequest::HttpStatusCodeAttribute).value<int>();

    // Forbidden, ask the user to authenticate again
    if (statusCode == 403)
    {
      m_timer->stop();
      m_ui->jobTableWidget->setEnabled(false);
      m_loginDialog.show();
    }
    else if (networkReply->error() == QNetworkReply::ConnectionRefusedError)
    {
      m_timer->stop();
      m_ui->jobTableWidget->setEnabled(false);
      QMessageBox::critical(NULL, QObject::tr("Connection refused"),
        QObject::tr("Unable to connect to server at %1:%2, please ensure server is running.")
          .arg(networkReply->url().host())
          .arg(networkReply->url().port()));
      m_loginDialog.show();
    }
    else
    {
      QMessageBox::critical(this, "", msg, QMessageBox::Ok);
    }
  }
  else
  {
    QMessageBox::critical(this, "", msg, QMessageBox::Ok);
  }
}

void CumulusWidget::handleDownloadResult(const cumulus::Job& job, const QString& path)
{
  (void)job;
  emit this->resultDownloaded(path);
}

} // end namespace
