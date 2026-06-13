#include "catalog/CatalogRepository.h"

#include <QDir>
#include <QFile>
#include <QFileInfo>
#include <QIODevice>
#include <QRegularExpression>
#include <QStandardPaths>
#include <QSet>
#include <QTextStream>

void CatalogRepository::setStorageDirectory(const QString &directory)
{
    m_storageDirectory = directory;
}

QString CatalogRepository::storageDirectory() const
{
    return effectiveStorageDirectory();
}

bool CatalogRepository::loadDefaults(QString *errorMessage)
{
    if (!ensureLocalCatalogs(errorMessage))
        return false;

    QVector<Row> cameraRows;
    QVector<Row> lensRows;
    QVector<Row> lightRows;

    if (!readCsvRows(cameraStoragePath(), &cameraRows, errorMessage))
        return false;
    if (!readCsvRows(lensStoragePath(), &lensRows, errorMessage))
        return false;
    if (!readCsvRows(lightStoragePath(), &lightRows, errorMessage))
        return false;

    return loadCameraRows(cameraRows, QString::fromUtf8("\345\206\205\347\275\256\347\233\270\346\234\272\345\272\223"), errorMessage)
        && loadLensRows(lensRows, QString::fromUtf8("\345\206\205\347\275\256\351\225\234\345\244\264\345\272\223"), errorMessage)
        && loadLightRows(lightRows, QString::fromUtf8("\345\206\205\347\275\256\345\205\211\346\272\220\345\272\223"), errorMessage);
}

QString CatalogRepository::effectiveStorageDirectory() const
{
    if (!m_storageDirectory.trimmed().isEmpty())
        return m_storageDirectory;
    QString path = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
    if (path.isEmpty())
        path = QDir::homePath() + QStringLiteral("/VisionSelect");
    return path;
}

QString CatalogRepository::cameraStoragePath() const
{
    return QDir(effectiveStorageDirectory()).filePath(QStringLiteral("cameras.csv"));
}

QString CatalogRepository::lensStoragePath() const
{
    return QDir(effectiveStorageDirectory()).filePath(QStringLiteral("lenses.csv"));
}

QString CatalogRepository::lightStoragePath() const
{
    return QDir(effectiveStorageDirectory()).filePath(QStringLiteral("lights.csv"));
}

bool CatalogRepository::ensureLocalCatalogs(QString *errorMessage)
{
    QDir dir(effectiveStorageDirectory());
    if (!dir.exists() && !QDir().mkpath(dir.absolutePath())) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8("\346\227\240\346\263\225\345\210\233\345\273\272\344\272\247\345\223\201\345\272\223\346\225\260\346\215\256\347\233\256\345\275\225\357\274\232%1").arg(dir.absolutePath());
        return false;
    }
    if (!QFileInfo::exists(cameraStoragePath())
        && !copyResourceToFile(QStringLiteral(":/data/cameras.csv"), cameraStoragePath(), false, errorMessage)) {
        return false;
    }
    if (!QFileInfo::exists(lensStoragePath())
        && !copyResourceToFile(QStringLiteral(":/data/lenses.csv"), lensStoragePath(), false, errorMessage)) {
        return false;
    }
    if (!QFileInfo::exists(lightStoragePath())
        && !copyResourceToFile(QStringLiteral(":/data/lights.csv"), lightStoragePath(), false, errorMessage)) {
        return false;
    }
    return appendMissingResourceRows(QStringLiteral(":/data/cameras.csv"), cameraStoragePath(), QStringLiteral("model"), errorMessage)
        && appendMissingResourceRows(QStringLiteral(":/data/lenses.csv"), lensStoragePath(), QStringLiteral("model"), errorMessage)
        && appendMissingResourceRows(QStringLiteral(":/data/lights.csv"), lightStoragePath(), QStringLiteral("model"), errorMessage);
}

bool CatalogRepository::copyResourceToFile(const QString &resourcePath, const QString &filePath, bool overwrite, QString *errorMessage) const
{
    QFileInfo targetInfo(filePath);
    if (!QDir().mkpath(targetInfo.absolutePath())) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8("\346\227\240\346\263\225\345\210\233\345\273\272\347\233\256\345\275\225\357\274\232%1").arg(targetInfo.absolutePath());
        return false;
    }
    if (overwrite && QFileInfo::exists(filePath) && !QFile::remove(filePath)) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8("\346\227\240\346\263\225\350\246\206\347\233\226\346\226\207\344\273\266\357\274\232%1").arg(filePath);
        return false;
    }
    if (!overwrite && QFileInfo::exists(filePath))
        return true;
    if (!QFile::copy(resourcePath, filePath)) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8("\346\227\240\346\263\225\345\210\235\345\247\213\345\214\226\344\272\247\345\223\201\345\272\223\346\226\207\344\273\266\357\274\232%1").arg(filePath);
        return false;
    }
    QFile::setPermissions(filePath,
                          QFileDevice::ReadOwner | QFileDevice::WriteOwner
                              | QFileDevice::ReadUser | QFileDevice::WriteUser
                              | QFileDevice::ReadGroup | QFileDevice::ReadOther);
    return true;
}

