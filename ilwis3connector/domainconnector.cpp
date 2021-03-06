#include "kernel.h"
#include "angle.h"
#include "point.h"
#include "connectorinterface.h"
#include "inifile.h"
#include "ilwisdata.h"
#include "domainitem.h"
#include "domain.h"
#include "itemdomain.h"
#include "identifieritem.h"
#include "thematicitem.h"
#include "numericrange.h"
#include "itemrange.h"
#include "identifierrange.h"
#include "rawconverter.h"
#include "ilwisobjectconnector.h"
#include "ilwis3connector.h"
#include "numericdomain.h"
#include "datadefinition.h"
#include "binaryilwis3table.h"
#include "ilwis3range.h"
#include "textdomain.h"
#include "domainconnector.h"

using namespace Ilwis;
using namespace Ilwis3;

ConnectorInterface *DomainConnector::create(const Resource& resource, bool load) {
    return new DomainConnector(resource, load);

}

DomainConnector::DomainConnector(const Resource& resource, bool load) : Ilwis3Connector(resource, load)
{
}

bool DomainConnector::loadMetaData(IlwisObject* data)
{
    Ilwis3Connector::loadMetaData(data);

    if (type() == itUNKNOWN) {
        kernel()->issues()->log(TR(ERR_INVALID_PROPERTY_FOR_2).arg("Domain type",_resource.name()));
        return false;
    }
    if ( type() == itNUMERICDOMAIN) {
        return handleValueDomains(data);
    } else if ( type() == itITEMDOMAIN || type() == itDOMAIN) {
        return handleItemDomains(data);
    }

    return false;
}

bool DomainConnector::handleIdDomain(IlwisObject* data) {
    ItemDomain<IndexedIdentifier> *iddomain = static_cast<ItemDomain<IndexedIdentifier> *>(data);
    bool ok;
    quint32 nritems = _odf->value("DomainIdentifier","Nr").toLong(&ok);
    if ( !ok) {
        return ERROR2(ERR_INVALID_PROPERTY_FOR_2,"domain items", data->name());
    }
    QString prefix = _odf->value("DomainSort","Prefix");
    iddomain->addItem(new IndexedIdentifier(prefix,0, nritems));

    return true;
}

bool DomainConnector::handleItemDomains(IlwisObject* data) {
    QString domtype = _odf->value("Domain","Type");
    bool hasDataFile = _odf->value("TableStore","Col1") == "Ord";
    if ( (domtype == "DomainUniqueID" || domtype ==  "DomainIdentifier") && !hasDataFile)
    { // no table found? internal domain
        return handleIdDomain(data);
    }
    Ilwis3::BinaryIlwis3Table tbl ;
    tbl.load(_odf);
    quint32 indexName = tbl.index("Name");
    if (indexName == iUNDEF) { // no name column in the table ?
        kernel()->issues()->log(TR(ERR_COLUMN_MISSING_2).arg("Name",_odf->fileinfo().baseName()));
        return false;
    }
    quint32 indexCode = tbl.index("Code"); // not mandatory
    ItemDomain<ThematicItem> *tdomain = static_cast<ItemDomain<ThematicItem> *>(data);
    QString itemName, itemCode = sUNDEF;
    for(quint32 i = 0; i < tbl.rows(); ++i) {
        tbl.get(i,indexName,itemName);
        if ( indexCode != iUNDEF)
            tbl.get(i,indexName,itemCode);
        ThematicItem *item = new ThematicItem({itemName,itemCode});
        tdomain->addItem(item);
    }

    return true;
}


bool DomainConnector::handleValueDomains(IlwisObject* data) {
    NumericDomain *vdata = static_cast<NumericDomain*>(data);
    IlwisTypes odfType = _resource.ilwisType();
    Range *range = 0;
    if ( (odfType & itDOMAIN) != 0){ // domain objects
        range = handleValueDomainObjects();
    } else {
        QString section;
        if ( odfType <= itRASTER) { //  the 'basemaps' objects
            section = "BaseMap"    ;
        } else if (odfType == itTABLE) {
            QUrlQuery queryItem(_resource.url());
            QString colName = queryItem.queryItemValue("column");
            section = QString("Col:%1").arg(colName);
        }
        if ( section != "") {
            range = Ilwis3Range::findRange(_odf, section);

        } else {
            kernel()->issues()->log(TR("Illegal type %1 for domain").arg(type()));
            return false;
        }
    }
    // TODO columns domain handling, no seperate odf, so special type(??)

    if (!range) {
        kernel()->issues()->log(TR(ERR_NO_INITIALIZED_1).arg(data->name()));
        return false;
    }
    vdata->setRange(range);


    return true;
}

