//=========================================================================
//  Copyright (c) Kitware, Inc.
//  All rights reserved.
//  See LICENSE.txt for details.
//
//  This software is distributed WITHOUT ANY WARRANTY; without even
//  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE.  See the above copyright notice for more information.
//=========================================================================
#include "smtk/extension/qt/testing/cxx/unitQtComponentItem.h"
#include "smtk/extension/qt/qtDescriptivePhraseDelegate.h"
#include "smtk/extension/qt/qtDescriptivePhraseModel.h"

#include "smtk/view/DescriptivePhrase.h"
#include "smtk/view/ResourcePhraseModel.h"
#include "smtk/view/SubphraseGenerator.h"
#include "smtk/view/View.h"
#include "smtk/view/VisibilityContent.h"

#include "smtk/resource/Manager.h"
#include "smtk/resource/testing/cxx/helpers.h"

#include "smtk/operation/Manager.h"
#include "smtk/operation/operators/ReadResource.h"

#include "smtk/session/polygon/Registrar.h"

#include "smtk/attribute/Attribute.h"
#include "smtk/attribute/FileItem.h"
#include "smtk/attribute/IntItem.h"
#include "smtk/attribute/ResourceItem.h"

#include "smtk/model/SessionRef.h"

#include "smtk/common/Registry.h"

#include "smtk/common/testing/cxx/helpers.h"
#include "smtk/model/testing/cxx/helpers.h"

#include "smtk/AutoInit.h"

#include <QApplication>
#include <QComboBox>
#include <QDialog>
#include <QHBoxLayout>
#include <QKeyEvent>
#include <QLabel>
#include <QListView>
#include <QPushButton>
#include <QTimer>
#include <QVBoxLayout>
#include <QWidget>

#include <algorithm>
#include <fstream>
#include <iostream>
#include <string>

#include <stdlib.h>
#include <string.h>

using namespace smtk::extension;

qtEventFilter::qtEventFilter(QObject* parent)
  : QObject(parent)
{
}

qtEventFilter::~qtEventFilter()
{
}

bool qtEventFilter::eventFilter(QObject* src, QEvent* event)
{
  (void)src;
  if (event->type() == QEvent::KeyPress)
  {
    auto keyEvent = static_cast<QKeyEvent*>(event);
    int kk = keyEvent->key();
    switch (kk)
    {
      case Qt::Key_Escape:
      case Qt::Key_Cancel:
        emit popDown();
        return true;
      case Qt::Key_Return:
      case Qt::Key_Enter:
      case Qt::Key_Space:
        emit toggleItem();
        return true;
      case Qt::Key_R:
        if (keyEvent->modifiers() & (Qt::MetaModifier | Qt::AltModifier | Qt::ControlModifier))
        {
        }
        else
        {
          break;
        }
      // fall through.
      case Qt::Key_Clear:
        emit reset();
        return true;
      case Qt::Key_L:
        if (keyEvent->modifiers() & (Qt::MetaModifier | Qt::AltModifier | Qt::ControlModifier))
        {
          emit toggleLink();
          return true;
        }
        break;
      default:
        break;
    }
    /*
    if (kk != Qt::Key_Up && kk != Qt::Key_Down && kk != Qt::Key_Right && kk != Qt::Key_Left)
    {
      qDebug("Ate key press %d", keyEvent->key());
      return true;
    }
    */
  }
  return false;
}

static std::function<void()> updater = nullptr;

static std::vector<char*> dataArgs;
static int userNumRequired = -1;
static int userMaxAllowed = -1;

