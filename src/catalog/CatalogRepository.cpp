#include "catalog/CatalogRepository.h"

#include <QFile>
#include <QIODevice>
#include <QTextStream>

bool CatalogRepository::loadDefaults(QString *errorMessage)
{
    QVector<Row> cameraRows;
    QVector<Row> lensRows;
    QVector<Row> lightRows;

    QFile cameras(QStringLiteral(":/data/cameras.csv"));
    QFile lenses(QStringLiteral(":/data/lenses.csv"));
    QFile lights(QStringLiteral(":/data/lights.csv"));

    if (!readCsvRowsFromDevice(&cameras, QStringLiteral(":/data/cameras.csv"), &cameraRows, errorMessage))
        return false;
    if (!readCsvRowsFromDevice(&lenses, QStringLiteral(":/data/lenses.csv"), &lensRows, errorMessage))
        return false;
    if (!readCsvRowsFromDevice(&lights, QStringLiteral(":/data/lights.csv"), &lightRows, errorMessage))
        return false;

    return loadCameraRows(cameraRows, QString::fromUtf8("\345\206\205\347\275\256\347\233\270\346\234\272\345\272\223"), errorMessage)
        && loadLensRows(lensRows, QString::fromUtf8("\345\206\205\347\275\256\351\225\234\345\244\264\345\272\223"), errorMessage)
        && loadLightRows(lightRows, QString::fromUtf8("\345\206\205\347\275\256\345\205\211\346\272\220\345\272\223"), errorMessage);
}

bool CatalogRepository::loadCameraCsv(const QString &filePath, QString *errorMessage)
{
    QVector<Row> rows;
    return readCsvRows(filePath, &rows, errorMessage)
        && loadCameraRows(rows, filePath, errorMessage);
}

bool CatalogRepository::loadLensCsv(const QString &filePath, QString *errorMessage)
{
    QVector<Row> rows;
    return readCsvRows(filePath, &rows, errorMessage)
        && loadLensRows(rows, filePath, errorMessage);
}

bool CatalogRepository::loadLightCsv(const QString &filePath, QString *errorMessage)
{
    QVector<Row> rows;
    return readCsvRows(filePath, &rows, errorMessage)
        && loadLightRows(rows, filePath, errorMessage);
}

QString CatalogRepository::summary() const
{
    return QString::fromUtf8("\347\233\270\346\234\272 %1 \346\254\276\357\274\214\351\225\234\345\244\264 %2 \346\254\276\357\274\214\345\205\211\346\272\220 %3 \346\254\276")
        .arg(m_cameras.size())
        .arg(m_lenses.size())
        .arg(m_lights.size());
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
        spec.resolutionX = integer(row, QStringLiteral("resolution_x"));
        spec.resolutionY = integer(row, QStringLiteral("resolution_y"));
        spec.pixelSizeUm = number(row, QStringLiteral("pixel_size_um"));
        spec.sensorFormat = text(row, QStringLiteral("sensor_format"));
        spec.colorMode = text(row, QStringLiteral("color_mode"));
        spec.shutterType = text(row, QStringLiteral("shutter_type"));
        spec.maxFps = number(row, QStringLiteral("max_fps"));
        spec.interfaceType = text(row, QStringLiteral("interface"));
        spec.bandwidthMBps = number(row, QStringLiteral("bandwidth_mbps"));
        spec.bitDepth = number(row, QStringLiteral("bit_depth"), 8.0);
        spec.dynamicRangeDb = number(row, QStringLiteral("dynamic_range_db"));
        spec.lensMount = text(row, QStringLiteral("lens_mount"));

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
        spec.lensType = lensTypeFromString(text(row, QStringLiteral("lens_type")));
        spec.lensMount = text(row, QStringLiteral("lens_mount"));
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
        spec.lightType = lightTypeFromString(text(row, QStringLiteral("light_type")));
        spec.color = text(row, QStringLiteral("color"));
        spec.wavelengthNm = integer(row, QStringLiteral("wavelength_nm"));
        spec.mode = text(row, QStringLiteral("mode"));
        spec.activeWidthMm = number(row, QStringLiteral("active_width_mm"));
        spec.activeHeightMm = number(row, QStringLiteral("active_height_mm"));
        spec.bestFor = text(row, QStringLiteral("best_for"));

        if (spec.model.isEmpty() || spec.activeWidthMm <= 0.0 || spec.activeHeightMm <= 0.0) {
            if (errorMessage)
                *errorMessage = QString::fromUtf8("%1 \344\270\255\345\205\211\346\272\220\346\225\260\346\215\256\346\227\240\346\225\210\357\274\232\345\236\213\345\217\267\345\222\214\346\234\211\346\225\210\347\205\247\346\230\216\345\260\272\345\257\270\345\277\205\351\241\273\346\234\211\346\225\210").arg(sourceName);
            return false;
        }
        imported.append(spec);
    }

    m_lights = imported;
    return true;
}
