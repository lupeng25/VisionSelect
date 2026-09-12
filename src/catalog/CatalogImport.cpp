#include "catalog/CatalogRepository.h"
#include <QDateTime>
#include <QDir>
#include <QSet>
#include <QSqlError>
#include <QSqlQuery>
#include <QUuid>

namespace {
QString tableName(CatalogDomain domain) {
    switch (domain) {
    case CatalogDomain::Camera: return QStringLiteral("camera_products");
    case CatalogDomain::Lens: return QStringLiteral("lens_products");
    case CatalogDomain::Light: return QStringLiteral("light_products");
    }
    return {};
}
template<class T> QString identity(const T &spec) {
    return spec.manufacturer.trimmed().toLower() + QLatin1Char('\n') + spec.model.trimmed().toLower();
}
}

bool CatalogRepository::previewImport(CatalogDomain domain, const QString &path, CatalogImportMode mode,
    CatalogImportPreview *preview, QString *error)
{
    if (error) error->clear();
    if (!preview || !openDatabase(error)) return false;
    QVector<Row> rows;
    if (!readCsvRows(path, &rows, error)) return false;
    QStringList keys;
    const auto collect = [&keys](const auto &specs) { for (const auto &spec : specs) keys.append(identity(spec)); };
    if (domain == CatalogDomain::Camera) {
        QVector<CameraSpec> specs;
        if (!parseCameraSpecs(rows, path, &specs, error)) return false;
        collect(specs);
    } else if (domain == CatalogDomain::Lens) {
        QVector<LensSpec> specs;
        if (!parseLensSpecs(rows, path, &specs, error)) return false;
        collect(specs);
    } else {
        QVector<LightSpec> specs;
        if (!parseLightSpecs(rows, path, &specs, error)) return false;
        collect(specs);
    }
    const QSet<QString> incoming(keys.begin(), keys.end());
    if (incoming.size() != keys.size()) {
        if (error) *error = QString::fromUtf8("CSV 包含重复的厂家与型号，请先合并重复行。");
        return false;
    }
    QSqlQuery query(m_db);
    if (!query.exec(QStringLiteral("SELECT manufacturer_key,model_key FROM %1").arg(tableName(domain)))) {
        if (error) *error = query.lastError().text();
        return false;
    }
    QSet<QString> existing;
    while (query.next()) existing.insert(query.value(0).toString() + QLatin1Char('\n') + query.value(1).toString());
    *preview = {};
    for (const auto &key : incoming) {
        if (existing.contains(key)) ++preview->updated;
        else ++preview->added;
    }
    if (mode == CatalogImportMode::Replace)
        for (const auto &key : existing) if (!incoming.contains(key)) ++preview->removed;
    return true;
}

bool CatalogRepository::importCsv(CatalogDomain domain, const QString &path, CatalogImportMode mode,
    QString *backupPath, QString *error)
{
    CatalogImportPreview preview;
    if (!previewImport(domain, path, mode, &preview, error)) return false;
    QVector<Row> rows;
    QVector<CameraSpec> cameras;
    QVector<LensSpec> lenses;
    QVector<LightSpec> lights;
    if (!readCsvRows(path, &rows, error)) return false;
    if (domain == CatalogDomain::Camera && !parseCameraSpecs(rows, path, &cameras, error)) return false;
    if (domain == CatalogDomain::Lens && !parseLensSpecs(rows, path, &lenses, error)) return false;
    if (domain == CatalogDomain::Light && !parseLightSpecs(rows, path, &lights, error)) return false;

    const QString directory = QDir(storageDirectory()).filePath(QStringLiteral("backups"));
    if (!QDir().mkpath(directory)) {
        if (error) *error = QString::fromUtf8("无法创建目录备份，导入已取消。");
        return false;
    }
    const QString backup = QDir(directory).filePath(QStringLiteral("catalog-%1-%2.db")
        .arg(QDateTime::currentDateTimeUtc().toString("yyyyMMdd-HHmmss"), QUuid::createUuid().toString(QUuid::Id128)));
    QSqlQuery backupQuery(m_db);
    backupQuery.prepare(QStringLiteral("VACUUM INTO ?"));
    backupQuery.addBindValue(backup);
    if (!backupQuery.exec()) {
        if (error) *error = QString::fromUtf8("无法备份产品库，导入已取消：%1").arg(backupQuery.lastError().text());
        return false;
    }
    backupQuery.finish();
    if (backupPath) *backupPath = backup;
    if (!m_db.transaction()) { if (error) *error = m_db.lastError().text(); return false; }
    bool ok = true;
    if (mode == CatalogImportMode::Replace) {
        QSqlQuery remove(m_db);
        ok = remove.exec(QStringLiteral("DELETE FROM %1").arg(tableName(domain)));
        if (!ok && error) *error = remove.lastError().text();
    }
    for (const auto &camera : cameras) if (ok) ok = insertCameraIntoDatabase(camera, "local", true, nullptr, error);
    for (const auto &lens : lenses) if (ok) ok = insertLensIntoDatabase(lens, "local", true, nullptr, error);
    for (const auto &light : lights) if (ok) ok = insertLightIntoDatabase(light, "local", true, nullptr, error);
    if (!ok || !m_db.commit()) {
        if (ok && error) *error = m_db.lastError().text();
        m_db.rollback();
        return false;
    }
    m_lightCandidateCache.clear();
    return refreshSnapshotsIfLoaded(error);
}
