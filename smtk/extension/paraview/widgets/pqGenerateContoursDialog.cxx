//=========================================================================
//  Copyright (c) Kitware, Inc.
//  All rights reserved.
//  See LICENSE.txt for details.
//
//  This software is distributed WITHOUT ANY WARRANTY; without even
//  the implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
//  PURPOSE.  See the above copyright notice for more information.
//=========================================================================

#include "pqGenerateContoursDialog.h"
#include "ui_qtGenerateContoursDialog.h"

#include "pqApplicationCore.h"
#include "pqObjectBuilder.h"
#include "pqOutputPort.h"
#include "pqDataRepresentation.h"
#include "pqPipelineSource.h"
#include "pqProgressManager.h"
#include "pqRenderView.h"
#include "pqSMAdaptor.h"
#include "pqServer.h"
#include "pqSetName.h"

#include "vtkDataObject.h"
#include "vtkPVArrayInformation.h"
#include "vtkPVDataInformation.h"
#include "vtkPVDataSetAttributesInformation.h"
#include "vtkSMSourceProxy.h"
#include "vtkSMPropertyHelper.h"
#include "vtkSMRepresentationProxy.h"
#include "vtkSMProxyManager.h"
#include "vtkSMPVRepresentationProxy.h"

#include <QDoubleValidator>
#include <QFileInfo>
#include <QIntValidator>
#include <QMessageBox>
#include <QVBoxLayout>
#include <QProgressDialog>

//-----------------------------------------------------------------------------
InternalDoubleValidator::InternalDoubleValidator(QObject * parent)
  :QDoubleValidator(parent)
{

}

//-----------------------------------------------------------------------------
void InternalDoubleValidator::fixup(QString &input) const
{
  if ( input.length() == 0 )
    {
    return;
    }

  double v = input.toDouble();
  double tol = 0.0001;
  if (v < this->bottom())
    {
    input = QString::number(this->bottom()+tol);
    }
  else if (v > this->top())
    {
    input = QString::number(this->top()-tol);
    }
}

inline bool internal_COLOR_REP_BY_ARRAY(
    vtkSMProxy* reproxy, const char* arrayname, int attribute_type,
    bool rescale = true )
{
  bool res = vtkSMPVRepresentationProxy::SetScalarColoring(
    reproxy, arrayname, attribute_type);
  if(rescale && res && vtkSMPVRepresentationProxy::GetUsingScalarColoring(reproxy))
    {
    vtkSMPropertyHelper inputHelper(reproxy->GetProperty("Input"));
    vtkSMSourceProxy* inputProxy =
      vtkSMSourceProxy::SafeDownCast(inputHelper.GetAsProxy());
    int port = inputHelper.GetOutputPort();
    if (inputProxy)
      {
      vtkPVDataInformation* dataInfo = inputProxy->GetDataInformation(port);
      vtkPVArrayInformation* info = dataInfo->GetArrayInformation(
        arrayname, attribute_type);
      vtkSMPVRepresentationProxy* pvRepProxy =
        vtkSMPVRepresentationProxy::SafeDownCast(reproxy);
      if (!info && pvRepProxy)
        {
        vtkPVDataInformation* representedDataInfo =
          pvRepProxy->GetRepresentedDataInformation();
        info = representedDataInfo->GetArrayInformation(arrayname, attribute_type);
        }
      // make sure we have the requested array before calling rescale TF
      if(info)
        {
        res = vtkSMPVRepresentationProxy::RescaleTransferFunctionToDataRange(
          reproxy, arrayname, attribute_type);
        }
      }
    }
  return res;
}

