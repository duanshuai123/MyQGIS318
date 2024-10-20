/***************************************************************************
    qgssinglesymbolrenderer.cpp
    ---------------------
    begin                : November 2009
    copyright            : (C) 2009 by Martin Dobias
    email                : wonder dot sk at gmail dot com
 ***************************************************************************
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 ***************************************************************************/

#include "qgssinglesymbolrenderer.h"

#include "qgssymbol.h"
#include "qgssymbollayerutils.h"

#include "qgsdatadefinedsizelegend.h"
#include "qgslogger.h"
#include "qgsfeature.h"
#include "qgsvectorlayer.h"
#include "qgssymbollayer.h"
#include "qgsogcutils.h"
#include "qgspointdisplacementrenderer.h"
#include "qgsinvertedpolygonrenderer.h"
#include "qgspainteffect.h"
#include "qgspainteffectregistry.h"
#include "qgsproperty.h"
#include "qgsstyleentityvisitor.h"

#include <QDomDocument>
#include <QDomElement>

QgsSingleSymbolRenderer::QgsSingleSymbolRenderer( QgsSymbol *symbol )
  : QgsFeatureRenderer( QStringLiteral( "singleSymbol" ) )
  , mSymbol( symbol )
{
  Q_ASSERT( symbol );
}

QgsSingleSymbolRenderer::~QgsSingleSymbolRenderer() = default;

// @duanshuai
QgsSymbol *QgsSingleSymbolRenderer::symbolForFeature( const QgsFeature &feature, QgsRenderContext &context ) {
  // Q_UNUSED( context )
  QgsFields fields = feature.fields();
  static QString symbox_id_name = "symbol_id";
  static QString symbox_xml_name = "symbol_xml";
  int index1 = fields.indexFromName(symbox_id_name);
  int index2 = fields.indexFromName(symbox_xml_name);
  bool create_symbol = false;
  if (index1 >=0 ) {
    int symbol_id = feature.attribute(index1).toInt();
    // std::cerr << "symbol id:" << symbol_id << std::endl;
    QgsIdSymbolMap id_map = QgsStyle::GetSymbolFromDb();
    if (id_map.contains(symbol_id)) {
      QgsSymbol* symbol = id_map[symbol_id].symbol;
      create_symbol = true;
      if (!mUsedSymbols.contains(symbol)) {
        mUsedSymbols.insert(symbol);
        if (mFields.isEmpty()){
          symbol->startRender( context, fields );
        } else {
          symbol->startRender( context, mFields );
        }
      }
      current_symbol = symbol;
      return current_symbol;
    }
  }

  if (!create_symbol) {
    if (index2 > 0) {
      QString symbol_xml = feature.attribute(symbox_xml_name).toString();
      if (symbol_xml.isEmpty()) {
        current_symbol = mSymbol.get();
        return current_symbol;
      }
      QDomDocument doc;
      doc.setContent(symbol_xml);
      QDomElement symbol_dom = doc.documentElement();
      QgsSymbol* symbol = QgsSymbolLayerUtils::loadSymbol( symbol_dom, QgsReadWriteContext() );
      if(symbol == nullptr) {
        std::cerr << "fatal wrong : load symbol failed" << std::endl;
        current_symbol = mSymbol.get();
        return current_symbol;
      }
      if (mFields.isEmpty()){
        symbol->startRender( context, fields );
      } else {
        symbol->startRender( context, mFields );
      }
      mUsedSymbols.insert(symbol);
      current_symbol = symbol;
      return current_symbol;
    }
  }
  current_symbol = mSymbol.get();
  return current_symbol;
}