bool CatalogRepository::appendMissingResourceRows(const QString &resourcePath, const QString &filePath, const QString &keyColumn, QString *errorMessage) const
{
    QVector<Row> localRows;
    QVector<Row> resourceRows;
    if (!readCsvRows(filePath, &localRows, errorMessage)
        || !readCsvRows(resourcePath, &resourceRows, errorMessage)) {
        return false;
    }

    QSet<QString> existingKeys;
    for (const Row &row : localRows) {
        const QString key = row.value(keyColumn).trimmed().toLower();
        if (!key.isEmpty())
            existingKeys.insert(key);
    }

    QFile resourceFile(resourcePath);
    if (!resourceFile.open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8("无法打开内置产品库：%1").arg(resourcePath);
        return false;
    }

    QTextStream in(&resourceFile);
    in.setCodec("UTF-8");
    QStringList headers;
    int keyIndex = -1;
    int lineNumber = 0;
    QStringList rowsToAppend;
    while (!in.atEnd()) {
        QString line = in.readLine();
        ++lineNumber;
        if (lineNumber == 1 && line.startsWith(QChar(0xFEFF)))
            line.remove(0, 1);
        if (line.trimmed().isEmpty())
            continue;

        const QStringList values = parseCsvLine(line);
        if (headers.isEmpty()) {
            headers = values;
            for (QString &header : headers)
                header = header.trimmed().toLower();
            keyIndex = headers.indexOf(keyColumn);
            if (keyIndex < 0) {
                if (errorMessage)
                    *errorMessage = QString::fromUtf8("%1 缺少合并键字段：%2").arg(resourcePath, keyColumn);
                return false;
            }
            continue;
        }

        if (values.size() != headers.size()) {
            if (errorMessage)
                *errorMessage = QString::fromUtf8("%1 第 %2 行字段数量不匹配：期望 %3，实际 %4")
                    .arg(resourcePath)
                    .arg(lineNumber)
                    .arg(headers.size())
                    .arg(values.size());
            return false;
        }

        const QString key = values.at(keyIndex).trimmed().toLower();
        if (!key.isEmpty() && !existingKeys.contains(key)) {
            rowsToAppend.append(line);
            existingKeys.insert(key);
        }
    }

    if (rowsToAppend.isEmpty())
        return true;

    bool endsWithNewline = true;
    QFile existingFile(filePath);
    if (existingFile.open(QIODevice::ReadOnly) && existingFile.size() > 0) {
        existingFile.seek(existingFile.size() - 1);
        const QByteArray last = existingFile.read(1);
        endsWithNewline = last == "\n" || last == "\r";
    }

    QFile outputFile(filePath);
    if (!outputFile.open(QIODevice::WriteOnly | QIODevice::Append | QIODevice::Text)) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8("无法更新本地产品库文件：%1").arg(filePath);
        return false;
    }

    QTextStream out(&outputFile);
    out.setCodec("UTF-8");
    if (!endsWithNewline)
        out << "\n";
    for (const QString &line : rowsToAppend)
        out << line << "\n";
    return true;
}

bool CatalogRepository::loadCameraCsv(const QString &filePath, QString *errorMessage)
{
    QVector<Row> rows;
    QVector<CameraSpec> previous = m_cameras;
    if (!readCsvRows(filePath, &rows, errorMessage)
        || !loadCameraRows(rows, filePath, errorMessage)) {
        m_cameras = previous;
        return false;
    }
    if (!saveCameras(errorMessage)) {
        m_cameras = previous;
        return false;
    }
    return true;
}

bool CatalogRepository::loadLensCsv(const QString &filePath, QString *errorMessage)
{
    QVector<Row> rows;
    QVector<LensSpec> previous = m_lenses;
    if (!readCsvRows(filePath, &rows, errorMessage)
        || !loadLensRows(rows, filePath, errorMessage)) {
        m_lenses = previous;
        return false;
    }
    if (!saveLenses(errorMessage)) {
        m_lenses = previous;
        return false;
    }
    return true;
}

bool CatalogRepository::loadLightCsv(const QString &filePath, QString *errorMessage)
{
    QVector<Row> rows;
    QVector<LightSpec> previous = m_lights;
    if (!readCsvRows(filePath, &rows, errorMessage)
        || !loadLightRows(rows, filePath, errorMessage)) {
        m_lights = previous;
        return false;
    }
    if (!saveLights(errorMessage)) {
        m_lights = previous;
        return false;
    }
    return true;
}

bool CatalogRepository::exportCameraCsv(const QString &filePath, QString *errorMessage) const
{
    return writeCameraCsv(filePath, m_cameras, errorMessage);
}

bool CatalogRepository::exportLensCsv(const QString &filePath, QString *errorMessage) const
{
    return writeLensCsv(filePath, m_lenses, errorMessage);
}

bool CatalogRepository::exportLightCsv(const QString &filePath, QString *errorMessage) const
{
    return writeLightCsv(filePath, m_lights, errorMessage);
}

bool CatalogRepository::exportCameraCsv(const QString &filePath, const QVector<int> &indexes, QString *errorMessage) const
{
    QVector<CameraSpec> selected;
    for (int index : indexes) {
        if (index >= 0 && index < m_cameras.size())
            selected.append(m_cameras.at(index));
    }
    return writeCameraCsv(filePath, selected, errorMessage);
}

bool CatalogRepository::exportLensCsv(const QString &filePath, const QVector<int> &indexes, QString *errorMessage) const
{
    QVector<LensSpec> selected;
    for (int index : indexes) {
        if (index >= 0 && index < m_lenses.size())
            selected.append(m_lenses.at(index));
    }
    return writeLensCsv(filePath, selected, errorMessage);
}

bool CatalogRepository::exportLightCsv(const QString &filePath, const QVector<int> &indexes, QString *errorMessage) const
{
    QVector<LightSpec> selected;
    for (int index : indexes) {
        if (index >= 0 && index < m_lights.size())
            selected.append(m_lights.at(index));
    }
    return writeLightCsv(filePath, selected, errorMessage);
}