//-----------------------------------------------------------------------------
pqGenerateContoursDialog::pqGenerateContoursDialog(
                                          pqPipelineSource* imagesource,
                                          QWidget *parent,
                                          Qt::WindowFlags flags)
  : QDialog(parent, flags), ImageSource(imagesource)
{
  this->MainDialog = new QDialog;
  this->InternalWidget = new Ui::qtGenerateContoursDialog;
  this->InternalWidget->setupUi(this->MainDialog);
  QObject::connect(this->InternalWidget->generateContoursButton, SIGNAL(clicked()),
    this, SLOT(generateContours()));
  QObject::connect(this->InternalWidget->createContourNodesButton, SIGNAL(clicked()),
    this, SLOT(onAccecptContours()));
  QObject::connect(this->InternalWidget->cancelButton, SIGNAL(clicked()),
    this, SLOT(onCancel()));
  QObject::connect(this->InternalWidget->imageOpacitySlider, SIGNAL(valueChanged(int)),
    this, SLOT(onOpacityChanged(int)));

  this->InternalWidget->createContourNodesButton->setEnabled(false);
  this->InternalWidget->generateContoursBox->setEnabled(false);

  pqProgressManager* progress_manager =
    pqApplicationCore::instance()->getProgressManager();
  QObject::connect(progress_manager, SIGNAL(progress(const QString&, int)),
                   this, SLOT(updateProgress(const QString&, int)));
  this->Progress = new QProgressDialog(this->MainDialog);
  this->Progress->setWindowTitle(QString("Loading Image"));
  this->Progress->setMaximum(0.0);
  this->Progress->setMinimum(0.0);

  this->Progress->show();

  // validator for contour value
  this->ContourValidator = new InternalDoubleValidator( parent );
  this->ContourValidator->setNotation( QDoubleValidator::StandardNotation );
  this->ContourValidator->setDecimals(5);
  this->InternalWidget->contourValue->setValidator( this->ContourValidator );

  QDoubleValidator *minLineLengthValidator = new InternalDoubleValidator( parent );
  minLineLengthValidator->setBottom(0);
  this->InternalWidget->minimumLineLength->setValidator( minLineLengthValidator );

  this->InternalWidget->relativeLineLengthCheckbox->setChecked(true);
  this->InternalWidget->minimumLineLength->setText("5");

  // re-set after loading the image
  this->InternalWidget->contourValue->setText("0");

  if(imagesource)
    {
    pqObjectBuilder* builder = pqApplicationCore::instance()->getObjectBuilder();
    // setup the render widget
    this->RenderView = qobject_cast<pqRenderView*>(
      builder->createView(pqRenderView::renderViewType(),
      imagesource->getServer()));

    QVBoxLayout* vboxlayout = new QVBoxLayout(this->InternalWidget->renderFrame);
    vboxlayout->setMargin(0);
    vboxlayout->addWidget(this->RenderView->widget());

    this->ContourRepresentation = 0;
    this->ContourSource = 0;
    this->CleanPolyLines = 0;

    this->ContourValue = VTK_FLOAT_MAX;
    this->MinimumLineLength = -1;
    this->UseRelativeLineLength = false;
    QObject::connect(this->InternalWidget->contourValue, SIGNAL(textChanged(const QString&)),
      this, SLOT(updateContourButtonStatus()));
    QObject::connect(this->InternalWidget->minimumLineLength, SIGNAL(textChanged(const QString&)),
      this, SLOT(updateContourButtonStatus()));
    QObject::connect(this->InternalWidget->relativeLineLengthCheckbox, SIGNAL(stateChanged(int)),
      this, SLOT(updateContourButtonStatus()));

    // setup Parallel projection and 2D manipulation
    pqSMAdaptor::setElementProperty(
      this->RenderView->getProxy()->GetProperty("CameraParallelProjection"), 1);
    // paraview default 2d manipulators
    const int TwoDManipulators[9] = {1, 3, 2, 2, 2, 6, 3, 1, 4};
    vtkSMProxy* viewproxy = this->RenderView->getProxy();
    vtkSMPropertyHelper(
      viewproxy->GetProperty("Camera2DManipulators")).Set(TwoDManipulators, 9);
    viewproxy->UpdateVTKObjects();

    pqDataRepresentation* imagerep = builder->createDataRepresentation(
      imagesource->getOutputPort(0), this->RenderView);
    if(imagerep)
      {
      internal_COLOR_REP_BY_ARRAY(
          imagerep->getProxy(), "Elevation", vtkDataObject::POINT);

      vtkSMProxy* lut = builder->createProxy("lookup_tables", "PVLookupTable",
                                           imagerep->getServer(), "transfer_functions");

      QList<QVariant> values;
      values << -5000.0  << 0.0 << 0.0 << 0
       << -1000.0  << 0.0 << 0.0 << 1.0
       <<  -100.0  << 0.129412 << 0.345098 << 0.996078
       <<   -50.0  << 0.0 << 0.501961 << 1.0
       <<   -10.0  << 0.356863 << 0.678431 << 1.0
       <<    -0.0  << 0.666667 << 1.0 << 1.0
       <<     0.01 << 0.0 << 0.250998 << 0.0
       <<    10.0 << 0.301961 << 0.482353 << 0.0
       <<    25.0 << 0.501961 << 1.0 << 0.501961
       <<   500.0 << 0.188224 << 1.0 << 0.705882
       <<  1000.0 << 1.0 << 1.0 << 0.0
       <<  2500.0 << 0.505882 << 0.211765 << 0.0
       <<  3200.0 << 0.752941 << 0.752941 << 0.752941
       <<  6000.0 << 1.0 << 1.0 << 1.0;
      pqSMAdaptor::setMultipleElementProperty(lut->GetProperty("RGBPoints"), values);
      vtkSMPropertyHelper(lut, "ColorSpace").Set(0);
       vtkSMPropertyHelper(lut, "Discretize").Set(0);
      lut->UpdateVTKObjects();
      vtkSMPropertyHelper(imagerep->getProxy(), "LookupTable").Set(lut);
      vtkSMPropertyHelper(imagerep->getProxy(), "SelectionVisibility").Set(0);

      imagerep->getProxy()->UpdateVTKObjects();
      imagerep->renderViewEventually();
      this->ImageRepresentation = imagerep;
      }
    }

  this->InternalWidget->generateContoursBox->setEnabled(true);
  this->RenderView->resetCamera();
  this->RenderView->forceRender();
 // see pqProxyInformationWidget
  if(imagesource)
    {
    vtkPVDataInformation *imageInfo = imagesource->getOutputPort(0)->getDataInformation();
    int extents[6];
    imageInfo->GetExtent(extents);
    vtkPVDataSetAttributesInformation *pointInfo = imageInfo->GetPointDataInformation();
    double range[2] = {0, 0};
    if (pointInfo->GetNumberOfArrays() > 0)
      {
      vtkPVArrayInformation *arrayInfo = pointInfo->GetArrayInformation(0);
      if (arrayInfo->GetNumberOfComponents() == 1)
        {
        arrayInfo->GetComponentRange(0, range);
        }
      }

    this->InternalWidget->imageDimensionsLabel->setText(
      QString("Dimensions:   %1 x %2").arg(extents[1] - extents[0] + 1).arg(extents[3] - extents[2] + 1) );
    this->InternalWidget->imageScalarRangeLabel->setText(
      QString("Scalar Range:   %1, %2").arg(range[0]).arg(range[1]) );

    }
  delete this->Progress;
  this->Progress = NULL;

   // not ready to enable abort
  //QObject::connect(this->ProgressWidget, SIGNAL(abortPressed()),
  //                 progress_manager, SLOT(triggerAbort()));
  //QObject::connect(progress_manager, SIGNAL(abort()),
  //                 this, SLOT(abort()));
}