// @duanshuai
QgsSymbol *QgsSingleSymbolRenderer::originalSymbolForFeature( const QgsFeature &feature, QgsRenderContext &context )
{
  // Q_UNUSED( context )
  // Q_UNUSED( feature )
  QgsFields fields = feature.fields();
  static QString symbox_id_name = "symbol_id";
  static QString symbox_xml_name = "symbol_xml";
  int index1 = fields.indexFromName(symbox_id_name);
  int index2 = fields.indexFromName(symbox_xml_name);
  bool create_symbol = false;
  if (index1 >=0 ) {
    int symbol_id = feature.attribute(index1).toInt();
    // std::cerr << "symbol id:" << symbol_id << std::endl;
    QgsIdSymbolMap id_map = QgsStyle::GetSymbolFromDb();
    if (id_map.contains(symbol_id)) {
      QgsSymbol* symbol = id_map[symbol_id].symbol;
      create_symbol = true;
      if (!mUsedSymbols.contains(symbol)) {
        mUsedSymbols.insert(symbol);
        if (mFields.isEmpty()){
          symbol->startRender( context, fields );
        } else {
          symbol->startRender( context, mFields );
        }
      }
      current_symbol = symbol;
      return current_symbol;
    }
  }

  if (!create_symbol) {
    if (index2 > 0) {
      QString symbol_xml = feature.attribute(symbox_xml_name).toString();
      if(symbol_xml.isEmpty()) {
        current_symbol = mSymbol.get();
        return current_symbol;
      }
      QDomDocument doc;
      doc.setContent(symbol_xml);
      QDomElement symbol_dom = doc.documentElement();
      QgsSymbol* symbol = QgsSymbolLayerUtils::loadSymbol( symbol_dom, QgsReadWriteContext() );
      if (mFields.isEmpty()){
        symbol->startRender( context, fields );
      } else {
        symbol->startRender( context, mFields );
      }
      mUsedSymbols.insert(symbol);
      current_symbol = symbol;
      return current_symbol;
    }
  }
  current_symbol = mSymbol.get();
  return current_symbol;
}

// @duanshuai
void QgsSingleSymbolRenderer::startRender( QgsRenderContext &context, const QgsFields &fields )
{
  QgsFeatureRenderer::startRender( context, fields );
  //std::cerr << "QgsSingleSymbolRenderer::startRender" << std::endl;

  if ( !mSymbol )
    return;
  if (mFields.isEmpty()) {
    mFields = fields;
  }
  if (mUsedSymbols.isEmpty()) {
    mSymbol->startRender( context, fields );
  } else {
    for (QgsSymbol* symbol : mUsedSymbols) {
      symbol->startRender( context, fields );
    }
  }

  //mSymbol->startRender( context, fields );
}


// @duanshuai
void QgsSingleSymbolRenderer::stopRender( QgsRenderContext &context )
{
  QgsFeatureRenderer::stopRender( context );

  if ( !mSymbol )
    return;

  if (mUsedSymbols.isEmpty()) {
    mSymbol->stopRender( context);
  } else {
    for (QgsSymbol* symbol : mUsedSymbols) {
      symbol->stopRender( context);
    }
  }

  //mSymbol->stopRender( context );
}

QSet<QString> QgsSingleSymbolRenderer::usedAttributes( const QgsRenderContext &context ) const
{
  QSet<QString> attributes;
  if ( mSymbol )
    attributes.unite( mSymbol->usedAttributes( context ) );
  return attributes;
}

bool QgsSingleSymbolRenderer::accept( QgsStyleEntityVisitorInterface *visitor ) const
{
  if ( mSymbol )
  {
    QgsStyleSymbolEntity entity( mSymbol.get() );
    return visitor->visit( QgsStyleEntityVisitorInterface::StyleLeaf( &entity ) );
  }
  return true;
}

QgsSymbol *QgsSingleSymbolRenderer::symbol() const
{
  return mSymbol.get();
}

void QgsSingleSymbolRenderer::setSymbol( QgsSymbol *s )
{
  Q_ASSERT( s );
  mSymbol.reset( s );
}

QString QgsSingleSymbolRenderer::dump() const
{
  return mSymbol ? QStringLiteral( "SINGLE: %1" ).arg( mSymbol->dump() ) : QString();
}