bool CatalogRepository::resetCamerasToBuiltIn(QString *errorMessage)
{
    if (!copyResourceToFile(QStringLiteral(":/data/cameras.csv"), cameraStoragePath(), true, errorMessage))
        return false;
    QVector<Row> rows;
    return readCsvRows(cameraStoragePath(), &rows, errorMessage)
        && loadCameraRows(rows, QString::fromUtf8("\345\206\205\347\275\256\347\233\270\346\234\272\345\272\223"), errorMessage);
}

bool CatalogRepository::resetLensesToBuiltIn(QString *errorMessage)
{
    if (!copyResourceToFile(QStringLiteral(":/data/lenses.csv"), lensStoragePath(), true, errorMessage))
        return false;
    QVector<Row> rows;
    return readCsvRows(lensStoragePath(), &rows, errorMessage)
        && loadLensRows(rows, QString::fromUtf8("\345\206\205\347\275\256\351\225\234\345\244\264\345\272\223"), errorMessage);
}

bool CatalogRepository::resetLightsToBuiltIn(QString *errorMessage)
{
    if (!copyResourceToFile(QStringLiteral(":/data/lights.csv"), lightStoragePath(), true, errorMessage))
        return false;
    QVector<Row> rows;
    return readCsvRows(lightStoragePath(), &rows, errorMessage)
        && loadLightRows(rows, QString::fromUtf8("\345\206\205\347\275\256\345\205\211\346\272\220\345\272\223"), errorMessage);
}

bool CatalogRepository::addCamera(const CameraSpec &camera, QString *errorMessage)
{
    if (!validateCamera(camera, QString::fromUtf8("\346\226\260\345\242\236\347\233\270\346\234\272"), errorMessage))
        return false;
    m_cameras.append(camera);
    if (!saveCameras(errorMessage)) {
        m_cameras.removeLast();
        return false;
    }
    return true;
}

bool CatalogRepository::updateCamera(int index, const CameraSpec &camera, QString *errorMessage)
{
    if (index < 0 || index >= m_cameras.size()) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8("\347\233\270\346\234\272\350\241\214\345\217\267\346\227\240\346\225\210");
        return false;
    }
    if (!validateCamera(camera, camera.model, errorMessage))
        return false;
    const CameraSpec previous = m_cameras.at(index);
    m_cameras[index] = camera;
    if (!saveCameras(errorMessage)) {
        m_cameras[index] = previous;
        return false;
    }
    return true;
}

bool CatalogRepository::removeCamera(int index, QString *errorMessage)
{
    if (index < 0 || index >= m_cameras.size()) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8("\347\233\270\346\234\272\350\241\214\345\217\267\346\227\240\346\225\210");
        return false;
    }
    const CameraSpec removed = m_cameras.takeAt(index);
    if (!saveCameras(errorMessage)) {
        m_cameras.insert(index, removed);
        return false;
    }
    return true;
}

bool CatalogRepository::addLens(const LensSpec &lens, QString *errorMessage)
{
    if (!validateLens(lens, QString::fromUtf8("\346\226\260\345\242\236\351\225\234\345\244\264"), errorMessage))
        return false;
    m_lenses.append(lens);
    if (!saveLenses(errorMessage)) {
        m_lenses.removeLast();
        return false;
    }
    return true;
}

bool CatalogRepository::updateLens(int index, const LensSpec &lens, QString *errorMessage)
{
    if (index < 0 || index >= m_lenses.size()) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8("\351\225\234\345\244\264\350\241\214\345\217\267\346\227\240\346\225\210");
        return false;
    }
    if (!validateLens(lens, lens.model, errorMessage))
        return false;
    const LensSpec previous = m_lenses.at(index);
    m_lenses[index] = lens;
    if (!saveLenses(errorMessage)) {
        m_lenses[index] = previous;
        return false;
    }
    return true;
}

bool CatalogRepository::removeLens(int index, QString *errorMessage)
{
    if (index < 0 || index >= m_lenses.size()) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8("\351\225\234\345\244\264\350\241\214\345\217\267\346\227\240\346\225\210");
        return false;
    }
    const LensSpec removed = m_lenses.takeAt(index);
    if (!saveLenses(errorMessage)) {
        m_lenses.insert(index, removed);
        return false;
    }
    return true;
}

bool CatalogRepository::addLight(const LightSpec &light, QString *errorMessage)
{
    if (!validateLight(light, QString::fromUtf8("新增光源"), errorMessage))
        return false;
    m_lights.append(light);
    if (!saveLights(errorMessage)) {
        m_lights.removeLast();
        return false;
    }
    return true;
}

bool CatalogRepository::updateLight(int index, const LightSpec &light, QString *errorMessage)
{
    if (index < 0 || index >= m_lights.size()) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8("光源行号无效");
        return false;
    }
    if (!validateLight(light, light.model, errorMessage))
        return false;
    const LightSpec previous = m_lights.at(index);
    m_lights[index] = light;
    if (!saveLights(errorMessage)) {
        m_lights[index] = previous;
        return false;
    }
    return true;
}

bool CatalogRepository::removeLight(int index, QString *errorMessage)
{
    if (index < 0 || index >= m_lights.size()) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8("光源行号无效");
        return false;
    }
    const LightSpec removed = m_lights.takeAt(index);
    if (!saveLights(errorMessage)) {
        m_lights.insert(index, removed);
        return false;
    }
    return true;
}

QString CatalogRepository::summary() const
{
    return QString::fromUtf8("\347\233\270\346\234\272 %1 \346\254\276\357\274\214\351\225\234\345\244\264 %2 \346\254\276\357\274\214\345\205\211\346\272\220 %3 \346\254\276")
        .arg(m_cameras.size())
        .arg(m_lenses.size())
        .arg(m_lights.size());
}

