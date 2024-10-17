#include "qgsmaptoolsymboledit.h"
#include "qgsfeature.h"
#include "qgsmapcanvas.h"
#include "qgsvectorlayer.h"
#include "qgsrenderer.h"
#include "qgssinglesymbolrenderer.h"
#include "qgsstyle.h"
#include "qgisapp.h"
#include "qgslayerstylingwidgetV2.h"
#include <QDialog>
#include <QVBoxLayout>
#include "qgsvectorlayerdigitizingproperties.h"
QgsMapToolSymbolEdit::QgsMapToolSymbolEdit( QgsMapCanvas *canvas ) 
  : QgsMapToolSelect(canvas) {

}

void QgsMapToolSymbolEdit::canvasReleaseEvent( QgsMapMouseEvent *e ) {
  QgsMapToolSelect::canvasReleaseEvent(e);
  QgsVectorLayer* pCurrentLayer = qobject_cast<QgsVectorLayer*>( mCanvas->currentLayer());
  if (!pCurrentLayer || pCurrentLayer->selectedFeatureCount() == 0)
    return;
  
  QgsFeatureIds selectedIds = pCurrentLayer->selectedFeatureIds();
  QgsFeatureId fid = *selectedIds.begin();
  QgsFeature feature = pCurrentLayer->getFeature(fid);
  if (!feature.isValid())
    return;
  QgsFields fields = feature.fields();

  QgsSymbol* current_symbol = nullptr; 
  static QString symbox_id_name = "symbol_id";
  static QString symbox_xml_name = "symbol_xml";
  int index1 = fields.indexFromName(symbox_id_name);
  int index2 = fields.indexFromName(symbox_xml_name);
  if (index1 >=0 ) {
    int symbol_id = feature.attribute(index1).toInt();
    // std::cerr << "symbol id:" << symbol_id << std::endl;
    QgsIdSymbolMap id_map = QgsStyle::GetSymbolFromDb();
    if (id_map.contains(symbol_id)) {
      QgsSymbol* symbol = id_map[symbol_id];
      current_symbol = symbol;
    }
  }

  while (current_symbol == nullptr) {
    if (index2 > 0) {
      QString symbol_xml = feature.attribute(symbox_xml_name).toString();
      if(symbol_xml.isEmpty()) {
        break;
      }
      QDomDocument doc;
      doc.setContent(symbol_xml);
      QDomElement symbol_dom = doc.documentElement();
      QgsSymbol* symbol = QgsSymbolLayerUtils::loadSymbol( symbol_dom, QgsReadWriteContext() );
      current_symbol = symbol;
    }
    break;
  };

  if (current_symbol == nullptr) {
    QgsFeatureRenderer* r = pCurrentLayer->renderer();
    QgsSingleSymbolRenderer* pSingleRender = dynamic_cast<QgsSingleSymbolRenderer*>(r);
    if (pSingleRender) {
       current_symbol = pSingleRender->symbol();
    }
  }
  // just edit current_symbol here
  std::cerr << "Symbol edit release canvas: " << current_symbol << std::endl;

  QList<QgsMapLayerConfigWidgetFactory*> list_factory;
  list_factory << new QgsVectorLayerDigitizingPropertiesFactory(this) ;
  // list_factory << new QgsPointCloudRendererWidgetFactory(this) ;
  // list_factory << new QgsPointCloudElevationPropertiesWidgetFactory(this) ;

  QgsLayerStylingWidgetV2* widget = new QgsLayerStylingWidgetV2(mCanvas, QgisApp::instance()->messageBar(), list_factory);
  widget->setLayer(pCurrentLayer);

  QDialog dialog;
  QVBoxLayout* mainLayout = new QVBoxLayout(&dialog);
  mainLayout->addWidget(widget);
  dialog.exec();
}

