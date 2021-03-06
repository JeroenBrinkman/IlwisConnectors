#include <QString>
#include <QUrl>
#include <QFileInfo>
#include <QScopedPointer>
#include <QSharedPointer>
#include <QDir>
#include <QVector>
#include "identity.h"
#include "kernel.h"
#include "inifile.h"
#include "resource.h"
#include "connectorinterface.h"
#include "mastercatalog.h"
#include "ilwisobjectconnector.h"
#include "ilwis3connector.h"
#include "catalogconnector.h"
#include "catalog.h"
#include "odfitem.h"
#include "ilwiscontext.h"
#include "filecatalogconnector.h"
#include "ilwis3catalogconnector.h"

using namespace Ilwis;
using namespace Ilwis3;

ConnectorInterface *Ilwis3CatalogConnector::create(const Resource &resource, bool) {
    return new Ilwis3CatalogConnector(resource);

}

Ilwis3CatalogConnector::Ilwis3CatalogConnector(const Resource &resource) : FileCatalogConnector(resource)
{
}

inline uint qHash(const QFileInfo& inf ){
    return ::qHash(inf.canonicalFilePath());
}

//bool operator==(const QFileInfo& inf1, const QFileInfo& inf2 ){
//    return inf1.canonicalFilePath() == inf2.canonicalFilePath();
//}

bool Ilwis3CatalogConnector::loadItems()
{
    QUrl location = _location.url();
    QStringList namefilter;
    namefilter << "*.mpr" << "*.mpa" << "*.mps" << "*.mpp" << "*.tbt" << "*.dom" << "*.rpr" << "*.csy" << "*.grf" << "*.mpl";

    QFileInfoList fileList = loadFolders(namefilter);

    // remove duplicates, shoudnt happen but better save than sorry
    QSet<QFileInfo> reduced = fileList.toSet();
    fileList.clear();
    fileList = QList<QFileInfo>::fromSet(reduced);

    QList<ODFItem> odfitems;
    QList<Resource> folders;
    QHash<QString, quint64> names;
    foreach(QFileInfo file, fileList) {
        QUrl container = location.url();
        QString path = file.canonicalFilePath();
        QString loc = container.toLocalFile();
        if ( path.compare(loc,Qt::CaseInsensitive) == 0)
            container = file.canonicalPath();
        IlwisTypes tp = Ilwis3Connector::ilwisType(path);
        QUrl url("file:///" + path);
        if ( mastercatalog()->resource2id(url, tp) == i64UNDEF) {
            if ( tp & itILWISOBJECT ) {
                ODFItem item(path);
                odfitems.push_back(item);
                names[file.fileName().toLower()] = item.id();
            } else {
                folders.push_back(loadFolder(file, container, path, url));
            }
        }

    }

    bool ok = true;


    QList<Resource> finalList;
    for(int i = 0; i < odfitems.size(); ++i) {
        ODFItem &item = odfitems[i];
        ok = item.resolveNames(names);


        if ( ok) {
            finalList.push_back(item);
        }
    }
    mastercatalog()->addItems(finalList);
    mastercatalog()->addItems(folders);


    return ok;
}

bool Ilwis3CatalogConnector::canUse(const QUrl &resource) const
{
    QString dn = resource.toString();
    if ( resource.scheme() != "file")
        return false;

    if ( resource.toString() == "file://") // root of file system
        return true;
    QFileInfo inf(resource.toLocalFile());
    if ( !inf.isDir())
        return false;

    if ( !inf.exists())
        return false;

    return true;
}

QString Ilwis3CatalogConnector::provider() const
{
    return "ilwis3";
}