bool CatalogRepository::saveCameras(QString *errorMessage) const
{
    return writeCameraCsv(cameraStoragePath(), m_cameras, errorMessage);
}

bool CatalogRepository::saveLenses(QString *errorMessage) const
{
    return writeLensCsv(lensStoragePath(), m_lenses, errorMessage);
}

bool CatalogRepository::saveLights(QString *errorMessage) const
{
    return writeLightCsv(lightStoragePath(), m_lights, errorMessage);
}

bool CatalogRepository::writeCameraCsv(const QString &filePath, const QVector<CameraSpec> &cameras, QString *errorMessage) const
{
    QFileInfo info(filePath);
    if (!QDir().mkpath(info.absolutePath())) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8("\346\227\240\346\263\225\345\210\233\345\273\272\347\233\256\345\275\225\357\274\232%1").arg(info.absolutePath());
        return false;
    }
    if (info.exists()) {
        QFile::setPermissions(filePath,
                              QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                  | QFileDevice::ReadUser | QFileDevice::WriteUser
                                  | QFileDevice::ReadGroup | QFileDevice::ReadOther);
    }
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8("\346\227\240\346\263\225\345\206\231\345\205\245 CSV\357\274\232%1").arg(filePath);
        return false;
    }
    QTextStream out(&file);
    out.setCodec("UTF-8");
    const QStringList headers = {
        QStringLiteral("model"), QStringLiteral("manufacturer"), QStringLiteral("resolution_x"), QStringLiteral("resolution_y"),
        QStringLiteral("pixel_size_um"), QStringLiteral("sensor_format"), QStringLiteral("color_mode"), QStringLiteral("shutter_type"),
        QStringLiteral("max_fps"), QStringLiteral("interface"), QStringLiteral("bandwidth_mbps"), QStringLiteral("bit_depth"),
        QStringLiteral("dynamic_range_db"), QStringLiteral("lens_mount")
    };
    out << headers.join(QLatin1Char(',')) << "\n";
    for (const CameraSpec &c : cameras) {
        QStringList row;
        row << c.model << c.manufacturer << QString::number(c.resolutionX) << QString::number(c.resolutionY)
            << QString::number(c.pixelSizeUm, 'g', 12) << c.sensorFormat << c.colorMode << c.shutterType
            << QString::number(c.maxFps, 'g', 12) << c.interfaceType << QString::number(c.bandwidthMBps, 'g', 12)
            << QString::number(c.bitDepth, 'g', 12) << QString::number(c.dynamicRangeDb, 'g', 12) << c.lensMount;
        for (QString &value : row)
            value = csvField(value);
        out << row.join(QLatin1Char(',')) << "\n";
    }
    return true;
}

bool CatalogRepository::writeLensCsv(const QString &filePath, const QVector<LensSpec> &lenses, QString *errorMessage) const
{
    QFileInfo info(filePath);
    if (!QDir().mkpath(info.absolutePath())) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8("\346\227\240\346\263\225\345\210\233\345\273\272\347\233\256\345\275\225\357\274\232%1").arg(info.absolutePath());
        return false;
    }
    if (info.exists()) {
        QFile::setPermissions(filePath,
                              QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                  | QFileDevice::ReadUser | QFileDevice::WriteUser
                                  | QFileDevice::ReadGroup | QFileDevice::ReadOther);
    }
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8("\346\227\240\346\263\225\345\206\231\345\205\245 CSV\357\274\232%1").arg(filePath);
        return false;
    }
    QTextStream out(&file);
    out.setCodec("UTF-8");
    const QStringList headers = {
        QStringLiteral("model"), QStringLiteral("manufacturer"), QStringLiteral("lens_type"), QStringLiteral("lens_mount"),
        QStringLiteral("focal_length_mm"), QStringLiteral("min_wd_mm"), QStringLiteral("distortion_percent"),
        QStringLiteral("image_circle_mm"), QStringLiteral("megapixel_rating"), QStringLiteral("recommended_min_pixel_um"),
        QStringLiteral("pmag"), QStringLiteral("nominal_wd_mm"), QStringLiteral("wd_tolerance_mm"),
        QStringLiteral("max_sensor_diagonal_mm"), QStringLiteral("telecentricity_deg"), QStringLiteral("dof_mm"),
        QStringLiteral("numerical_aperture"), QStringLiteral("f_number"), QStringLiteral("coaxial_illumination"), QStringLiteral("notes")
    };
    out << headers.join(QLatin1Char(',')) << "\n";
    for (const LensSpec &l : lenses) {
        QStringList row;
        row << l.model << l.manufacturer
            << (l.lensType == LensType::FixedFocal ? QStringLiteral("FixedFocal")
                : l.lensType == LensType::ObjectTelecentric ? QStringLiteral("ObjectTelecentric") : QStringLiteral("BiTelecentric"))
            << l.lensMount << QString::number(l.focalLengthMm, 'g', 12)
            << QString::number(l.minWorkingDistanceMm, 'g', 12) << QString::number(l.distortionPercent, 'g', 12)
            << QString::number(l.imageCircleMm, 'g', 12) << QString::number(l.megapixelRating, 'g', 12)
            << QString::number(l.recommendedMinPixelUm, 'g', 12) << QString::number(l.pmag, 'g', 12)
            << QString::number(l.nominalWorkingDistanceMm, 'g', 12) << QString::number(l.workingDistanceToleranceMm, 'g', 12)
            << QString::number(l.maxSensorDiagonalMm, 'g', 12) << QString::number(l.telecentricityDeg, 'g', 12)
            << QString::number(l.dofMm, 'g', 12) << QString::number(l.numericalAperture, 'g', 12)
            << QString::number(l.fNumber, 'g', 12) << (l.coaxialIllumination ? QStringLiteral("true") : QStringLiteral("false"))
            << l.notes;
        for (QString &value : row)
            value = csvField(value);
        out << row.join(QLatin1Char(',')) << "\n";
    }
    return true;
}

