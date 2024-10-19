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
#include <QDialogButtonBox>
#include <QVBoxLayout>
#include <QDomDocument>
#include <QByteArray>
#include <QTextStream>
#include <QMessageBox>
#include "qgssymbollayerutils.h"
#include "qgsreadwritecontext.h"
#include "qgsvectorlayerdigitizingproperties.h"
#include "qgssinglesymbolrendererwidget.h"
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
      QgsSymbol* symbol = id_map[symbol_id].symbol;
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

  // QList<QgsMapLayerConfigWidgetFactory*> list_factory;
  // list_factory << new QgsVectorLayerDigitizingPropertiesFactory(this) ;
  // list_factory << new QgsPointCloudRendererWidgetFactory(this) ;
  // list_factory << new QgsPointCloudElevationPropertiesWidgetFactory(this) ;
  // QgsLayerStylingWidgetV2* widget = new QgsLayerStylingWidgetV2(mCanvas, QgisApp::instance()->messageBar(), list_factory);
  // widget->setLayer(pCurrentLayer);

  QgsSingleSymbolRendererWidget* widget = new QgsSingleSymbolRendererWidget(pCurrentLayer, QgsStyle::defaultStyle(), current_symbol->clone());
  QDialog dialog;
  QVBoxLayout* mainLayout = new QVBoxLayout(&dialog);
  mainLayout->addWidget(widget);

  QDialogButtonBox* buttonBox = new QDialogButtonBox(&dialog);
  buttonBox->setObjectName(QString::fromUtf8("buttonBox"));
  buttonBox->setGeometry(QRect(20, 270, 341, 32));
  buttonBox->setOrientation(Qt::Horizontal);
  buttonBox->setStandardButtons(QDialogButtonBox::Cancel|QDialogButtonBox::Ok);
  QObject::connect(buttonBox, SIGNAL(accepted()), &dialog, SLOT(accept()));
  QObject::connect(buttonBox, SIGNAL(rejected()), &dialog, SLOT(reject()));
  mainLayout->addWidget(buttonBox);
  dialog.setMinimumSize(500, 800);
  if (dialog.exec() != QDialog::Accepted)
    return;
  QgsFeatureRenderer* r = widget->renderer();
  QgsSingleSymbolRenderer* pSingleRender = dynamic_cast<QgsSingleSymbolRenderer*>(r);
  auto newSymbol = pSingleRender->symbol();
  std::cerr << "new symbol:" << newSymbol << std::endl;

  // TODO add support for groups
  QDomDocument doc( QStringLiteral( "dummy" ) );
  QDomElement symEl = QgsSymbolLayerUtils::saveSymbol( "", newSymbol, doc, QgsReadWriteContext() );
  if (symEl.isNull())
    return ;

  QByteArray xmlArray;
  QTextStream stream( &xmlArray );
  stream.setCodec( "UTF-8" );
  symEl.save( stream, 4 );

  std::hash<std::string> str_hash;
  std::string data = xmlArray.constData();
  size_t hash_key = str_hash( data );
  std::string str_hash_key = std::to_string(hash_key);
  QgsIdSymbolMap id_map2 = QgsStyle::GetSymbolFromDb();
  int symbol_id = -1;
  for(auto iter = id_map2.begin(); iter != id_map2.end(); iter++) {
    std::cerr<< "iter key:" << iter.key() << " value:" << iter.value().hash_key << std::endl;
    if (iter.value().hash_key == str_hash_key) {
      std::cerr << "symbol already exists" << std::endl;
      symbol_id = iter.key();
      break;
    }
  }
  pCurrentLayer->startEditing();
  if (symbol_id >  -1) {
    pCurrentLayer->changeAttributeValue(fid, index1, symbol_id);
  } else {
    pCurrentLayer->changeAttributeValue(fid, index1, "");
    pCurrentLayer->changeAttributeValue(fid, index2, QString::fromStdString(data));
  }
  pCurrentLayer->commitChanges();
  // QgsStyle::AddSymbolToDb(hash_key, newSymbol);
}

