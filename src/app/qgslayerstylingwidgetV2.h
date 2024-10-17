/***************************************************************************
    qgslayerstylingwidget.h
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
#ifndef QGSLAYERSTYLESDOCK_H_V2_
#define QGSLAYERSTYLESDOCK_H_V2_

#include <QToolButton>
#include <QWidget>
#include <QLabel>
#include <QTabWidget>
#include <QStackedWidget>
#include <QDialogButtonBox>
#include <QCheckBox>
#include <QUndoCommand>
#include <QDomNode>
#include <QTime>
#include <QTimer>
#include "ui_qgsmapstylingwidgetbase.h"
#include "qgsmaplayerconfigwidgetfactory.h"
#include "qgis_app.h"

class QgsLabelingWidget;
class QgsMaskingWidget;
class QgsMapLayer;
class QgsMapCanvas;
class QgsRendererPropertiesDialog;
class QgsRendererRasterPropertiesWidget;
class QgsRendererMeshPropertiesWidget;
class QgsUndoWidget;
class QgsRasterHistogramWidget;
class QgsMapLayerStyleManagerWidget;
class QgsVectorLayer3DRendererWidget;
class QgsMeshLayer3DRendererWidget;
class QgsPointCloudLayer3DRendererWidget;
class QgsMessageBar;
class QgsVectorTileBasicRendererWidget;
class QgsVectorTileBasicLabelingWidget;

#include "qgslayerstylingwidget.h"
class APP_EXPORT QgsLayerStylingWidgetV2 : public QWidget, private Ui::QgsLayerStylingWidgetBase
{
    Q_OBJECT
  public:

    enum Page
    {
      Symbology = 1,
      VectorLabeling,
      RasterTransparency,
      RasterHistogram,
      History,
      Symbology3D,
    };

    QgsLayerStylingWidgetV2( QgsMapCanvas *canvas, QgsMessageBar *messageBar, const QList<QgsMapLayerConfigWidgetFactory *> &pages, QWidget *parent = nullptr );
    ~QgsLayerStylingWidgetV2() override;
    QgsMapLayer *layer() { return mCurrentLayer; }

    void setPageFactories( const QList<QgsMapLayerConfigWidgetFactory *> &factories );

    /**
     * Sets whether updates of the styling widget are blocked. This can be called to prevent
     * the widget being refreshed multiple times when a batch of layer style changes are
     * about to be applied
     * \param blocked set to TRUE to block updates, or FALSE to re-allow updates
     */
    void blockUpdates( bool blocked );

  signals:
    void styleChanged( QgsMapLayer *layer );

  public slots:
    void setLayer( QgsMapLayer *layer );
    void apply();
    void autoApply();
    void undo();
    void redo();
    void updateCurrentWidgetLayer();

    /**
     * Sets the current visible page in the widget.
     * \param page standard page to display
     */
    void setCurrentPage( QgsLayerStylingWidgetV2::Page page );

  private slots:

    void layerAboutToBeRemoved( QgsMapLayer *layer );
    void liveApplyToggled( bool value );

  private:
    void pushUndoItem( const QString &name, bool triggerRepaint = true );
    int mNotSupportedPage;
    int mLayerPage;
    QTimer *mAutoApplyTimer = nullptr;
    QDomNode mLastStyleXml;
    QgsMapCanvas *mMapCanvas = nullptr;
    QgsMessageBar *mMessageBar = nullptr;
    bool mBlockAutoApply;
    QgsUndoWidget *mUndoWidget = nullptr;
    QgsMapLayer *mCurrentLayer = nullptr;
    QgsLabelingWidget *mLabelingWidget = nullptr;
    QgsMaskingWidget *mMaskingWidget = nullptr;
#ifdef HAVE_3D
    QgsVectorLayer3DRendererWidget *mVector3DWidget = nullptr;
    QgsMeshLayer3DRendererWidget *mMesh3DWidget = nullptr;
#endif
    QgsRendererRasterPropertiesWidget *mRasterStyleWidget = nullptr;
    QgsRendererMeshPropertiesWidget *mMeshStyleWidget = nullptr;
    QgsVectorTileBasicRendererWidget *mVectorTileStyleWidget = nullptr;
    QgsVectorTileBasicLabelingWidget *mVectorTileLabelingWidget = nullptr;
    QList<QgsMapLayerConfigWidgetFactory *> mPageFactories;
    QMap<int, QgsMapLayerConfigWidgetFactory *> mUserPages;
    QgsLayerStyleManagerWidgetFactory *mStyleManagerFactory = nullptr;
};

#endif // QGSLAYERSTYLESDOCK_H