bool CatalogRepository::writeLightCsv(const QString &filePath, const QVector<LightSpec> &lights, QString *errorMessage) const
{
    QFileInfo info(filePath);
    if (!QDir().mkpath(info.absolutePath())) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8("\346\227\240\346\263\225\345\210\233\345\273\272\347\233\256\345\275\225\357\274\232%1").arg(info.absolutePath());
        return false;
    }
    if (info.exists()) {
        QFile::setPermissions(filePath,
                              QFileDevice::ReadOwner | QFileDevice::WriteOwner
                                  | QFileDevice::ReadUser | QFileDevice::WriteUser
                                  | QFileDevice::ReadGroup | QFileDevice::ReadOther);
    }
    QFile file(filePath);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8("\346\227\240\346\263\225\345\206\231\345\205\245 CSV\357\274\232%1").arg(filePath);
        return false;
    }
    QTextStream out(&file);
    out.setCodec("UTF-8");
    const QStringList headers = {
        QStringLiteral("model"), QStringLiteral("manufacturer"), QStringLiteral("light_type"), QStringLiteral("color"),
        QStringLiteral("wavelength_nm"), QStringLiteral("mode"), QStringLiteral("active_width_mm"),
        QStringLiteral("active_height_mm"), QStringLiteral("best_for")
    };
    out << headers.join(QLatin1Char(',')) << "\n";
    for (const LightSpec &l : lights) {
        QStringList row;
        row << l.model << l.manufacturer
            << (l.lightType == LightType::Backlight ? QStringLiteral("Backlight")
                : l.lightType == LightType::Ring ? QStringLiteral("Ring")
                : l.lightType == LightType::Bar ? QStringLiteral("Bar")
                : l.lightType == LightType::Coaxial ? QStringLiteral("Coaxial")
                : l.lightType == LightType::Dome ? QStringLiteral("Dome")
                : l.lightType == LightType::TelecentricBacklight ? QStringLiteral("TelecentricBacklight") : QStringLiteral("DarkField"))
            << l.color << QString::number(l.wavelengthNm) << l.mode
            << QString::number(l.activeWidthMm, 'g', 12) << QString::number(l.activeHeightMm, 'g', 12)
            << l.bestFor;
        for (QString &value : row)
            value = csvField(value);
        out << row.join(QLatin1Char(',')) << "\n";
    }
    return true;
}

QString CatalogRepository::csvField(const QString &value)
{
    QString escaped = value;
    escaped.replace(QLatin1Char('"'), QStringLiteral("\"\""));
    return QLatin1Char('"') + escaped + QLatin1Char('"');
}

bool CatalogRepository::validateCamera(const CameraSpec &camera, const QString &sourceName, QString *errorMessage)
{
    if (camera.model.trimmed().isEmpty() || camera.resolutionX <= 0 || camera.resolutionY <= 0 || camera.pixelSizeUm <= 0.0) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8("%1 \347\233\270\346\234\272\346\225\260\346\215\256\346\227\240\346\225\210\357\274\232\345\236\213\345\217\267\343\200\201\345\210\206\350\276\250\347\216\207\343\200\201\345\203\217\345\205\203\345\260\272\345\257\270\345\277\205\351\241\273\346\234\211\346\225\210").arg(sourceName);
        return false;
    }
    return true;
}

bool CatalogRepository::validateLens(const LensSpec &lens, const QString &sourceName, QString *errorMessage)
{
    if (lens.model.trimmed().isEmpty() || lens.imageCircleMm <= 0.0) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8("%1 \351\225\234\345\244\264\346\225\260\346\215\256\346\227\240\346\225\210\357\274\232\345\236\213\345\217\267\345\222\214\345\203\217\345\234\206\345\277\205\351\241\273\346\234\211\346\225\210").arg(sourceName);
        return false;
    }
    if (lens.isTelecentric() && lens.pmag <= 0.0) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8("%1 \350\277\234\345\277\203\351\225\234\345\244\264\347\274\272\345\260\221 PMAG \346\224\276\345\244\247\345\200\215\347\216\207").arg(sourceName);
        return false;
    }
    if (!lens.isTelecentric() && lens.focalLengthMm <= 0.0) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8("%1 \346\231\256\351\200\232\351\225\234\345\244\264\347\274\272\345\260\221\347\204\246\350\267\235").arg(sourceName);
        return false;
    }
    return true;
}

bool CatalogRepository::validateLight(const LightSpec &light, const QString &sourceName, QString *errorMessage)
{
    if (light.model.trimmed().isEmpty() || light.activeWidthMm <= 0.0 || light.activeHeightMm <= 0.0) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8("%1 光源数据无效：型号和有效照明尺寸必须有效").arg(sourceName);
        return false;
    }
    return true;
}