//-----------------------------------------------------------------------------
pqGenerateContoursDialog::~pqGenerateContoursDialog()
{
  delete this->InternalWidget;
  if (this->MainDialog)
    {
    delete MainDialog;
    }
  pqObjectBuilder* builder = pqApplicationCore::instance()->getObjectBuilder();
  if (this->ContourRepresentation)
    {
    builder->destroy(this->ContourRepresentation);
    }
  if (this->CleanPolyLines)
    {
    builder->destroy(this->CleanPolyLines);
    this->CleanPolyLines = 0;
    }
  if (this->ContourSource)
    {
    builder->destroy(this->ContourSource);
    this->ContourSource = 0;
    }
  if (this->ImageSource)
    {
    builder->destroy(this->ImageSource);
    this->ImageSource = 0;
    }
}
//-----------------------------------------------------------------------------
int pqGenerateContoursDialog::exec()
{
  return this->MainDialog->exec();
}
//-----------------------------------------------------------------------------
void pqGenerateContoursDialog::generateContours()
{
  this->Progress = new QProgressDialog(this->MainDialog);
  this->Progress->setMaximum(0.0);
  this->Progress->setMinimum(0.0);

  this->Progress->show();
  this->ProgressMessage = "Computing Contours";
  this->updateProgress(this->ProgressMessage, 0);
  this->disableWhileProcessing();

  pqObjectBuilder* builder = pqApplicationCore::instance()->getObjectBuilder();

  // save values used for this computation, so can know whether reecute is necessary
  this->ContourValue = this->InternalWidget->contourValue->text().toDouble();
  this->MinimumLineLength = this->InternalWidget->minimumLineLength->text().toDouble();
  this->UseRelativeLineLength = this->InternalWidget->relativeLineLengthCheckbox->isChecked();

  if (!this->ContourSource)
    {
    this->ContourSource = builder->createFilter("filters",
                                                "Contour", this->ImageSource);
    }
  vtkSMPropertyHelper(this->ContourSource->getProxy(), "ContourValues").Set(
    this->ContourValue );
  this->ContourSource->getProxy()->UpdateVTKObjects();

  // connects lines up and get rid of short lines (with current hard coded setting, lines
  // less than 5 times the average line length are discarded)
  if (!this->CleanPolyLines)
    {
    this->CleanPolyLines = builder->createFilter("polygon_filters",
      "CleanPolylines", this->ContourSource);
    }
  vtkSMPropertyHelper(this->CleanPolyLines->getProxy(), "UseRelativeLineLength").Set(
    this->UseRelativeLineLength );
  vtkSMPropertyHelper(this->CleanPolyLines->getProxy(), "MinimumLineLength").Set(
    this->MinimumLineLength );
  this->CleanPolyLines->getProxy()->UpdateVTKObjects();
  vtkSMSourceProxy::SafeDownCast( this->CleanPolyLines->getProxy() )->
    UpdatePipeline();


  pqPipelineSource *polyDataStatsFilter = builder->createFilter("polygon_filters",
    "PolyDataStatsFilter", this->CleanPolyLines);
  vtkSMSourceProxy::SafeDownCast( polyDataStatsFilter->getProxy() )->
    UpdatePipeline();
  polyDataStatsFilter->getProxy()->UpdatePropertyInformation();

  vtkIdType numberOfLines =
    vtkSMPropertyHelper(polyDataStatsFilter->getProxy(), "NumberOfLines").GetAsIdType();

  vtkIdType numberOfPoints =
    vtkSMPropertyHelper(polyDataStatsFilter->getProxy(), "NumberOfPoints").GetAsIdType();

  builder->destroy( polyDataStatsFilter );

  this->InternalWidget->numberOfContoursLabel->setText( QString("Number Of Contours: %1").arg(numberOfLines) );
  this->InternalWidget->numberOfPointsLabel->setText( QString("Number Of Points: %1").arg(numberOfPoints) );

  // add as a single representation.... by default it colors by LineIndex, so
  // with a decent color map, should be able to distinguish many the separate contours
  if (!this->ContourRepresentation)
    {
    this->ContourRepresentation =
      builder->createDataRepresentation(this->CleanPolyLines->getOutputPort(0),
      this->RenderView, "GeometryRepresentation");
    pqSMAdaptor::setElementProperty(
      this->ContourRepresentation->getProxy()->GetProperty("LineWidth"), 2);
    // move contour slightly in front of the image, so no rendering "issues"
    double position[3] = {0, 0, 0.1};
    vtkSMPropertyHelper(this->ContourRepresentation->getProxy(), "Position").Set(position, 3);
    this->ContourRepresentation->getProxy()->UpdateVTKObjects();
    }

  this->RenderView->render();
  this->InternalWidget->createContourNodesButton->setEnabled(true);
  this->ProgressMessage = "";
  this->updateProgress(this->ProgressMessage, 0);
  this->InternalWidget->okCancelBox->setEnabled(true);

  // reanble contour box, but disable contour generation button until parameter change
  this->InternalWidget->generateContoursBox->setEnabled(true);
  this->InternalWidget->loadImageFrame->setEnabled(true);
  this->InternalWidget->generateContoursButton->setEnabled(false);
  delete this->Progress;
  this->Progress = NULL;
}