QgsSingleSymbolRenderer *QgsSingleSymbolRenderer::clone() const
{
  QgsSingleSymbolRenderer *r = new QgsSingleSymbolRenderer( mSymbol->clone() );
  r->setUsingSymbolLevels( usingSymbolLevels() );
  r->setDataDefinedSizeLegend( mDataDefinedSizeLegend ? new QgsDataDefinedSizeLegend( *mDataDefinedSizeLegend ) : nullptr );
  copyRendererData( r );
  return r;
}

void QgsSingleSymbolRenderer::toSld( QDomDocument &doc, QDomElement &element, const QVariantMap &props ) const
{
  QVariantMap newProps = props;

  QDomElement ruleElem = doc.createElement( QStringLiteral( "se:Rule" ) );
  element.appendChild( ruleElem );

  QDomElement nameElem = doc.createElement( QStringLiteral( "se:Name" ) );
  nameElem.appendChild( doc.createTextNode( QStringLiteral( "Single symbol" ) ) );
  ruleElem.appendChild( nameElem );

  QgsSymbolLayerUtils::applyScaleDependency( doc, ruleElem, newProps );

  if ( mSymbol ) mSymbol->toSld( doc, ruleElem, newProps );
}

QgsSymbolList QgsSingleSymbolRenderer::symbols( QgsRenderContext &context ) const
{
  Q_UNUSED( context )
  QgsSymbolList lst;
  lst.append( mSymbol.get() );
  return lst;
}


QgsFeatureRenderer *QgsSingleSymbolRenderer::create( QDomElement &element, const QgsReadWriteContext &context )
{
  QDomElement symbolsElem = element.firstChildElement( QStringLiteral( "symbols" ) );
  if ( symbolsElem.isNull() )
    return nullptr;

  QgsSymbolMap symbolMap = QgsSymbolLayerUtils::loadSymbols( symbolsElem, context );

  if ( !symbolMap.contains( QStringLiteral( "0" ) ) )
    return nullptr;

  QgsSingleSymbolRenderer *r = new QgsSingleSymbolRenderer( symbolMap.take( QStringLiteral( "0" ) ) );

  // delete symbols if there are any more
  QgsSymbolLayerUtils::clearSymbolMap( symbolMap );

  QDomElement rotationElem = element.firstChildElement( QStringLiteral( "rotation" ) );
  if ( !rotationElem.isNull() && !rotationElem.attribute( QStringLiteral( "field" ) ).isEmpty() )
  {
    convertSymbolRotation( r->mSymbol.get(), rotationElem.attribute( QStringLiteral( "field" ) ) );
  }

  QDomElement sizeScaleElem = element.firstChildElement( QStringLiteral( "sizescale" ) );
  if ( !sizeScaleElem.isNull() && !sizeScaleElem.attribute( QStringLiteral( "field" ) ).isEmpty() )
  {
    convertSymbolSizeScale( r->mSymbol.get(),
                            QgsSymbolLayerUtils::decodeScaleMethod( sizeScaleElem.attribute( QStringLiteral( "scalemethod" ) ) ),
                            sizeScaleElem.attribute( QStringLiteral( "field" ) ) );
  }

  QDomElement ddsLegendSizeElem = element.firstChildElement( QStringLiteral( "data-defined-size-legend" ) );
  if ( !ddsLegendSizeElem.isNull() )
  {
    r->mDataDefinedSizeLegend.reset( QgsDataDefinedSizeLegend::readXml( ddsLegendSizeElem, context ) );
  }

  // TODO: symbol levels
  return r;
}