QString CatalogRepository::normalizeCameraSensorFormat(const QString &value, int resolutionX, int resolutionY, double pixelSizeUm)
{
    QString normalized = value.trimmed();
    normalized.replace(QString::fromUtf8("Ã"), QStringLiteral("x"));
    normalized.replace(QString::fromUtf8("脳"), QStringLiteral("x"));
    normalized.replace(QString::fromUtf8("×"), QStringLiteral("x"));
    normalized.replace(QString::fromUtf8("â"), QStringLiteral("\""));
    normalized.replace(QChar(0x201C), QStringLiteral("\""));
    normalized.replace(QChar(0x201D), QStringLiteral("\""));
    normalized.replace(QChar(0x2033), QStringLiteral("\""));
    normalized.replace(QChar(0xFF02), QStringLiteral("\""));
    normalized.replace(QStringLiteral("''"), QStringLiteral("\""));
    normalized.remove(QRegularExpression(QStringLiteral("\\bInGaAs\\s*,\\s*"), QRegularExpression::CaseInsensitiveOption));
    normalized.remove(QRegularExpression(QStringLiteral("\\bCMOS\\b"), QRegularExpression::CaseInsensitiveOption));
    normalized = normalized.trimmed();

    if (normalized.isEmpty()
        || QRegularExpression(QStringLiteral("^IMX\\d+"), QRegularExpression::CaseInsensitiveOption).match(normalized).hasMatch()) {
        if (resolutionX > 0 && resolutionY > 0 && pixelSizeUm > 0.0) {
            return QStringLiteral("%1 mm x %2 mm")
                .arg(resolutionX * pixelSizeUm / 1000.0, 0, 'f', 2)
                .arg(resolutionY * pixelSizeUm / 1000.0, 0, 'f', 2);
        }
        return QString();
    }

    normalized.replace(QRegularExpression(QString::fromUtf8("\\s*[xX×脳]\\s*")), QStringLiteral(" x "));
    normalized.replace(QRegularExpression(QStringLiteral("(\\d)\\s*mm\\b"), QRegularExpression::CaseInsensitiveOption), QStringLiteral("\\1 mm"));
    normalized.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral(" "));
    normalized.replace(QRegularExpression(QStringLiteral("\\s+\"")), QStringLiteral("\""));
    normalized = normalized.trimmed();

    if (QRegularExpression(QStringLiteral("^(?:\\d+(?:\\.\\d+)?|\\d+/\\d+(?:\\.\\d+)?)$")).match(normalized).hasMatch())
        normalized.append(QLatin1Char('"'));
    return normalized;
}

QString CatalogRepository::normalizeCameraColorMode(const QString &value)
{
    const QString normalized = value.trimmed();
    const QString lower = normalized.toLower();
    if (normalized.isEmpty())
        return normalized;
    if (lower.contains(QStringLiteral("mono"))
        || normalized.contains(QString::fromUtf8("黑白"))
        || normalized.contains(QString::fromUtf8("单色"))) {
        return QStringLiteral("Mono");
    }
    if (lower.contains(QStringLiteral("color"))
        || lower.contains(QStringLiteral("colour"))
        || normalized.contains(QString::fromUtf8("彩色"))) {
        return QStringLiteral("Color");
    }
    return normalized;
}

QString CatalogRepository::normalizeCameraInterface(const QString &value)
{
    QString normalized = value.trimmed();
    normalized.replace(QString::fromUtf8("Ã"), QStringLiteral("x"));
    normalized.replace(QString::fromUtf8("脳"), QStringLiteral("x"));
    normalized.replace(QString::fromUtf8("×"), QStringLiteral("x"));
    normalized.replace(QString::fromUtf8("â"), QStringLiteral("\""));
    normalized.replace(QChar(0x201C), QStringLiteral("\""));
    normalized.replace(QChar(0x201D), QStringLiteral("\""));
    return normalized;
}

QString CatalogRepository::normalizeLensMount(const QString &value)
{
    QString normalized = value.trimmed();
    QString mojibakeTimes;
    mojibakeTimes.append(QChar(0x00C3));
    mojibakeTimes.append(QChar(0x0097));
    normalized.replace(mojibakeTimes, QStringLiteral("x"));
    normalized.replace(QChar(0x00D7), QStringLiteral("x"));
    normalized.replace(QChar(0x8133), QStringLiteral("x"));
    normalized.replace(QRegularExpression(QStringLiteral("\\s*x\\s*"), QRegularExpression::CaseInsensitiveOption), QStringLiteral(" x "));
    normalized.replace(QRegularExpression(QStringLiteral("\\s+")), QStringLiteral(" "));
    return normalized.trimmed();
}

bool CatalogRepository::readCsvRows(const QString &filePath, QVector<Row> *rows, QString *errorMessage)
{
    QFile file(filePath);
    return readCsvRowsFromDevice(&file, filePath, rows, errorMessage);
}

bool CatalogRepository::readCsvRowsFromDevice(QIODevice *device, const QString &sourceName, QVector<Row> *rows, QString *errorMessage)
{
    if (!device->open(QIODevice::ReadOnly | QIODevice::Text)) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8("\346\227\240\346\263\225\346\211\223\345\274\200 CSV\357\274\232%1").arg(sourceName);
        return false;
    }

    QTextStream in(device);
    in.setCodec("UTF-8");

    QStringList headers;
    int lineNumber = 0;
    while (!in.atEnd()) {
        QString line = in.readLine();
        ++lineNumber;
        if (lineNumber == 1 && line.startsWith(QChar(0xFEFF)))
            line.remove(0, 1);
        if (line.trimmed().isEmpty())
            continue;

        const QStringList values = parseCsvLine(line);
        if (headers.isEmpty()) {
            headers = values;
            for (QString &header : headers)
                header = header.trimmed().toLower();
            continue;
        }

        if (values.size() != headers.size()) {
            if (errorMessage) {
                *errorMessage = QString::fromUtf8("%1 \347\254\254 %2 \350\241\214\345\255\227\346\256\265\346\225\260\351\207\217\344\270\215\345\214\271\351\205\215\357\274\232\346\234\237\346\234\233 %3\357\274\214\345\256\236\351\231\205 %4")
                    .arg(sourceName)
                    .arg(lineNumber)
                    .arg(headers.size())
                    .arg(values.size());
            }
            return false;
        }

        Row row;
        for (int i = 0; i < headers.size(); ++i)
            row.insert(headers.at(i), values.at(i).trimmed());
        rows->append(row);
    }

    if (headers.isEmpty()) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8("%1 \346\262\241\346\234\211\350\241\250\345\244\264").arg(sourceName);
        return false;
    }
    return true;
}