//-----------------------------------------------------------------------------
void pqGenerateContoursDialog::onAccecptContours()
{
  this->Progress = new QProgressDialog(this->MainDialog);
  this->Progress->setMaximum(0.0);
  this->Progress->setMinimum(0.0);

  this->Progress->show();
  this->disableWhileProcessing();

  emit this->contoursAccepted(this->CleanPolyLines);
  this->MainDialog->done(QDialog::Accepted);

  delete this->Progress;
  this->Progress = NULL;

}

//-----------------------------------------------------------------------------
void pqGenerateContoursDialog::onCancel()
{
  this->MainDialog->done(QDialog::Rejected);

  // disconnect progressManager????
}

//-----------------------------------------------------------------------------
void pqGenerateContoursDialog::close()
{
  this->onCancel();
}

//-----------------------------------------------------------------------------
void pqGenerateContoursDialog::updateProgress(const QString& message, int progress)
{
  // Is there any progress being reported?
  if (!this->Progress)
    {
    return;
    }
  this->Progress->setLabelText(message);
  this->Progress->setValue(progress);
  QCoreApplication::processEvents();
}

//-----------------------------------------------------------------------------
void pqGenerateContoursDialog::disableWhileProcessing()
{
  this->InternalWidget->loadImageFrame->setEnabled(false);
  this->InternalWidget->generateContoursBox->setEnabled(false);
  this->InternalWidget->okCancelBox->setEnabled(false);
}

//-----------------------------------------------------------------------------
void pqGenerateContoursDialog::updateContourButtonStatus()
{
  if (this->ContourValue == this->InternalWidget->contourValue->text().toDouble() &&
    this->MinimumLineLength == this->InternalWidget->minimumLineLength->text().toDouble() &&
    this->UseRelativeLineLength == this->InternalWidget->relativeLineLengthCheckbox->isChecked())
    {
    this->InternalWidget->generateContoursButton->setEnabled(false);
    }
  else
    {
    this->InternalWidget->generateContoursButton->setEnabled(true);
    }
}
//-----------------------------------------------------------------------------
void pqGenerateContoursDialog::onOpacityChanged(int opacity)
{
  if(this->ImageRepresentation)
    {
    vtkSMPropertyHelper(this->ImageRepresentation->getProxy(), "Opacity").Set(
      static_cast<double>(opacity) / 100.0 );
    this->ImageRepresentation->getProxy()->UpdateVTKObjects();
    this->RenderView->render();
    }
}