int unitQtComponentItem(int argc, char* argv[])
{
  bool exitImmediately = false;
  int shift = 0;
  for (int ii = 1; ii < argc; ++ii)
  {
    if (!strcmp(argv[ii], "--exit"))
    {
      exitImmediately = true;
      ++shift;
    }
    else if (!strcmp(argv[ii], "--num-required") && ii + 1 < argc)
    {
      userNumRequired = atoi(argv[ii + 1]);
      shift += 2;
    }
    else if (!strcmp(argv[ii], "--max-allowed") && ii + 1 < argc)
    {
      userMaxAllowed = atoi(argv[ii + 1]);
      shift += 2;
    }
    if (shift > 0)
    {
      argv[ii - shift] = argv[ii];
    }
  }
  argc -= shift;
  if (argc < 2)
  {
    std::string testFile;
    testFile = SMTK_DATA_DIR;
    testFile += "/model/2d/smtk/epic-trex-drummer.smtk";
    dataArgs.push_back(argv[0]);
    dataArgs.push_back(strdup(testFile.c_str()));
    dataArgs.push_back(nullptr);
    argc = 2;
    argv = &dataArgs[0];
  }
  QApplication app(argc, argv);

  auto rsrcMgr = smtk::resource::Manager::create();
  auto operMgr = smtk::operation::Manager::create();

  auto registry = smtk::common::Registry<smtk::session::polygon::Registrar, smtk::resource::Manager,
    smtk::operation::Manager>(rsrcMgr, operMgr);

  // Constructing the PhraseModel with a View properly initializes the SubphraseGenerator
  // to point back to the model (thus ensuring subphrases are decorated). This is required
  // since we need to decorate phrases to show+edit "visibility" as set membership:
  auto view = smtk::view::View::New("ComponentItem", "stuff");
  auto phraseModel = smtk::view::ResourcePhraseModel::create(view);
  phraseModel->root()->findDelegate()->setModel(phraseModel);

  std::map<smtk::resource::ComponentPtr, int>
    m_visibleThings; // Our internal state of what is selected.

  auto main = new QWidget;
  auto qvbl = new QVBoxLayout(main);
  auto qhbl = new QHBoxLayout();
  auto labl = new QLabel(main);
  auto plbl = new QLabel(main);
  auto pbtn = new QPushButton("…", main);
  auto lbtn = new QPushButton(main);
  auto dlog = new QDialog(pbtn);
  auto dlbl = new QLabel(dlog);
  plbl->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
  dlbl->setAlignment(Qt::AlignLeft | Qt::AlignVCenter);
  dlog->setWindowFlags(Qt::FramelessWindowHint | Qt::Popup);
  updater = [&plbl, &dlbl, &m_visibleThings]() {
    static const int numRequired = userNumRequired >= 0 ? userNumRequired : 3;
    static const int maxAllowed =
      (userMaxAllowed > 0 && userMaxAllowed >= numRequired) ? userMaxAllowed : 4;
    std::ostringstream label;
    int numSel = 0;
    for (auto entry : m_visibleThings)
    {
      if (entry.second)
      {
        ++numSel;
      }
    }
    bool ok = true;
    if (numRequired < 2 && maxAllowed < 2)
    {
      auto ment = std::dynamic_pointer_cast<smtk::model::Entity>(m_visibleThings.begin()->first);
      label << (numSel == 1
          ? (ment ? ment->referenceAs<smtk::model::EntityRef>().name() : "item name")
          : (numSel > 0 ? "too many" : "(none)"));
      ok = numSel >= numRequired && numSel <= maxAllowed;
    }
    else
    {
      label << numSel;
      if (numRequired > 0)
      {
        label << " of ";
        if (numRequired == maxAllowed)
        { // Exactly N values are allowed and required.
          label << numRequired;
        }
        else if (maxAllowed > 0)
        { // There is a minimum required, but a limited additional number are acceptable
          label << numRequired << "—" << maxAllowed;
        }
        else
        { // Any number are allowed, but there is a minimum.
          label << numRequired << "+";
        }
        ok &= (numSel >= numRequired);
      }
      else
      { // no values are required, but there may be a cap on the maximum number.
        if (maxAllowed > 0)
        {
          label << " of 0–" << maxAllowed;
        }
      }
    }
    ok &= (maxAllowed <= 0 || numSel <= maxAllowed);
    plbl->setText(label.str().c_str());
    plbl->setAutoFillBackground(ok ? false : true);
    QPalette pal = plbl->palette();
    pal.setColor(QPalette::Background, QColor(QRgb(ok ? 0x00ff00 : 0xff7777)));
    plbl->setPalette(pal);
    plbl->update();

    dlbl->setText(label.str().c_str());
    dlbl->setAutoFillBackground(ok ? false : true);
    QPalette dpal = dlbl->palette();
    dpal.setColor(QPalette::Background, QColor(QRgb(ok ? 0x00ff00 : 0xff7777)));
    dlbl->setPalette(dpal);
    dlbl->update();
    // pbtn->setStyleSheet(ok ? origStyle : invalidStyle);
    if (!ok)
    {
      //QPalette pal = pbtn->palette();
      //pal.setColor(QPalette::Button, QColor(Qt::red));
      //pbtn->setAutoFillBackground(true);
      //pbtn->setPalette(pal);
      //pbtn->update();
    }
    else
    {
      //pbtn->setAutoFillBackground(false);
      //pbtn->update();
    }
  };

  phraseModel->addSource(rsrcMgr, operMgr);
  phraseModel->setDecorator([&m_visibleThings](smtk::view::DescriptivePhrasePtr phr) {
    smtk::view::VisibilityContent::decoratePhrase(
      phr, [&m_visibleThings](smtk::view::VisibilityContent::Query qq, int val,
             smtk::view::ConstPhraseContentPtr data) {
        smtk::model::EntityPtr ent =
          data ? std::dynamic_pointer_cast<smtk::model::Entity>(data->relatedComponent()) : nullptr;
        smtk::model::ResourcePtr mResource = ent
          ? ent->modelResource()
          : (data ? std::dynamic_pointer_cast<smtk::model::Resource>(data->relatedResource())
                  : nullptr);

        switch (qq)
        {
          case smtk::view::VisibilityContent::DISPLAYABLE:
            return (ent || (!ent && mResource)) ? 1 : 0;
          case smtk::view::VisibilityContent::EDITABLE:
            return (ent || (!ent && mResource)) ? 1 : 0;
          case smtk::view::VisibilityContent::GET_VALUE:
            if (ent)
            {
              auto valIt = m_visibleThings.find(ent);
              if (valIt != m_visibleThings.end())
              {
                return valIt->second;
              }
              return 0; // visibility is assumed if there is no entry.
            }
            return 0; // visibility is false if the component is not a model entity or NULL.
          case smtk::view::VisibilityContent::SET_VALUE:
            if (ent)
            {
              m_visibleThings[ent] = val ? 1 : 0;
              updater();
              return 1;
            }
        }
        return 0;
      });
    return 0;
  });

  auto oper = operMgr->create<smtk::operation::ReadResource>();
  if (!oper)
  {
    std::cout << "No read operator\n";
    return 1;
  }

  oper->parameters()->findFile("filename")->setValue(argv[1]);
  auto result = oper->operate();
  if (result->findInt("outcome")->value() !=
    static_cast<int>(smtk::operation::Operation::Outcome::SUCCEEDED))
  {
    std::cout << "Read operator failed\n";
    return 2;
  }

  auto rsrc = result->findResource("resource")->value(0);
  auto modelRsrc = std::dynamic_pointer_cast<smtk::model::Resource>(rsrc);
  if (!modelRsrc)
  {
    std::cout << "Read operator succeeded but had empty output\n";
    return 4;
  }
  auto qmodel = new qtDescriptivePhraseModel;

  labl->setText("edges");
  pbtn->setAutoDefault(true);
  pbtn->setDefault(true);
  lbtn->setText("link");
  lbtn->setCheckable(true);

  main->setLayout(qvbl);
  qvbl->addLayout(qhbl);
  qhbl->addWidget(labl);
  //qhbl->addStretch();
  qhbl->addWidget(plbl);
  qhbl->addWidget(pbtn);
  qhbl->addWidget(lbtn);
  main->show();

  // Initialize the button
  updater();

  QObject::connect(pbtn, SIGNAL(clicked()), dlog, SLOT(exec()));
  // auto combo = new QComboBox(dlog);
  auto layout = new QVBoxLayout(dlog);
  // layout->addWidget(combo);
  qmodel->setPhraseModel(phraseModel);
  // combo->setModel(qmodel);
  QModelIndex comboRoot =
    qmodel->index(4, 0, qmodel->index(0, 0, qmodel->index(0, 0, QModelIndex())));
  std::cout << " root is " << qmodel->data(comboRoot).toString().toStdString() << "\n";
  // QListView* listView = new QListView(combo);
  QListView* listView = new QListView(dlog);
  listView->setModel(qmodel);
  layout->addWidget(listView);
  QObject::connect(pbtn, SIGNAL(clicked()), listView, SLOT(setFocus()));

  auto donebox = new QHBoxLayout;
  auto donebtn = new QPushButton("Done");
  donebox->addWidget(dlbl);
  //donebox->addStretch();
  donebox->addWidget(donebtn);
  donebtn->setAutoDefault(true);
  donebtn->setDefault(true);
  layout->addLayout(donebox);
  QObject::connect(donebtn, SIGNAL(clicked()), dlog, SLOT(accept()));

  auto delegate = new smtk::extension::qtDescriptivePhraseDelegate;
  delegate->setTextVerticalPad(6);
  delegate->setTitleFontWeight(1);
  delegate->setDrawSubtitle(false);
  delegate->setVisibilityMode(true);
  auto ef = new qtEventFilter(dlog);
  listView->installEventFilter(ef);
  QObject::connect(ef, &qtEventFilter::reset, [&m_visibleThings, &phraseModel]() {
    phraseModel->root()->visitChildren(
      [&phraseModel, &m_visibleThings](
        smtk::view::DescriptivePhrasePtr cphr, std::vector<int>&) -> int {
        std::map<smtk::resource::ComponentPtr, int>::iterator it;
        if (cphr->relatedComponent() &&
          (it = m_visibleThings.find(cphr->relatedComponent())) != m_visibleThings.end())
        {
          m_visibleThings.erase(it);
          phraseModel->triggerDataChangedFor(cphr->relatedComponent());
        }
        return 0; // Do not terminate early.
      });
    m_visibleThings.clear(); // just in case there were things not in any phrase.
    updater();
  });
  QObject::connect(ef, &qtEventFilter::toggleItem, [&listView, &qmodel]() {
    auto cphr = qmodel->getItem(listView->currentIndex());
    if (cphr)
    {
      cphr->setRelatedVisibility(!cphr->relatedVisibility());
      updater();
    }
  });
  QObject::connect(
    ef, &qtEventFilter::toggleLink, []() { std::cout << "copy to/from app selection\n"; });
  QObject::connect(ef, &qtEventFilter::popDown, [&dlog]() { dlog->hide(); });

  listView->setItemDelegate(delegate);
  // combo->setView(listView);
  // combo->setRootModelIndex(comboRoot);
  listView->setRootIndex(comboRoot);
  QObject::connect(delegate, SIGNAL(requestVisibilityChange(const QModelIndex&)), qmodel,
    SLOT(toggleVisibility(const QModelIndex&)));

  //dlog->show();

  if (exitImmediately)
  {
    QTimer::singleShot(1, qApp, SLOT(closeAllWindows()));
  }

  return app.exec();
}