QStringList CatalogRepository::parseCsvLine(const QString &line)
{
    QStringList out;
    QString current;
    bool inQuotes = false;

    for (int i = 0; i < line.size(); ++i) {
        const QChar ch = line.at(i);
        if (ch == QLatin1Char('"')) {
            if (inQuotes && i + 1 < line.size() && line.at(i + 1) == QLatin1Char('"')) {
                current.append(ch);
                ++i;
            } else {
                inQuotes = !inQuotes;
            }
        } else if (ch == QLatin1Char(',') && !inQuotes) {
            out.append(current);
            current.clear();
        } else {
            current.append(ch);
        }
    }
    out.append(current);
    return out;
}

bool CatalogRepository::ensureColumns(const QVector<Row> &rows, const QStringList &required, const QString &sourceName, QString *errorMessage)
{
    if (rows.isEmpty()) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8("%1 \346\262\241\346\234\211\346\225\260\346\215\256\350\241\214").arg(sourceName);
        return false;
    }

    for (const QString &column : required) {
        if (!rows.first().contains(column)) {
            if (errorMessage)
                *errorMessage = QString::fromUtf8("%1 \347\274\272\345\260\221\345\277\205\351\234\200\345\255\227\346\256\265\357\274\232%2").arg(sourceName, column);
            return false;
        }
    }
    return true;
}

double CatalogRepository::number(const Row &row, const QString &key, double defaultValue)
{
    bool ok = false;
    const double value = row.value(key).toDouble(&ok);
    return ok ? value : defaultValue;
}

int CatalogRepository::integer(const Row &row, const QString &key, int defaultValue)
{
    bool ok = false;
    const int value = row.value(key).toInt(&ok);
    return ok ? value : defaultValue;
}

QString CatalogRepository::text(const Row &row, const QString &key)
{
    return row.value(key).trimmed();
}

QString CatalogRepository::firstText(const Row &row, const QStringList &keys)
{
    for (const QString &key : keys) {
        const QString value = text(row, key);
        if (!value.isEmpty())
            return value;
    }
    return QString();
}

bool CatalogRepository::loadCameraRows(const QVector<Row> &rows, const QString &sourceName, QString *errorMessage)
{
    const QStringList required = {
        QStringLiteral("model"),
        QStringLiteral("resolution_x"),
        QStringLiteral("resolution_y"),
        QStringLiteral("pixel_size_um"),
        QStringLiteral("color_mode"),
        QStringLiteral("shutter_type"),
        QStringLiteral("max_fps"),
        QStringLiteral("interface"),
        QStringLiteral("bandwidth_mbps"),
        QStringLiteral("bit_depth"),
        QStringLiteral("lens_mount")
    };
    if (!ensureColumns(rows, required, sourceName, errorMessage))
        return false;

    QVector<CameraSpec> imported;
    for (const Row &row : rows) {
        CameraSpec spec;
        spec.model = text(row, QStringLiteral("model"));
        spec.manufacturer = firstText(row, {QStringLiteral("manufacturer"), QStringLiteral("vendor"), QStringLiteral("brand"), QStringLiteral("maker"),
                                            QString::fromUtf8("\345\216\202\345\256\266"), QString::fromUtf8("\345\216\202\345\225\206"),
                                            QString::fromUtf8("\345\210\266\351\200\240\345\225\206")});
        spec.resolutionX = integer(row, QStringLiteral("resolution_x"));
        spec.resolutionY = integer(row, QStringLiteral("resolution_y"));
        spec.pixelSizeUm = number(row, QStringLiteral("pixel_size_um"));
        spec.sensorFormat = normalizeCameraSensorFormat(text(row, QStringLiteral("sensor_format")), spec.resolutionX, spec.resolutionY, spec.pixelSizeUm);
        spec.colorMode = normalizeCameraColorMode(text(row, QStringLiteral("color_mode")));
        spec.shutterType = text(row, QStringLiteral("shutter_type"));
        spec.maxFps = number(row, QStringLiteral("max_fps"));
        spec.interfaceType = normalizeCameraInterface(text(row, QStringLiteral("interface")));
        spec.bandwidthMBps = number(row, QStringLiteral("bandwidth_mbps"));
        spec.bitDepth = number(row, QStringLiteral("bit_depth"), 8.0);
        spec.dynamicRangeDb = number(row, QStringLiteral("dynamic_range_db"));
        spec.lensMount = normalizeLensMount(text(row, QStringLiteral("lens_mount")));

        if (spec.model.isEmpty() || spec.resolutionX <= 0 || spec.resolutionY <= 0 || spec.pixelSizeUm <= 0.0) {
            if (errorMessage)
                *errorMessage = QString::fromUtf8("%1 \344\270\255\347\233\270\346\234\272\346\225\260\346\215\256\346\227\240\346\225\210\357\274\232\345\236\213\345\217\267\343\200\201\345\210\206\350\276\250\347\216\207\343\200\201\345\203\217\345\205\203\345\260\272\345\257\270\345\277\205\351\241\273\346\234\211\346\225\210").arg(sourceName);
            return false;
        }
        imported.append(spec);
    }

    m_cameras = imported;
    return true;
}

