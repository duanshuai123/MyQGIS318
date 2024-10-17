
#ifndef QGSMAPTOOLSYMBOLEDIT_H
#define QGSMAPTOOLSYMBOLEDIT_H

#include "qgsmaptoolselect.h"

class APP_EXPORT QgsMapToolSymbolEdit : public QgsMapToolSelect
{
    Q_OBJECT
  public:
    QgsMapToolSymbolEdit( QgsMapCanvas *canvas );
    void canvasReleaseEvent( QgsMapMouseEvent *e ) override;
};

#endif