Range * DomainConnector::handleValueDomainObjects() const {
    bool isOk = true;
    double vmin, vmax;
    QString type = _odf->value("DomainValue", "Type");
    vmin = _odf->value(type, "Min").toDouble(&isOk);
    if (!isOk) {
        kernel()->issues()->log(TR(ERR_INVALID_PROPERTY_FOR_2).arg("Minimum", _resource.name()));
        return 0;
    }

    vmax = _odf->value(type ,"Max").toDouble(&isOk);
    if (!isOk) {
        kernel()->issues()->log(TR(ERR_INVALID_PROPERTY_FOR_2).arg("Maximum", _resource.name()));
        return 0;
    }
    double resolution = _odf->value(type , "Step").toDouble(&isOk);
    if (!isOk || fmod(resolution , 1.0) == 0)
        return new NumericRange(vmin, vmax,1);
    return new NumericRange(vmin, vmax, resolution);

}


QString DomainConnector::parseDomainInfo(const QString& inf) const{
    if ( inf != sUNDEF) {
        QStringList parts = inf.split(";");
        if ( parts.size() > 3) {
            QString dmtype = parts[2];
            return dmtype.toLower();
        }
    }
    return sUNDEF;
}

bool DomainConnector::storeMetaDataSortDomain(Domain *dom, IlwisTypes tp) {
    ItemDomain<NamedIdentifier> *piddomain = static_cast< ItemDomain<NamedIdentifier> *>(dom);
    INamedIdDomain iddomain;
    iddomain.set(piddomain);

    auto writeColumnFunc = [&] (const QString& name, const QString& domName, const QString& domInfo, const QString& rng, const QString& storeType) -> void {
        _odf->setKeyValue(name, "Time", Time::now().toString());
        _odf->setKeyValue(name, "Version", "3.1");
        _odf->setKeyValue(name, "Class", "Column");
        _odf->setKeyValue(name, "Domain", domName);
        _odf->setKeyValue(name, "DomainInfo", domInfo);
        if ( rng != sUNDEF)
            _odf->setKeyValue(name, "Range", rng);
        _odf->setKeyValue(name, "ReadOnly", "No");
        _odf->setKeyValue(name, "OwnedByTable", "No");
        _odf->setKeyValue(name, "Type", "ColumnStore");
        _odf->setKeyValue(name, "StoreType", storeType);

    };

    _odf->setKeyValue("Table", "Time", Time::now().toString());
    _odf->setKeyValue("Table","Version","3.1");
    _odf->setKeyValue("Table","Class","Table");
    _odf->setKeyValue("Table","Domain","String.dom");
    _odf->setKeyValue("Table","Type","TableStore");
    _odf->setKeyValue("Table","DomainInfo", "String.dom;String;string;0;;");
    _odf->setKeyValue("Table","Columns",tp == itTHEMATICITEM ? "5" : "3");
    _odf->setKeyValue("Table","Records", QString::number(iddomain->count()));
    _odf->setKeyValue("DomainSort","Prefix", "");
    _odf->setKeyValue("DomainSort","Sorting","Alphabetical");
    _odf->setKeyValue("Domain", "Type", tp == itTHEMATICITEM ? "DomainSort" : "DomainIdentifier");
    _odf->setKeyValue( tp == itTHEMATICITEM ? "DomainClass" : "DomainIdentifier", "Nr", QString::number(iddomain->count()));

    QFileInfo inf(dom->name());
    QString dataName  = inf.baseName() + ".dm#";
    _odf->setKeyValue("TableStore", "Data", dataName);
    _odf->setKeyValue("TableStore", "Col0", "Name");
    _odf->setKeyValue("TableStore", "Col1", "Ord");
    _odf->setKeyValue("TableStore", "Col2", "Ind");
    _odf->setKeyValue("TableStore", "Type", "TableBinary");
    if (tp == itTHEMATICITEM) {
        _odf->setKeyValue("TableStore", "Col3", "Code");
        _odf->setKeyValue("TableStore", "Col4", "Description");
    }

    writeColumnFunc("Col:Name","String.dom","String.dom;String;string;0;;", sUNDEF, "String");
    writeColumnFunc("Col:Ord","value.dom","value.dom;Long;value;0;-9999999.9:9999999.9:0.1:offset=0;", "-32766:32767:offset=0","Int");
    writeColumnFunc("Col:Ind","value.dom","value.dom;Long;value;0;-9999999.9:9999999.9:0.1:offset=0;", "-32766:32767:offset=0","Int");
    if ( tp == itTHEMATICITEM) {
        writeColumnFunc("Col:Code","String.dom","String.dom;String;string;0;;", sUNDEF, "String");
        writeColumnFunc("Col:Description","String.dom","String.dom;String;string;0;;", sUNDEF, "String");
    }

    BinaryIlwis3Table ilw3tbl;
    std::ofstream output_file;
    if(!ilw3tbl.openOutput(dom->name()  + ".dm#", output_file))
        return false;
    ITextDomain txtdom;
    txtdom.prepare();
    DataDefinition deftxt(txtdom);
    INumericDomain numdom;
    numdom.prepare("integer");
    DataDefinition deford(numdom);
    deford.range(new NumericRange(-32766,32767,1));
    DataDefinition defind(numdom);
    defind.range(new NumericRange(-32766,32767,1));

    ilw3tbl.addStoreDefinition(deftxt);
    ilw3tbl.addStoreDefinition(deford);
    ilw3tbl.addStoreDefinition(defind);
    if ( tp == itTHEMATICITEM) {
        ilw3tbl.addStoreDefinition(deftxt);
        ilw3tbl.addStoreDefinition(deftxt);
    }

    for(SPDomainItem item : iddomain){
        std::vector<QVariant> record(tp == itTHEMATICITEM ? 5 : 3);
        record[0] = item->name();
        record[1] = item->raw() + 1;
        record[2] = item->raw() + 1;
        if ( tp == itTHEMATICITEM) {
            SPThematicItem thematicItem = item.staticCast<ThematicItem>();
            record[3] = thematicItem->code();
            record[4] = thematicItem->description();
        }
        ilw3tbl.storeRecord(output_file, record);
    }

    output_file.close();

    return true;

}