QgsFeatureRenderer *QgsSingleSymbolRenderer::createFromSld( QDomElement &element, QgsWkbTypes::GeometryType geomType )
{
  // XXX this renderer can handle only one Rule!

  // get the first Rule element
  QDomElement ruleElem = element.firstChildElement( QStringLiteral( "Rule" ) );
  if ( ruleElem.isNull() )
  {
    QgsDebugMsg( QStringLiteral( "no Rule elements found!" ) );
    return nullptr;
  }

  QString label, description;
  QgsSymbolLayerList layers;

  // retrieve the Rule element child nodes
  QDomElement childElem = ruleElem.firstChildElement();
  while ( !childElem.isNull() )
  {
    if ( childElem.localName() == QLatin1String( "Name" ) )
    {
      // <se:Name> tag contains the rule identifier,
      // so prefer title tag for the label property value
      if ( label.isEmpty() )
        label = childElem.firstChild().nodeValue();
    }
    else if ( childElem.localName() == QLatin1String( "Description" ) )
    {
      // <se:Description> can contains a title and an abstract
      QDomElement titleElem = childElem.firstChildElement( QStringLiteral( "Title" ) );
      if ( !titleElem.isNull() )
      {
        label = titleElem.firstChild().nodeValue();
      }

      QDomElement abstractElem = childElem.firstChildElement( QStringLiteral( "Abstract" ) );
      if ( !abstractElem.isNull() )
      {
        description = abstractElem.firstChild().nodeValue();
      }
    }
    else if ( childElem.localName() == QLatin1String( "Abstract" ) )
    {
      // <sld:Abstract> (v1.0)
      description = childElem.firstChild().nodeValue();
    }
    else if ( childElem.localName() == QLatin1String( "Title" ) )
    {
      // <sld:Title> (v1.0)
      label = childElem.firstChild().nodeValue();
    }
    else if ( childElem.localName().endsWith( QLatin1String( "Symbolizer" ) ) )
    {
      // create symbol layers for this symbolizer
      QgsSymbolLayerUtils::createSymbolLayerListFromSld( childElem, geomType, layers );
    }

    childElem = childElem.nextSiblingElement();
  }

  if ( layers.isEmpty() )
    return nullptr;

  // now create the symbol
  std::unique_ptr< QgsSymbol > symbol;
  switch ( geomType )
  {
    case QgsWkbTypes::LineGeometry:
      symbol = qgis::make_unique< QgsLineSymbol >( layers );
      break;

    case QgsWkbTypes::PolygonGeometry:
      symbol = qgis::make_unique< QgsFillSymbol >( layers );
      break;

    case QgsWkbTypes::PointGeometry:
      symbol = qgis::make_unique< QgsMarkerSymbol >( layers );
      break;

    default:
      QgsDebugMsg( QStringLiteral( "invalid geometry type: found %1" ).arg( geomType ) );
      return nullptr;
  }

  // and finally return the new renderer
  return new QgsSingleSymbolRenderer( symbol.release() );
}

QDomElement QgsSingleSymbolRenderer::save( QDomDocument &doc, const QgsReadWriteContext &context )
{
  QDomElement rendererElem = doc.createElement( RENDERER_TAG_NAME );
  rendererElem.setAttribute( QStringLiteral( "type" ), QStringLiteral( "singleSymbol" ) );
  rendererElem.setAttribute( QStringLiteral( "symbollevels" ), ( mUsingSymbolLevels ? QStringLiteral( "1" ) : QStringLiteral( "0" ) ) );
  rendererElem.setAttribute( QStringLiteral( "forceraster" ), ( mForceRaster ? QStringLiteral( "1" ) : QStringLiteral( "0" ) ) );

  QgsSymbolMap symbols;
  symbols[QStringLiteral( "0" )] = mSymbol.get();
  QDomElement symbolsElem = QgsSymbolLayerUtils::saveSymbols( symbols, QStringLiteral( "symbols" ), doc, context );
  rendererElem.appendChild( symbolsElem );

  QDomElement rotationElem = doc.createElement( QStringLiteral( "rotation" ) );
  rendererElem.appendChild( rotationElem );

  QDomElement sizeScaleElem = doc.createElement( QStringLiteral( "sizescale" ) );
  rendererElem.appendChild( sizeScaleElem );

  if ( mPaintEffect && !QgsPaintEffectRegistry::isDefaultStack( mPaintEffect ) )
    mPaintEffect->saveProperties( doc, rendererElem );

  if ( !mOrderBy.isEmpty() )
  {
    QDomElement orderBy = doc.createElement( QStringLiteral( "orderby" ) );
    mOrderBy.save( orderBy );
    rendererElem.appendChild( orderBy );
  }
  rendererElem.setAttribute( QStringLiteral( "enableorderby" ), ( mOrderByEnabled ? QStringLiteral( "1" ) : QStringLiteral( "0" ) ) );

  if ( mDataDefinedSizeLegend )
  {
    QDomElement ddsLegendElem = doc.createElement( QStringLiteral( "data-defined-size-legend" ) );
    mDataDefinedSizeLegend->writeXml( ddsLegendElem, context );
    rendererElem.appendChild( ddsLegendElem );
  }

  return rendererElem;
}

