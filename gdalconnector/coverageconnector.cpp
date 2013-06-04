#include <QLibrary>
#include <QString>
#include <QSqlQuery>
#include <QSqlError>
#include <QSqlRecord>

#include "kernel.h"
#include "coverage.h"
#include "module.h"
#include "ilwisdata.h"
#include "numericdomain.h"
#include "numericrange.h"
#include "columndefinition.h"
#include "table.h"
#include "ilwisobjectconnector.h"
#include "gdalproxy.h"
#include "gdalconnector.h"
#include "coverageconnector.h"



using namespace Ilwis;
using namespace Gdal;

CoverageConnector::CoverageConnector(const Resource& item,bool load) : GdalConnector(item,load)
{
}

bool CoverageConnector::loadMetaData(Ilwis::IlwisObject *data)
{
    GdalConnector::loadMetaData(data);

    Coverage *coverage = static_cast<Coverage *>(data);

    ICoordinateSystem csy = setObject<ICoordinateSystem>("coordinatesystem", _filename);
    if(!csy.isValid()) {
        return ERROR2(ERR_COULDNT_CREATE_OBJECT_FOR_2, "coordinatesystem", coverage->name());
    }

    IDomain dom;
    if(!dom.prepare("code=value")) { //TODO  for the moment only value maps in gdal
        return ERROR1(ERR_NO_INITIALIZED_1,data->name());
    }
    coverage->datadef().domain(dom);

    double geosys[6];
    CPLErr err = gdal()->getGeotransform(_dataSet, geosys) ;
    if ( err != CE_None) {
        return ERROR2(ERR_INVALID_PROPERTY_FOR_2, "Bounds", coverage->name());
    }

    double a1 = geosys[0];
    double b1 = geosys[3];
    double a2 = geosys[1];
    double b2 = geosys[5];
    Pixel pix(gdal()->xsize(_dataSet), gdal()->ysize(_dataSet));
    Coordinate crdLeftup( a1 , b1);
    Coordinate crdRightDown(a1 + pix.x() * a2, b1 + pix.y() * b2 ) ;
    Coordinate cMin( min(crdLeftup.x(), crdRightDown.x()), min(crdLeftup.y(), crdRightDown.y()));
    Coordinate cMax( max(crdLeftup.x(), crdRightDown.x()), max(crdLeftup.y(), crdRightDown.y()));

    coverage->setEnvelope(Box2D<double>(cMin, cMax));

    return true;
}