bool DomainConnector::storeMetaData(IlwisObject *data)
{
    Domain *dom = static_cast<Domain *>(data);
    QString dmName = dom->name();
    QString alias = kernel()->database().findAlias(dmName,"domain","ilwis3");
    if ( alias != sUNDEF)
        return true; // nothing to be done, already exists as a system domain
    Ilwis3Connector::storeMetaData(data, itDOMAIN);

    _odf->setKeyValue("Ilwis", "Type", "Domain");
    if ( dom->ilwisType() == itNUMERICDOMAIN) {
        SPNumericRange numRange = dom->range<NumericRange>();
        int width=12;
        QString type = "DomainValueInt";
        if ( dom->valueType() & (itINT8 & itUINT8)){
            width=3;
        } else if ( dom->valueType() & (itINT16 & itUINT16) ){
            width = 8;
        } else if ( dom->valueType() & (itINT32 & itUINT32) ){
            width=10;
        } else
            type = "DomainValueReal";

        _odf->setKeyValue("Domain", "Type", "DomainValue");
        _odf->setKeyValue("Domain", "Width", QString::number(width));
        _odf->setKeyValue("DomainValue", "Type", type);
        _odf->setKeyValue(type, "Min", QString::number(numRange->min()));
        _odf->setKeyValue(type, "Max", QString::number(numRange->max()));
        if ( numRange->step() != 1) {
            _odf->setKeyValue(type, "Step", QString::number(numRange->step()));
        }
    } else if ( dom->valueType() == itTHEMATICITEM) {
        storeMetaDataSortDomain(dom, itTHEMATICITEM);
    } else if ( dom->valueType() & itIDENTIFIERITEM) {
        storeMetaDataSortDomain(dom, itIDENTIFIERITEM);
    } else if ( dom->ilwisType() == itTEXTDOMAIN) {
    } else if ( dom->ilwisType() == itCOLORDOMAIN) {
    } else if ( dom->ilwisType() == itTIMEDOMAIN) {
    } else if ( dom->ilwisType() == itCOORDDOMAIN) {
    }

    _odf->store();
    return true;
}

IlwisObject *DomainConnector::create() const
{
    //TODO other domain types time, coordinatesystem
    QString subtype = sUNDEF;
    if ( type() & itCOVERAGE) {
        subtype = parseDomainInfo( _odf->value("BaseMap","DomainInfo"));

    } else if( type() & itTABLE) {
        QUrlQuery queryItem(_resource.url());
        QString colName = queryItem.queryItemValue("column");
        if ( colName != sUNDEF) {
            subtype = parseDomainInfo( _odf->value(QString("Col:%1").arg(colName),"DomainInfo"));
        }

    }

    if ( type() == itNUMERICDOMAIN)
        return new NumericDomain(_resource);
    else if (type() == itITEMDOMAIN || type() == itDOMAIN) { // second case is for internal domains
        subtype =_odf->value("Domain", "Type");
        if ( subtype == "DomainIdentifier" || subtype == "DomainUniqueID")
            return new ItemDomain<IndexedIdentifier>(_resource);
        if ( subtype == "DomainClass")
            return new ItemDomain<ThematicItem>(_resource);
    }
    return 0;
}