bool CatalogRepository::loadLensRows(const QVector<Row> &rows, const QString &sourceName, QString *errorMessage)
{
    const QStringList required = {
        QStringLiteral("model"),
        QStringLiteral("lens_type"),
        QStringLiteral("lens_mount"),
        QStringLiteral("distortion_percent"),
        QStringLiteral("image_circle_mm")
    };
    if (!ensureColumns(rows, required, sourceName, errorMessage))
        return false;

    QVector<LensSpec> imported;
    for (const Row &row : rows) {
        LensSpec spec;
        spec.model = text(row, QStringLiteral("model"));
        spec.manufacturer = firstText(row, {QStringLiteral("manufacturer"), QStringLiteral("vendor"), QStringLiteral("brand"), QStringLiteral("maker"),
                                            QString::fromUtf8("\345\216\202\345\256\266"), QString::fromUtf8("\345\216\202\345\225\206"),
                                            QString::fromUtf8("\345\210\266\351\200\240\345\225\206")});
        spec.lensType = lensTypeFromString(text(row, QStringLiteral("lens_type")));
        spec.lensMount = normalizeLensMount(text(row, QStringLiteral("lens_mount")));
        spec.focalLengthMm = number(row, QStringLiteral("focal_length_mm"));
        spec.minWorkingDistanceMm = number(row, QStringLiteral("min_wd_mm"));
        spec.distortionPercent = number(row, QStringLiteral("distortion_percent"));
        spec.imageCircleMm = number(row, QStringLiteral("image_circle_mm"));
        spec.megapixelRating = number(row, QStringLiteral("megapixel_rating"));
        spec.recommendedMinPixelUm = number(row, QStringLiteral("recommended_min_pixel_um"));
        spec.pmag = number(row, QStringLiteral("pmag"));
        spec.nominalWorkingDistanceMm = number(row, QStringLiteral("nominal_wd_mm"));
        spec.workingDistanceToleranceMm = number(row, QStringLiteral("wd_tolerance_mm"));
        spec.maxSensorDiagonalMm = number(row, QStringLiteral("max_sensor_diagonal_mm"));
        spec.telecentricityDeg = number(row, QStringLiteral("telecentricity_deg"));
        spec.dofMm = number(row, QStringLiteral("dof_mm"));
        spec.numericalAperture = number(row, QStringLiteral("numerical_aperture"));
        spec.fNumber = number(row, QStringLiteral("f_number"));
        spec.coaxialIllumination = parseBool(text(row, QStringLiteral("coaxial_illumination")));
        spec.notes = text(row, QStringLiteral("notes"));

        if (spec.model.isEmpty() || spec.imageCircleMm <= 0.0) {
            if (errorMessage)
                *errorMessage = QString::fromUtf8("%1 \344\270\255\351\225\234\345\244\264\346\225\260\346\215\256\346\227\240\346\225\210\357\274\232\345\236\213\345\217\267\345\222\214\345\203\217\345\234\206\345\277\205\351\241\273\346\234\211\346\225\210").arg(sourceName);
            return false;
        }
        if (spec.isTelecentric() && spec.pmag <= 0.0) {
            if (errorMessage)
                *errorMessage = QString::fromUtf8("%1 \344\270\255\350\277\234\345\277\203\351\225\234\345\244\264 %2 \347\274\272\345\260\221 PMAG \346\224\276\345\244\247\345\200\215\347\216\207").arg(sourceName, spec.model);
            return false;
        }
        if (!spec.isTelecentric() && spec.focalLengthMm <= 0.0) {
            if (errorMessage)
                *errorMessage = QString::fromUtf8("%1 \344\270\255\346\231\256\351\200\232\351\225\234\345\244\264 %2 \347\274\272\345\260\221\347\204\246\350\267\235").arg(sourceName, spec.model);
            return false;
        }
        imported.append(spec);
    }

    m_lenses = imported;
    return true;
}

bool CatalogRepository::loadLightRows(const QVector<Row> &rows, const QString &sourceName, QString *errorMessage)
{
    const QStringList required = {
        QStringLiteral("model"),
        QStringLiteral("light_type"),
        QStringLiteral("active_width_mm"),
        QStringLiteral("active_height_mm")
    };
    if (!ensureColumns(rows, required, sourceName, errorMessage))
        return false;

    QVector<LightSpec> imported;
    for (const Row &row : rows) {
        LightSpec spec;
        spec.model = text(row, QStringLiteral("model"));
        spec.manufacturer = firstText(row, {QStringLiteral("manufacturer"), QStringLiteral("vendor"), QStringLiteral("brand"), QStringLiteral("maker"),
                                            QString::fromUtf8("\345\216\202\345\256\266"), QString::fromUtf8("\345\216\202\345\225\206"),
                                            QString::fromUtf8("\345\210\266\351\200\240\345\225\206")});
        spec.lightType = lightTypeFromString(text(row, QStringLiteral("light_type")));
        spec.color = text(row, QStringLiteral("color"));
        spec.wavelengthNm = integer(row, QStringLiteral("wavelength_nm"));
        spec.mode = text(row, QStringLiteral("mode"));
        spec.activeWidthMm = number(row, QStringLiteral("active_width_mm"));
        spec.activeHeightMm = number(row, QStringLiteral("active_height_mm"));
        spec.bestFor = text(row, QStringLiteral("best_for"));
        if (spec.lightType == LightType::Ring && spec.isDarkFieldLike())
            spec.lightType = LightType::DarkField;

        if (!validateLight(spec, sourceName, errorMessage))
            return false;
        imported.append(spec);
    }

    m_lights = imported;
    return true;
}