QgsLegendSymbolList QgsSingleSymbolRenderer::legendSymbolItems() const
{
  if ( mDataDefinedSizeLegend && mSymbol->type() == QgsSymbol::Marker )
  {
    const QgsMarkerSymbol *symbol = static_cast<const QgsMarkerSymbol *>( mSymbol.get() );
    QgsProperty sizeDD( symbol->dataDefinedSize() );
    if ( sizeDD && sizeDD.isActive() )
    {
      QgsDataDefinedSizeLegend ddSizeLegend( *mDataDefinedSizeLegend );
      ddSizeLegend.updateFromSymbolAndProperty( static_cast<const QgsMarkerSymbol *>( mSymbol.get() ), sizeDD );
      return ddSizeLegend.legendSymbolList();
    }
  }

  QgsLegendSymbolList lst;
  lst << QgsLegendSymbolItem( mSymbol.get(), QString(), QStringLiteral( "0" ) );
  return lst;
}

QSet< QString > QgsSingleSymbolRenderer::legendKeysForFeature( const QgsFeature &feature, QgsRenderContext &context ) const
{
  Q_UNUSED( feature )
  Q_UNUSED( context )
  return QSet< QString >() << QStringLiteral( "0" );
}

void QgsSingleSymbolRenderer::setLegendSymbolItem( const QString &key, QgsSymbol *symbol )
{
  Q_UNUSED( key )
  setSymbol( symbol );
}

QgsSingleSymbolRenderer *QgsSingleSymbolRenderer::convertFromRenderer( const QgsFeatureRenderer *renderer )
{
  QgsSingleSymbolRenderer *r = nullptr;
  if ( renderer->type() == QLatin1String( "singleSymbol" ) )
  {
    r = dynamic_cast<QgsSingleSymbolRenderer *>( renderer->clone() );
  }
  else if ( renderer->type() == QLatin1String( "pointDisplacement" ) || renderer->type() == QLatin1String( "pointCluster" ) )
  {
    const QgsPointDistanceRenderer *pointDistanceRenderer = dynamic_cast<const QgsPointDistanceRenderer *>( renderer );
    if ( pointDistanceRenderer )
      r = convertFromRenderer( pointDistanceRenderer->embeddedRenderer() );
  }
  else if ( renderer->type() == QLatin1String( "invertedPolygonRenderer" ) )
  {
    const QgsInvertedPolygonRenderer *invertedPolygonRenderer = dynamic_cast<const QgsInvertedPolygonRenderer *>( renderer );
    if ( invertedPolygonRenderer )
      r = convertFromRenderer( invertedPolygonRenderer->embeddedRenderer() );
  }

  if ( !r )
  {
    QgsRenderContext context;
    QgsSymbolList symbols = const_cast<QgsFeatureRenderer *>( renderer )->symbols( context );
    if ( !symbols.isEmpty() )
    {
      r = new QgsSingleSymbolRenderer( symbols.at( 0 )->clone() );
    }
  }

  if ( r )
  {
    r->setOrderBy( renderer->orderBy() );
    r->setOrderByEnabled( renderer->orderByEnabled() );
  }

  return r;
}

void QgsSingleSymbolRenderer::setDataDefinedSizeLegend( QgsDataDefinedSizeLegend *settings )
{
  mDataDefinedSizeLegend.reset( settings );
}

QgsDataDefinedSizeLegend *QgsSingleSymbolRenderer::dataDefinedSizeLegend() const
{
  return mDataDefinedSizeLegend.get();
}
