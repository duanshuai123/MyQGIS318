/***************************************************************************
    QgsLayerStylingWidgetV2.cpp
    ---------------------
    begin                : April 2016
    copyright            : (C) 2016 by Nathan Woodrow
    email                : woodrow dot nathan at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/
#include <QLabel>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include <QWidget>
#include <QSizePolicy>
#include <QUndoStack>
#include <QListWidget>

#include "qgsapplication.h"
#include "qgslabelingwidget.h"
#include "qgsmaskingwidget.h"
#include "qgslayerstylingwidgetV2.h"
#include "qgsrastertransparencywidget.h"
#include "qgsrendererpropertiesdialog.h"
#include "qgsrendererrasterpropertieswidget.h"
#include "qgsrenderermeshpropertieswidget.h"
#include "qgsrasterhistogramwidget.h"
#include "qgsrasterrenderer.h"
#include "qgsrasterrendererwidget.h"
#include "qgsmapcanvas.h"
#include "qgsmaplayer.h"
#include "qgsstyle.h"
#include "qgsvectorlayer.h"
#include "qgspointcloudlayer.h"
#include "qgsvectortilelayer.h"
#include "qgsvectortilebasiclabelingwidget.h"
#include "qgsvectortilebasicrendererwidget.h"
#include "qgsmeshlayer.h"
#include "qgsproject.h"
#include "qgsundowidget.h"
#include "qgsreadwritecontext.h"
#include "qgsrenderer.h"
#include "qgsrendererregistry.h"
#include "qgsrasterdataprovider.h"
#include "qgsrasterlayer.h"
#include "qgsmaplayerconfigwidget.h"
#include "qgsmaplayerstylemanagerwidget.h"
#include "qgsruntimeprofiler.h"
#include "qgsrasterminmaxwidget.h"
#include "qgisapp.h"
#include "qgssymbolwidgetcontext.h"

#ifdef HAVE_3D
#include "qgsvectorlayer3drendererwidget.h"
#include "qgsmeshlayer3drendererwidget.h"
#endif


QgsLayerStylingWidgetV2::QgsLayerStylingWidgetV2( QgsMapCanvas *canvas, QgsMessageBar *messageBar, const QList<QgsMapLayerConfigWidgetFactory *> &pages, QWidget *parent )
  : QWidget( parent )
  , mNotSupportedPage( 0 )
  , mLayerPage( 1 )
  , mMapCanvas( canvas )
  , mMessageBar( messageBar )
  , mBlockAutoApply( false )
  , mPageFactories( pages )
{
  setupUi( this );

  mOptionsListWidget->setIconSize( QgisApp::instance()->iconSize( false ) );
  mOptionsListWidget->setMaximumWidth( static_cast< int >( mOptionsListWidget->iconSize().width() * 1.18 ) );

  connect( QgsProject::instance(), static_cast < void ( QgsProject::* )( QgsMapLayer * ) > ( &QgsProject::layerWillBeRemoved ), this, &QgsLayerStylingWidgetV2::layerAboutToBeRemoved );

  QgsSettings settings;
  mLiveApplyCheck->setChecked( settings.value( QStringLiteral( "UI/autoApplyStyling" ), true ).toBool() );

  mAutoApplyTimer = new QTimer( this );
  mAutoApplyTimer->setSingleShot( true );

  mUndoWidget = new QgsUndoWidget( this, mMapCanvas );
  mUndoWidget->setButtonsVisible( false );
  mUndoWidget->setAutoDelete( false );
  mUndoWidget->setObjectName( QStringLiteral( "Undo Styles" ) );
  mUndoWidget->hide();

  mStyleManagerFactory = new QgsLayerStyleManagerWidgetFactory();

  setPageFactories( pages );

  connect( mUndoButton, &QAbstractButton::pressed, this, &QgsLayerStylingWidgetV2::undo );
  connect( mRedoButton, &QAbstractButton::pressed, this, &QgsLayerStylingWidgetV2::redo );

  connect( mAutoApplyTimer, &QTimer::timeout, this, &QgsLayerStylingWidgetV2::apply );

  connect( mOptionsListWidget, &QListWidget::currentRowChanged, this, &QgsLayerStylingWidgetV2::updateCurrentWidgetLayer );
  connect( mButtonBox->button( QDialogButtonBox::Apply ), &QAbstractButton::clicked, this, &QgsLayerStylingWidgetV2::apply );
  connect( mLayerCombo, &QgsMapLayerComboBox::layerChanged, this, &QgsLayerStylingWidgetV2::setLayer );
  connect( mLiveApplyCheck, &QAbstractButton::toggled, this, &QgsLayerStylingWidgetV2::liveApplyToggled );

  mLayerCombo->setFilters( QgsMapLayerProxyModel::Filter::HasGeometry
                           | QgsMapLayerProxyModel::Filter::RasterLayer
                           | QgsMapLayerProxyModel::Filter::PluginLayer
                           | QgsMapLayerProxyModel::Filter::MeshLayer
                           | QgsMapLayerProxyModel::Filter::VectorTileLayer
                           | QgsMapLayerProxyModel::Filter::PointCloudLayer );

  mStackedWidget->setCurrentIndex( 0 );
}

QgsLayerStylingWidgetV2::~QgsLayerStylingWidgetV2()
{
  delete mStyleManagerFactory; //@duanshuai
}

void QgsLayerStylingWidgetV2::setPageFactories( const QList<QgsMapLayerConfigWidgetFactory *> &factories )
{
  mPageFactories = factories;
  // Always append the style manager factory at the bottom of the list
  mPageFactories.append( mStyleManagerFactory );
}

void QgsLayerStylingWidgetV2::blockUpdates( bool blocked )
{
  if ( !mCurrentLayer )
    return;

  if ( blocked )
  {
    disconnect( mCurrentLayer, &QgsMapLayer::styleChanged, this, &QgsLayerStylingWidgetV2::updateCurrentWidgetLayer );
  }
  else
  {
    connect( mCurrentLayer, &QgsMapLayer::styleChanged, this, &QgsLayerStylingWidgetV2::updateCurrentWidgetLayer );
  }
}

void QgsLayerStylingWidgetV2::setLayer( QgsMapLayer *layer )
{
  if ( layer == mCurrentLayer )
    return;


  // when current layer is changed, apply the main panel stack to allow it to gracefully clean up
  mWidgetStack->acceptAllPanels();

  if ( mCurrentLayer )
  {
    disconnect( mCurrentLayer, &QgsMapLayer::styleChanged, this, &QgsLayerStylingWidgetV2::updateCurrentWidgetLayer );
  }

  if ( !layer || !layer->isSpatial() || !QgsProject::instance()->layerIsEmbedded( layer->id() ).isEmpty() )
  {
    mLayerCombo->setLayer( nullptr );
    mStackedWidget->setCurrentIndex( mNotSupportedPage );
    mLastStyleXml.clear();
    mCurrentLayer = nullptr;
    return;
  }

  bool sameLayerType = false;
  if ( mCurrentLayer )
  {
    sameLayerType = mCurrentLayer->type() == layer->type();
  }

  mCurrentLayer = layer;

  mUndoWidget->setUndoStack( layer->undoStackStyles() );

  connect( mCurrentLayer, &QgsMapLayer::styleChanged, this, &QgsLayerStylingWidgetV2::updateCurrentWidgetLayer );

  int lastPage = mOptionsListWidget->currentIndex().row();
  mOptionsListWidget->blockSignals( true );
  mOptionsListWidget->clear();
  mUserPages.clear();

  switch ( layer->type() )
  {
    case QgsMapLayerType::VectorLayer:
    {
      QListWidgetItem *symbolItem = new QListWidgetItem( QgsApplication::getThemeIcon( QStringLiteral( "propertyicons/symbology.svg" ) ), QString() );
      symbolItem->setData( Qt::UserRole, Symbology );
      symbolItem->setToolTip( tr( "Symbology" ) );
      mOptionsListWidget->addItem( symbolItem );
      QListWidgetItem *labelItem = new QListWidgetItem( QgsApplication::getThemeIcon( QStringLiteral( "labelingSingle.svg" ) ), QString() );
      labelItem->setData( Qt::UserRole, VectorLabeling );
      labelItem->setToolTip( tr( "Labels" ) );
      mOptionsListWidget->addItem( labelItem );
      QListWidgetItem *maskItem = new QListWidgetItem( QgsApplication::getThemeIcon( QStringLiteral( "propertyicons/labelmask.svg" ) ), QString() );
      maskItem->setData( Qt::UserRole, VectorLabeling );
      maskItem->setToolTip( tr( "Masks" ) );
      mOptionsListWidget->addItem( maskItem );

#ifdef HAVE_3D
      QListWidgetItem *symbol3DItem = new QListWidgetItem( QgsApplication::getThemeIcon( QStringLiteral( "3d.svg" ) ), QString() );
      symbol3DItem->setData( Qt::UserRole, Symbology3D );
      symbol3DItem->setToolTip( tr( "3D View" ) );
      mOptionsListWidget->addItem( symbol3DItem );
#endif
      break;
    }
    case QgsMapLayerType::RasterLayer:
    {
      QListWidgetItem *symbolItem = new QListWidgetItem( QgsApplication::getThemeIcon( QStringLiteral( "propertyicons/symbology.svg" ) ), QString() );
      symbolItem->setData( Qt::UserRole, Symbology );
      symbolItem->setToolTip( tr( "Symbology" ) );
      mOptionsListWidget->addItem( symbolItem );
      QListWidgetItem *transparencyItem = new QListWidgetItem( QgsApplication::getThemeIcon( QStringLiteral( "propertyicons/transparency.svg" ) ), QString() );
      transparencyItem->setToolTip( tr( "Transparency" ) );
      transparencyItem->setData( Qt::UserRole, RasterTransparency );
      mOptionsListWidget->addItem( transparencyItem );

      if ( static_cast<QgsRasterLayer *>( layer )->dataProvider() && static_cast<QgsRasterLayer *>( layer )->dataProvider()->capabilities() & QgsRasterDataProvider::Size )
      {
        QListWidgetItem *histogramItem = new QListWidgetItem( QgsApplication::getThemeIcon( QStringLiteral( "propertyicons/histogram.svg" ) ), QString() );
        histogramItem->setData( Qt::UserRole, RasterHistogram );
        mOptionsListWidget->addItem( histogramItem );
        histogramItem->setToolTip( tr( "Histogram" ) );
      }
      break;
    }
    case QgsMapLayerType::MeshLayer:
    {
      QListWidgetItem *symbolItem = new QListWidgetItem( QgsApplication::getThemeIcon( QStringLiteral( "propertyicons/symbology.svg" ) ), QString() );
      symbolItem->setData( Qt::UserRole, Symbology );
      symbolItem->setToolTip( tr( "Symbology" ) );
      mOptionsListWidget->addItem( symbolItem );

#ifdef HAVE_3D
      QListWidgetItem *symbol3DItem = new QListWidgetItem( QgsApplication::getThemeIcon( QStringLiteral( "3d.svg" ) ), QString() );
      symbol3DItem->setData( Qt::UserRole, Symbology3D );
      symbol3DItem->setToolTip( tr( "3D View" ) );
      mOptionsListWidget->addItem( symbol3DItem );
#endif
      break;
    }

    case QgsMapLayerType::VectorTileLayer:
    {
      QListWidgetItem *symbolItem = new QListWidgetItem( QgsApplication::getThemeIcon( QStringLiteral( "propertyicons/symbology.svg" ) ), QString() );
      symbolItem->setData( Qt::UserRole, Symbology );
      symbolItem->setToolTip( tr( "Symbology" ) );
      mOptionsListWidget->addItem( symbolItem );
      QListWidgetItem *labelItem = new QListWidgetItem( QgsApplication::getThemeIcon( QStringLiteral( "labelingSingle.svg" ) ), QString() );
      labelItem->setData( Qt::UserRole, VectorLabeling );
      labelItem->setToolTip( tr( "Labels" ) );
      mOptionsListWidget->addItem( labelItem );
      break;
    }

    case QgsMapLayerType::PointCloudLayer:
    case QgsMapLayerType::PluginLayer:
    case QgsMapLayerType::AnnotationLayer:
      break;
  }

  for ( QgsMapLayerConfigWidgetFactory *factory : qgis::as_const( mPageFactories ) )
  {
    if ( factory->supportsStyleDock() && factory->supportsLayer( layer ) )
    {
      QListWidgetItem *item = new QListWidgetItem( factory->icon(), QString() );
      item->setToolTip( factory->title() );
      mOptionsListWidget->addItem( item );
      int row = mOptionsListWidget->row( item );
      mUserPages[row] = factory;
    }
  }
  QListWidgetItem *historyItem = new QListWidgetItem( QgsApplication::getThemeIcon( QStringLiteral( "mActionHistory.svg" ) ), QString() );
  historyItem->setData( Qt::UserRole, History );
  historyItem->setToolTip( tr( "History" ) );
  mOptionsListWidget->addItem( historyItem );
  mOptionsListWidget->blockSignals( false );

  if ( sameLayerType )
  {
    mOptionsListWidget->setCurrentRow( lastPage );
  }
  else
  {
    mOptionsListWidget->setCurrentRow( 0 );
  }

  mStackedWidget->setCurrentIndex( 1 );

  QString errorMsg;
  QDomDocument doc( QStringLiteral( "style" ) );
  mLastStyleXml = doc.createElement( QStringLiteral( "style" ) );
  doc.appendChild( mLastStyleXml );
  mCurrentLayer->writeStyle( mLastStyleXml, doc, errorMsg, QgsReadWriteContext() );
}

void QgsLayerStylingWidgetV2::apply()
{
  if ( !mCurrentLayer )
    return;

  disconnect( mCurrentLayer, &QgsMapLayer::styleChanged, this, &QgsLayerStylingWidgetV2::updateCurrentWidgetLayer );

  QString undoName = QStringLiteral( "Style Change" );

  QWidget *current = mWidgetStack->mainPanel();

  bool styleWasChanged = false;
  bool triggerRepaint = false;  // whether the change needs the layer to be repainted
  if ( QgsLabelingWidget *widget = qobject_cast<QgsLabelingWidget *>( current ) )
  {
    widget->apply();
    styleWasChanged = true;
    undoName = QStringLiteral( "Label Change" );
  }
  if ( QgsMaskingWidget *widget = qobject_cast<QgsMaskingWidget *>( current ) )
  {
    widget->apply();
    styleWasChanged = true;
    undoName = QStringLiteral( "Mask Change" );
  }
  if ( QgsPanelWidgetWrapper *wrapper = qobject_cast<QgsPanelWidgetWrapper *>( current ) )
  {
    if ( QgsRendererPropertiesDialog *widget = qobject_cast<QgsRendererPropertiesDialog *>( wrapper->widget() ) )
    {
      widget->apply();
      QgsVectorLayer *layer = qobject_cast<QgsVectorLayer *>( mCurrentLayer );
      QgsRendererAbstractMetadata *m = QgsApplication::rendererRegistry()->rendererMetadata( layer->renderer()->type() );
      undoName = QStringLiteral( "Style Change - %1" ).arg( m->visibleName() );
      styleWasChanged = true;
      triggerRepaint = true;
    }
  }
  else if ( QgsRasterTransparencyWidget *widget = qobject_cast<QgsRasterTransparencyWidget *>( current ) )
  {
    widget->apply();
    styleWasChanged = true;
    triggerRepaint = true;
  }
  else if ( qobject_cast<QgsRasterHistogramWidget *>( current ) )
  {
    mRasterStyleWidget->apply();
    styleWasChanged = true;
    triggerRepaint = true;
  }
  else if ( QgsMapLayerConfigWidget *widget = qobject_cast<QgsMapLayerConfigWidget *>( current ) )
  {
    widget->apply();
    styleWasChanged = true;
    triggerRepaint = widget->shouldTriggerLayerRepaint();
  }

  pushUndoItem( undoName, triggerRepaint );

  if ( styleWasChanged )
  {
    emit styleChanged( mCurrentLayer );
    QgsProject::instance()->setDirty( true );
  }
  connect( mCurrentLayer, &QgsMapLayer::styleChanged, this, &QgsLayerStylingWidgetV2::updateCurrentWidgetLayer );
}

void QgsLayerStylingWidgetV2::autoApply()
{
  if ( mLiveApplyCheck->isChecked() && !mBlockAutoApply )
  {
    mAutoApplyTimer->start( 100 );
  }
}

void QgsLayerStylingWidgetV2::undo()
{
  mUndoWidget->undo();
  updateCurrentWidgetLayer();
}

void QgsLayerStylingWidgetV2::redo()
{
  mUndoWidget->redo();
  updateCurrentWidgetLayer();
}

void QgsLayerStylingWidgetV2::updateCurrentWidgetLayer()
{
  if ( !mCurrentLayer )
    return;  // non-spatial are ignored in setLayer()

  mBlockAutoApply = true;

  whileBlocking( mLayerCombo )->setLayer( mCurrentLayer );

  int row = mOptionsListWidget->currentIndex().row();

  mStackedWidget->setCurrentIndex( mLayerPage );

  QgsPanelWidget *current = mWidgetStack->takeMainPanel();
  if ( current )
  {
    if ( QgsLabelingWidget *widget = qobject_cast<QgsLabelingWidget *>( current ) )
    {
      mLabelingWidget = widget;
    }
    else if ( QgsMaskingWidget *widget = qobject_cast<QgsMaskingWidget *>( current ) )
    {
      mMaskingWidget = widget;
    }
    else if ( QgsUndoWidget *widget = qobject_cast<QgsUndoWidget *>( current ) )
    {
      mUndoWidget = widget;
    }
    else if ( QgsRendererRasterPropertiesWidget *widget = qobject_cast<QgsRendererRasterPropertiesWidget *>( current ) )
    {
      mRasterStyleWidget = widget;
    }
#ifdef HAVE_3D
    else if ( QgsVectorLayer3DRendererWidget *widget = qobject_cast<QgsVectorLayer3DRendererWidget *>( current ) )
    {
      mVector3DWidget = widget;
    }
#endif
    else if ( QgsRendererMeshPropertiesWidget *widget = qobject_cast<QgsRendererMeshPropertiesWidget *>( current ) )
    {
      mMeshStyleWidget = widget;
    }
#ifdef HAVE_3D
    else if ( QgsMeshLayer3DRendererWidget *widget = qobject_cast<QgsMeshLayer3DRendererWidget *>( current ) )
    {
      mMesh3DWidget = widget;
    }
#endif
  }

  mWidgetStack->clear();
  // Create the user page widget if we are on one of those pages
  // TODO Make all widgets use this method.
  if ( mUserPages.contains( row ) )
  {
    QgsMapLayerConfigWidget *panel = mUserPages[row]->createWidget( mCurrentLayer, mMapCanvas, true, mWidgetStack );
    if ( panel )
    {
      panel->setDockMode( true );
      connect( panel, &QgsPanelWidget::widgetChanged, this, &QgsLayerStylingWidgetV2::autoApply );
      mWidgetStack->setMainPanel( panel );
    }
  }

  // The last widget is always the undo stack.
  if ( row == mOptionsListWidget->count() - 1 )
  {
    mWidgetStack->setMainPanel( mUndoWidget );
  }
  else
  {
    switch ( mCurrentLayer->type() )
    {
      case QgsMapLayerType::VectorLayer:
      {
        QgsVectorLayer *vlayer = qobject_cast<QgsVectorLayer *>( mCurrentLayer );

        switch ( row )
        {
          case 0: // Style
          {
            QgsRendererPropertiesDialog *styleWidget = new QgsRendererPropertiesDialog( vlayer, QgsStyle::defaultStyle(), true, mStackedWidget );
            QgsSymbolWidgetContext context;
            context.setMapCanvas( mMapCanvas );
            context.setMessageBar( mMessageBar );
            styleWidget->setContext( context );
            styleWidget->setDockMode( true );
            connect( styleWidget, &QgsRendererPropertiesDialog::widgetChanged, this, &QgsLayerStylingWidgetV2::autoApply );
            QgsPanelWidgetWrapper *wrapper = new QgsPanelWidgetWrapper( styleWidget, mStackedWidget );
            wrapper->setDockMode( true );
            connect( styleWidget, &QgsRendererPropertiesDialog::showPanel, wrapper, &QgsPanelWidget::openPanel );
            mWidgetStack->setMainPanel( wrapper );
            break;
          }
          case 1: // Labels
          {
            if ( !mLabelingWidget )
            {
              mLabelingWidget = new QgsLabelingWidget( nullptr, mMapCanvas, mWidgetStack, mMessageBar );
              mLabelingWidget->setDockMode( true );
              connect( mLabelingWidget, &QgsLabelingWidget::widgetChanged, this, &QgsLayerStylingWidgetV2::autoApply );
            }
            mLabelingWidget->setLayer( vlayer );
            mWidgetStack->setMainPanel( mLabelingWidget );
            break;
          }
          case 2: // Masks
          {
            if ( !mMaskingWidget )
            {
              mMaskingWidget = new QgsMaskingWidget( mWidgetStack );
              mMaskingWidget->layout()->setContentsMargins( 0, 0, 0, 0 );
              connect( mMaskingWidget, &QgsMaskingWidget::widgetChanged, this, &QgsLayerStylingWidgetV2::autoApply );
            }
            mMaskingWidget->setLayer( vlayer );
            mWidgetStack->setMainPanel( mMaskingWidget );
            break;
          }
#ifdef HAVE_3D
          case 3:  // 3D View
          {
            if ( !mVector3DWidget )
            {
              mVector3DWidget = new QgsVectorLayer3DRendererWidget( vlayer, mMapCanvas, mWidgetStack );
              mVector3DWidget->setDockMode( true );
              connect( mVector3DWidget, &QgsVectorLayer3DRendererWidget::widgetChanged, this, &QgsLayerStylingWidgetV2::autoApply );
            }
            mVector3DWidget->syncToLayer( vlayer );
            mWidgetStack->setMainPanel( mVector3DWidget );
            break;
          }
#endif
          default:
            break;
        }
        break;
      }

      case QgsMapLayerType::RasterLayer:
      {
        QgsRasterLayer *rlayer = qobject_cast<QgsRasterLayer *>( mCurrentLayer );
        bool hasMinMaxCollapsedState = false;
        bool minMaxCollapsed = false;

        switch ( row )
        {
          case 0: // Style
          {
            // Backup collapsed state of min/max group so as to restore it
            // on the new widget.
            if ( mRasterStyleWidget )
            {
              QgsRasterRendererWidget *currentRenderWidget = mRasterStyleWidget->currentRenderWidget();
              if ( currentRenderWidget )
              {
                QgsRasterMinMaxWidget *mmWidget = currentRenderWidget->minMaxWidget();
                if ( mmWidget )
                {
                  hasMinMaxCollapsedState = true;
                  minMaxCollapsed = mmWidget->isCollapsed();
                }
              }
            }
            mRasterStyleWidget = new QgsRendererRasterPropertiesWidget( rlayer, mMapCanvas, mWidgetStack );
            if ( hasMinMaxCollapsedState )
            {
              QgsRasterRendererWidget *currentRenderWidget = mRasterStyleWidget->currentRenderWidget();
              if ( currentRenderWidget )
              {
                QgsRasterMinMaxWidget *mmWidget = currentRenderWidget->minMaxWidget();
                if ( mmWidget )
                {
                  mmWidget->setCollapsed( minMaxCollapsed );
                }
              }
            }
            mRasterStyleWidget->setDockMode( true );
            connect( mRasterStyleWidget, &QgsPanelWidget::widgetChanged, this, &QgsLayerStylingWidgetV2::autoApply );
            mWidgetStack->setMainPanel( mRasterStyleWidget );
            break;
          }

          case 1: // Transparency
          {
            QgsRasterTransparencyWidget *transwidget = new QgsRasterTransparencyWidget( rlayer, mMapCanvas, mWidgetStack );
            transwidget->setDockMode( true );
            connect( transwidget, &QgsPanelWidget::widgetChanged, this, &QgsLayerStylingWidgetV2::autoApply );
            mWidgetStack->setMainPanel( transwidget );
            break;
          }
          case 2: // Histogram
          {
            if ( rlayer->dataProvider()->capabilities() & QgsRasterDataProvider::Size )
            {
              if ( !mRasterStyleWidget )
              {
                mRasterStyleWidget = new QgsRendererRasterPropertiesWidget( rlayer, mMapCanvas, mWidgetStack );
                mRasterStyleWidget->syncToLayer( rlayer );
              }
              connect( mRasterStyleWidget, &QgsPanelWidget::widgetChanged, this, &QgsLayerStylingWidgetV2::autoApply );

              QgsRasterHistogramWidget *widget = new QgsRasterHistogramWidget( rlayer, mWidgetStack );
              connect( widget, &QgsPanelWidget::widgetChanged, this, &QgsLayerStylingWidgetV2::autoApply );
              QString name = mRasterStyleWidget->currentRenderWidget()->renderer()->type();
              widget->setRendererWidget( name, mRasterStyleWidget->currentRenderWidget() );
              widget->setDockMode( true );

              mWidgetStack->setMainPanel( widget );
            }
            break;
          }
          default:
            break;
        }
        break;
      }

      case QgsMapLayerType::MeshLayer:
      {
        QgsMeshLayer *meshLayer = qobject_cast<QgsMeshLayer *>( mCurrentLayer );
        switch ( row )
        {
          case 0: // Style
          {
            mMeshStyleWidget = new QgsRendererMeshPropertiesWidget( meshLayer, mMapCanvas, mWidgetStack );

            mMeshStyleWidget->setDockMode( true );
            connect( mMeshStyleWidget, &QgsPanelWidget::widgetChanged, this, &QgsLayerStylingWidgetV2::autoApply );
            mWidgetStack->setMainPanel( mMeshStyleWidget );
            break;
          }
#ifdef HAVE_3D
          case 1:  // 3D View
          {
            if ( !mMesh3DWidget )
            {
              mMesh3DWidget = new QgsMeshLayer3DRendererWidget( nullptr, mMapCanvas, mWidgetStack );
              mMesh3DWidget->setDockMode( true );
              connect( mMesh3DWidget, &QgsMeshLayer3DRendererWidget::widgetChanged, this, &QgsLayerStylingWidgetV2::autoApply );
            }
            mMesh3DWidget->syncToLayer( meshLayer );
            mWidgetStack->setMainPanel( mMesh3DWidget );
            break;
          }
#endif
          default:
            break;
        }
        break;
      }

      case QgsMapLayerType::VectorTileLayer:
      {
        QgsVectorTileLayer *vtLayer = qobject_cast<QgsVectorTileLayer *>( mCurrentLayer );
        switch ( row )
        {
          case 0: // Style
          {
            mVectorTileStyleWidget = new QgsVectorTileBasicRendererWidget( vtLayer, mMapCanvas, mMessageBar, mWidgetStack );
            mVectorTileStyleWidget->setDockMode( true );
            connect( mVectorTileStyleWidget, &QgsPanelWidget::widgetChanged, this, &QgsLayerStylingWidgetV2::autoApply );
            mWidgetStack->setMainPanel( mVectorTileStyleWidget );
            break;
          }
          case 1: // Labeling
          {
            mVectorTileLabelingWidget = new QgsVectorTileBasicLabelingWidget( vtLayer, mMapCanvas, mMessageBar, mWidgetStack );
            mVectorTileLabelingWidget->setDockMode( true );
            connect( mVectorTileLabelingWidget, &QgsPanelWidget::widgetChanged, this, &QgsLayerStylingWidgetV2::autoApply );
            mWidgetStack->setMainPanel( mVectorTileLabelingWidget );
            break;
          }
          default:
            break;
        }
        break;
      }

      case QgsMapLayerType::PointCloudLayer:
      {
        break;
      }
      case QgsMapLayerType::PluginLayer:
      case QgsMapLayerType::AnnotationLayer:
      {
        mStackedWidget->setCurrentIndex( mNotSupportedPage );
        break;
      }
    }
  }

  mBlockAutoApply = false;
}

void QgsLayerStylingWidgetV2::setCurrentPage( QgsLayerStylingWidgetV2::Page page )
{
  for ( int i = 0; i < mOptionsListWidget->count(); ++i )
  {
    int data = mOptionsListWidget->item( i )->data( Qt::UserRole ).toInt();
    if ( data == static_cast< int >( page ) )
    {
      mOptionsListWidget->setCurrentRow( i );
      return;
    }
  }
}

void QgsLayerStylingWidgetV2::layerAboutToBeRemoved( QgsMapLayer *layer )
{
  if ( layer == mCurrentLayer )
  {
    // when current layer is removed, apply the main panel stack to allow it to gracefully clean up
    mWidgetStack->acceptAllPanels();

    mAutoApplyTimer->stop();
    setLayer( nullptr );
  }
}

void QgsLayerStylingWidgetV2::liveApplyToggled( bool value )
{
  QgsSettings settings;
  settings.setValue( QStringLiteral( "UI/autoApplyStyling" ), value );
}

void QgsLayerStylingWidgetV2::pushUndoItem( const QString &name, bool triggerRepaint )
{
  QString errorMsg;
  QDomDocument doc( QStringLiteral( "style" ) );
  QDomElement rootNode = doc.createElement( QStringLiteral( "qgis" ) );
  doc.appendChild( rootNode );
  mCurrentLayer->writeStyle( rootNode, doc, errorMsg, QgsReadWriteContext() );
  mCurrentLayer->undoStackStyles()->push( new QgsMapLayerStyleCommand( mCurrentLayer, name, rootNode, mLastStyleXml, triggerRepaint ) );
  // Override the last style on the stack
  mLastStyleXml = rootNode.cloneNode();
}

