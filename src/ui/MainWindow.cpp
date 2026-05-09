#include "ui/MainWindow.h"

#include "report/PdfReportWriter.h"
#include "selection/CalculationAssistant.h"
#include "selection/SelectionEngine.h"

#include <QApplication>
#include <QCheckBox>
#include <QClipboard>
#include <QComboBox>
#include <QDateTime>
#include <QDialog>
#include <QDialogButtonBox>
#include <QDoubleSpinBox>
#include <QFile>
#include <QFileDialog>
#include <QFormLayout>
#include <QFrame>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QKeySequence>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QPushButton>
#include <QScrollArea>
#include <QShortcut>
#include <QSignalBlocker>
#include <QSize>
#include <QSpinBox>
#include <QStackedWidget>
#include <QStyle>
#include <QTabWidget>
#include <QTableWidget>
#include <QTextEdit>
#include <QTextStream>
#include <QVBoxLayout>

namespace {
QLabel *pageTitle(const QString &text, const QString &subtitle = QString())
{
    QWidget *unused = nullptr;
    Q_UNUSED(unused)
    QLabel *label = new QLabel(text);
    label->setObjectName(QStringLiteral("PageTitle"));
    if (!subtitle.isEmpty())
        label->setToolTip(subtitle);
    return label;
}

QTableWidgetItem *item(const QString &text)
{
    QTableWidgetItem *tableItem = new QTableWidgetItem(text);
    tableItem->setFlags(tableItem->flags() ^ Qt::ItemIsEditable);
    tableItem->setToolTip(text);
    return tableItem;
}

QTableWidgetItem *indexedItem(const QString &text, int sourceIndex)
{
    QTableWidgetItem *tableItem = item(text);
    tableItem->setData(Qt::UserRole, sourceIndex);
    return tableItem;
}

int rowSourceIndex(const QTableWidget *table, int row)
{
    if (!table || row < 0 || row >= table->rowCount())
        return -1;
    const QTableWidgetItem *first = table->item(row, 0);
    if (!first)
        return -1;
    return first->data(Qt::UserRole).toInt();
}

void copySelectionToClipboard(QTableWidget *table)
{
    if (!table)
        return;

    QList<QTableWidgetSelectionRange> ranges = table->selectedRanges();
    if (ranges.isEmpty() && table->currentRow() >= 0)
        ranges.append(QTableWidgetSelectionRange(table->currentRow(), 0, table->currentRow(), table->columnCount() - 1));
    if (ranges.isEmpty())
        return;

    QStringList lines;
    for (const QTableWidgetSelectionRange &range : ranges) {
        for (int row = range.topRow(); row <= range.bottomRow(); ++row) {
            QStringList cells;
            for (int column = range.leftColumn(); column <= range.rightColumn(); ++column) {
                if (table->isColumnHidden(column))
                    continue;
                QTableWidgetItem *cell = table->item(row, column);
                cells.append(cell ? cell->text() : QString());
            }
            lines.append(cells.join(QLatin1Char('\t')));
        }
    }
    QApplication::clipboard()->setText(lines.join(QLatin1Char('\n')));
}

void installTableCopyShortcut(QTableWidget *table)
{
    QShortcut *copyShortcut = new QShortcut(QKeySequence::Copy, table);
    QObject::connect(copyShortcut, &QShortcut::activated, table, [table]() {
        copySelectionToClipboard(table);
    });
}

QString catalogKey(const QString &manufacturer, const QString &model)
{
    return manufacturer.trimmed() + QLatin1Char('\n') + model.trimmed();
}

QString selectedCatalogKey(const QTableWidget *table)
{
    if (!table || table->currentRow() < 0)
        return QString();
    const QTableWidgetItem *manufacturer = table->item(table->currentRow(), 1);
    const QTableWidgetItem *model = table->item(table->currentRow(), 0);
    if (!manufacturer || !model)
        return QString();
    return catalogKey(manufacturer->text(), model->text());
}

void selectCatalogRow(QTableWidget *table, const QString &key, int fallbackRow)
{
    if (!table || table->rowCount() <= 0)
        return;
    if (!key.isEmpty()) {
        for (int row = 0; row < table->rowCount(); ++row) {
            const QTableWidgetItem *manufacturer = table->item(row, 1);
            const QTableWidgetItem *model = table->item(row, 0);
            if (manufacturer && model && catalogKey(manufacturer->text(), model->text()) == key) {
                table->selectRow(row);
                return;
            }
        }
    }
    table->selectRow(qBound(0, fallbackRow, table->rowCount() - 1));
}

void selectRowBySourceIndex(QTableWidget *table, int sourceIndex)
{
    if (!table || sourceIndex < 0)
        return;
    for (int row = 0; row < table->rowCount(); ++row) {
        if (rowSourceIndex(table, row) == sourceIndex) {
            table->selectRow(row);
            return;
        }
    }
}

QString number(double value, int decimals = 2)
{
    return QString::number(value, 'f', decimals);
}

QString productLabel(const QString &manufacturer, const QString &model)
{
    if (manufacturer.trimmed().isEmpty())
        return model;
    return manufacturer.trimmed() + QLatin1Char(' ') + model;
}

QString riskSummary(const SelectionResult &result)
{
    return result.score.risks.isEmpty()
        ? QString::fromUtf8("无主要风险")
        : result.score.risks.join(QString::fromUtf8("；"));
}

QString exposureText(double exposureUs)
{
    return exposureUs > 0.0
        ? QStringLiteral("%1 us").arg(exposureUs, 0, 'f', 1)
        : QString::fromUtf8("无运动约束");
}

QString bomSpecForCamera(const CameraSpec &camera, const SelectionResult &result)
{
    return QStringLiteral("%1 x %2, %3, %4, %5 fps, %6 MB/s")
        .arg(camera.resolutionX)
        .arg(camera.resolutionY)
        .arg(camera.pixelSizeUm, 0, 'f', 2)
        .arg(camera.interfaceType)
        .arg(camera.maxFps, 0, 'f', 1)
        .arg(result.interfaceCapacityMBps, 0, 'f', 1);
}

QString bomSpecForLens(const LensSpec &lens, const SelectionResult &result)
{
    if (lens.isTelecentric()) {
        return QStringLiteral("%1, PMAG %2x, WD %3 mm, DOF %4 mm, image %5 mm")
            .arg(lens.typeLabel())
            .arg(result.magnification, 0, 'f', 3)
            .arg(lens.nominalWorkingDistanceMm, 0, 'f', 1)
            .arg(result.estimatedDofMm, 0, 'f', 2)
            .arg(lens.imageCircleMm, 0, 'f', 1);
    }
    return QStringLiteral("%1, f %2 mm, min WD %3 mm, DOF %4 mm, image %5 mm")
        .arg(lens.typeLabel())
        .arg(lens.focalLengthMm, 0, 'f', 1)
        .arg(lens.minWorkingDistanceMm, 0, 'f', 1)
        .arg(result.estimatedDofMm, 0, 'f', 2)
        .arg(lens.imageCircleMm, 0, 'f', 1);
}

QString bomSpecForLight(const LightSpec &light, const SelectionResult &result)
{
    return QStringLiteral("%1, %2, %3, %4 x %5 mm, margin %6%")
        .arg(light.typeLabel())
        .arg(light.color)
        .arg(light.mode)
        .arg(light.activeWidthMm, 0, 'f', 0)
        .arg(light.activeHeightMm, 0, 'f', 0)
        .arg(result.lightCoverageMarginPercent, 0, 'f', 0);
}

QString csvCell(QString value)
{
    value.replace(QLatin1Char('"'), QStringLiteral("\"\""));
    return QLatin1Char('"') + value + QLatin1Char('"');
}

void setupTable(QTableWidget *table)
{
    table->setAlternatingRowColors(true);
    table->setSelectionBehavior(QAbstractItemView::SelectRows);
    table->setSelectionMode(QAbstractItemView::SingleSelection);
    table->verticalHeader()->setVisible(false);
    table->horizontalHeader()->setStretchLastSection(true);
    table->horizontalHeader()->setDefaultAlignment(Qt::AlignLeft | Qt::AlignVCenter);
    table->setSortingEnabled(true);
    installTableCopyShortcut(table);
}

QDoubleSpinBox *dialogSpin(double min, double max, double value, const QString &suffix = QString(), int decimals = 3)
{
    QDoubleSpinBox *spin = new QDoubleSpinBox;
    spin->setRange(min, max);
    spin->setDecimals(decimals);
    spin->setValue(value);
    spin->setSuffix(suffix);
    spin->setKeyboardTracking(false);
    return spin;
}

QSpinBox *dialogIntSpin(int min, int max, int value, const QString &suffix = QString())
{
    QSpinBox *spin = new QSpinBox;
    spin->setRange(min, max);
    spin->setValue(value);
    spin->setSuffix(suffix);
    spin->setKeyboardTracking(false);
    return spin;
}

void setComboText(QComboBox *combo, const QString &text)
{
    const int index = combo->findText(text, Qt::MatchFixedString);
    if (index >= 0) {
        combo->setCurrentIndex(index);
    } else {
        combo->setEditText(text);
    }
}

QComboBox *editableCombo(const QStringList &items, const QString &value)
{
    QComboBox *combo = new QComboBox;
    combo->setEditable(true);
    combo->addItems(items);
    setComboText(combo, value);
    return combo;
}

bool editCameraDialog(QWidget *parent, CameraSpec *camera, const QString &title)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(title);
    QVBoxLayout *outer = new QVBoxLayout(&dialog);
    QFormLayout *form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignLeft);

    QLineEdit *model = new QLineEdit(camera->model);
    QLineEdit *manufacturer = new QLineEdit(camera->manufacturer);
    QSpinBox *resolutionX = dialogIntSpin(1, 200000, camera->resolutionX);
    QSpinBox *resolutionY = dialogIntSpin(1, 200000, camera->resolutionY);
    QDoubleSpinBox *pixelSize = dialogSpin(0.01, 1000.0, camera->pixelSizeUm, QStringLiteral(" um"));
    QLineEdit *sensorFormat = new QLineEdit(camera->sensorFormat);
    QComboBox *colorMode = editableCombo({QStringLiteral("Mono"), QStringLiteral("Color")}, camera->colorMode);
    QComboBox *shutterType = editableCombo({QStringLiteral("Global"), QStringLiteral("Rolling")}, camera->shutterType);
    QDoubleSpinBox *maxFps = dialogSpin(0.0, 100000.0, camera->maxFps, QStringLiteral(" fps"), 2);
    QComboBox *interfaceType = editableCombo({QStringLiteral("GigE"), QStringLiteral("USB3"), QStringLiteral("10GigE"), QStringLiteral("CameraLink"), QStringLiteral("CoaXPress")}, camera->interfaceType);
    QDoubleSpinBox *bandwidth = dialogSpin(0.0, 100000.0, camera->bandwidthMBps, QStringLiteral(" MB/s"), 2);
    QDoubleSpinBox *bitDepth = dialogSpin(1.0, 32.0, camera->bitDepth, QStringLiteral(" bit"), 1);
    QDoubleSpinBox *dynamicRange = dialogSpin(0.0, 200.0, camera->dynamicRangeDb, QStringLiteral(" dB"), 1);
    QComboBox *lensMount = editableCombo({QStringLiteral("C"), QStringLiteral("M12"), QStringLiteral("M42"), QStringLiteral("M58"), QStringLiteral("F")}, camera->lensMount);

    form->addRow(QString::fromUtf8("\345\236\213\345\217\267"), model);
    form->addRow(QString::fromUtf8("\345\216\202\345\256\266"), manufacturer);
    form->addRow(QString::fromUtf8("\345\210\206\350\276\250\347\216\207 X"), resolutionX);
    form->addRow(QString::fromUtf8("\345\210\206\350\276\250\347\216\207 Y"), resolutionY);
    form->addRow(QString::fromUtf8("\345\203\217\345\205\203"), pixelSize);
    form->addRow(QString::fromUtf8("\351\235\266\351\235\242/\344\274\240\346\204\237\345\231\250"), sensorFormat);
    form->addRow(QString::fromUtf8("\351\242\234\350\211\262"), colorMode);
    form->addRow(QString::fromUtf8("\345\277\253\351\227\250"), shutterType);
    form->addRow(QStringLiteral("fps"), maxFps);
    form->addRow(QString::fromUtf8("\346\216\245\345\217\243"), interfaceType);
    form->addRow(QString::fromUtf8("\345\270\246\345\256\275"), bandwidth);
    form->addRow(QString::fromUtf8("\344\275\215\346\267\261"), bitDepth);
    form->addRow(QString::fromUtf8("\345\212\250\346\200\201\350\214\203\345\233\264"), dynamicRange);
    form->addRow(QString::fromUtf8("\351\225\234\345\244\264\345\217\243"), lensMount);
    outer->addLayout(form);

    QDialogButtonBox *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    QObject::connect(box, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(box, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    outer->addWidget(box);

    if (dialog.exec() != QDialog::Accepted)
        return false;

    camera->model = model->text().trimmed();
    camera->manufacturer = manufacturer->text().trimmed();
    camera->resolutionX = resolutionX->value();
    camera->resolutionY = resolutionY->value();
    camera->pixelSizeUm = pixelSize->value();
    camera->sensorFormat = sensorFormat->text().trimmed();
    camera->colorMode = colorMode->currentText().trimmed();
    camera->shutterType = shutterType->currentText().trimmed();
    camera->maxFps = maxFps->value();
    camera->interfaceType = interfaceType->currentText().trimmed();
    camera->bandwidthMBps = bandwidth->value();
    camera->bitDepth = bitDepth->value();
    camera->dynamicRangeDb = dynamicRange->value();
    camera->lensMount = lensMount->currentText().trimmed();
    return true;
}

bool editLensDialog(QWidget *parent, LensSpec *lens, const QString &title)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(title);
    QVBoxLayout *outer = new QVBoxLayout(&dialog);
    QScrollArea *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    QWidget *content = new QWidget;
    QFormLayout *form = new QFormLayout(content);
    form->setLabelAlignment(Qt::AlignLeft);

    QLineEdit *model = new QLineEdit(lens->model);
    QLineEdit *manufacturer = new QLineEdit(lens->manufacturer);
    QComboBox *type = new QComboBox;
    type->addItems({QStringLiteral("FixedFocal"), QStringLiteral("ObjectTelecentric"), QStringLiteral("BiTelecentric")});
    type->setCurrentIndex(lens->lensType == LensType::FixedFocal ? 0 : lens->lensType == LensType::ObjectTelecentric ? 1 : 2);
    QComboBox *mount = editableCombo({QStringLiteral("C"), QStringLiteral("M12"), QStringLiteral("M42"), QStringLiteral("M58"), QStringLiteral("F")}, lens->lensMount);
    QDoubleSpinBox *focal = dialogSpin(0.0, 10000.0, lens->focalLengthMm, QStringLiteral(" mm"));
    QDoubleSpinBox *minWd = dialogSpin(0.0, 100000.0, lens->minWorkingDistanceMm, QStringLiteral(" mm"));
    QDoubleSpinBox *distortion = dialogSpin(0.0, 100.0, lens->distortionPercent, QStringLiteral(" %"));
    QDoubleSpinBox *imageCircle = dialogSpin(0.0, 1000.0, lens->imageCircleMm, QStringLiteral(" mm"));
    QDoubleSpinBox *mp = dialogSpin(0.0, 1000.0, lens->megapixelRating, QStringLiteral(" MP"), 2);
    QDoubleSpinBox *recommendedPixel = dialogSpin(0.0, 1000.0, lens->recommendedMinPixelUm, QStringLiteral(" um"));
    QDoubleSpinBox *pmag = dialogSpin(0.0, 1000.0, lens->pmag, QStringLiteral("x"));
    QDoubleSpinBox *nominalWd = dialogSpin(0.0, 100000.0, lens->nominalWorkingDistanceMm, QStringLiteral(" mm"));
    QDoubleSpinBox *wdTolerance = dialogSpin(0.0, 10000.0, lens->workingDistanceToleranceMm, QStringLiteral(" mm"));
    QDoubleSpinBox *maxSensor = dialogSpin(0.0, 1000.0, lens->maxSensorDiagonalMm, QStringLiteral(" mm"));
    QDoubleSpinBox *telecentricity = dialogSpin(0.0, 90.0, lens->telecentricityDeg, QStringLiteral(" deg"));
    QDoubleSpinBox *dof = dialogSpin(0.0, 100000.0, lens->dofMm, QStringLiteral(" mm"));
    QDoubleSpinBox *na = dialogSpin(0.0, 10.0, lens->numericalAperture, QString(), 4);
    QDoubleSpinBox *fNumber = dialogSpin(0.0, 1000.0, lens->fNumber, QStringLiteral(" F"), 2);
    QCheckBox *coaxial = new QCheckBox(QString::fromUtf8("\346\224\257\346\214\201\345\220\214\350\275\264\347\205\247\346\230\216"));
    coaxial->setChecked(lens->coaxialIllumination);
    QTextEdit *notes = new QTextEdit(lens->notes);
    notes->setMinimumHeight(72);

    form->addRow(QString::fromUtf8("\345\236\213\345\217\267"), model);
    form->addRow(QString::fromUtf8("\345\216\202\345\256\266"), manufacturer);
    form->addRow(QString::fromUtf8("\347\261\273\345\236\213"), type);
    form->addRow(QString::fromUtf8("\346\216\245\345\217\243"), mount);
    form->addRow(QString::fromUtf8("\347\204\246\350\267\235"), focal);
    form->addRow(QString::fromUtf8("\346\234\200\345\260\217 WD"), minWd);
    form->addRow(QString::fromUtf8("\347\225\270\345\217\230"), distortion);
    form->addRow(QString::fromUtf8("\345\203\217\345\234\206"), imageCircle);
    form->addRow(QStringLiteral("MP"), mp);
    form->addRow(QString::fromUtf8("\346\216\250\350\215\220\346\234\200\345\260\217\345\203\217\345\205\203"), recommendedPixel);
    form->addRow(QStringLiteral("PMAG"), pmag);
    form->addRow(QString::fromUtf8("\346\240\207\347\247\260 WD"), nominalWd);
    form->addRow(QString::fromUtf8("WD \345\256\271\345\267\256"), wdTolerance);
    form->addRow(QString::fromUtf8("\346\234\200\345\244\247\351\235\266\351\235\242"), maxSensor);
    form->addRow(QString::fromUtf8("\350\277\234\345\277\203\345\272\246"), telecentricity);
    form->addRow(QStringLiteral("DOF"), dof);
    form->addRow(QStringLiteral("NA"), na);
    form->addRow(QStringLiteral("F/#"), fNumber);
    form->addRow(QString(), coaxial);
    form->addRow(QString::fromUtf8("\345\244\207\346\263\250"), notes);

    scroll->setWidget(content);
    outer->addWidget(scroll);
    QDialogButtonBox *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    QObject::connect(box, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(box, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    outer->addWidget(box);
    dialog.resize(520, 720);

    if (dialog.exec() != QDialog::Accepted)
        return false;

    lens->model = model->text().trimmed();
    lens->manufacturer = manufacturer->text().trimmed();
    lens->lensType = type->currentIndex() == 0 ? LensType::FixedFocal : type->currentIndex() == 1 ? LensType::ObjectTelecentric : LensType::BiTelecentric;
    lens->lensMount = mount->currentText().trimmed();
    lens->focalLengthMm = focal->value();
    lens->minWorkingDistanceMm = minWd->value();
    lens->distortionPercent = distortion->value();
    lens->imageCircleMm = imageCircle->value();
    lens->megapixelRating = mp->value();
    lens->recommendedMinPixelUm = recommendedPixel->value();
    lens->pmag = pmag->value();
    lens->nominalWorkingDistanceMm = nominalWd->value();
    lens->workingDistanceToleranceMm = wdTolerance->value();
    lens->maxSensorDiagonalMm = maxSensor->value();
    lens->telecentricityDeg = telecentricity->value();
    lens->dofMm = dof->value();
    lens->numericalAperture = na->value();
    lens->fNumber = fNumber->value();
    lens->coaxialIllumination = coaxial->isChecked();
    lens->notes = notes->toPlainText().trimmed();
    return true;
}

bool editLightDialog(QWidget *parent, LightSpec *light, const QString &title)
{
    QDialog dialog(parent);
    dialog.setWindowTitle(title);
    QVBoxLayout *outer = new QVBoxLayout(&dialog);
    QFormLayout *form = new QFormLayout;
    form->setLabelAlignment(Qt::AlignLeft);

    QLineEdit *model = new QLineEdit(light->model);
    QLineEdit *manufacturer = new QLineEdit(light->manufacturer);
    QComboBox *type = new QComboBox;
    type->addItems({QStringLiteral("Backlight"), QStringLiteral("Ring"), QStringLiteral("Bar"),
                    QStringLiteral("Coaxial"), QStringLiteral("Dome"), QStringLiteral("TelecentricBacklight"),
                    QStringLiteral("DarkField")});
    const QString currentType = light->lightType == LightType::Backlight ? QStringLiteral("Backlight")
        : light->lightType == LightType::Ring ? QStringLiteral("Ring")
        : light->lightType == LightType::Bar ? QStringLiteral("Bar")
        : light->lightType == LightType::Coaxial ? QStringLiteral("Coaxial")
        : light->lightType == LightType::Dome ? QStringLiteral("Dome")
        : light->lightType == LightType::TelecentricBacklight ? QStringLiteral("TelecentricBacklight") : QStringLiteral("DarkField");
    type->setCurrentText(currentType);
    QComboBox *color = editableCombo({QStringLiteral("White"), QStringLiteral("Red"), QStringLiteral("Blue"),
                                      QStringLiteral("Green"), QStringLiteral("IR"), QStringLiteral("UV")}, light->color);
    QSpinBox *wavelength = dialogIntSpin(0, 100000, light->wavelengthNm, QStringLiteral(" nm"));
    QComboBox *mode = editableCombo({QStringLiteral("Continuous"), QStringLiteral("Strobe"), QStringLiteral("Trigger"), QStringLiteral("Pulse")}, light->mode);
    QDoubleSpinBox *activeWidth = dialogSpin(0.1, 100000.0, light->activeWidthMm, QStringLiteral(" mm"), 2);
    QDoubleSpinBox *activeHeight = dialogSpin(0.1, 100000.0, light->activeHeightMm, QStringLiteral(" mm"), 2);
    QTextEdit *bestFor = new QTextEdit(light->bestFor);
    bestFor->setMinimumHeight(88);

    form->addRow(QString::fromUtf8("型号"), model);
    form->addRow(QString::fromUtf8("厂家"), manufacturer);
    form->addRow(QString::fromUtf8("类型"), type);
    form->addRow(QString::fromUtf8("颜色"), color);
    form->addRow(QString::fromUtf8("波长"), wavelength);
    form->addRow(QString::fromUtf8("模式"), mode);
    form->addRow(QString::fromUtf8("有效宽度"), activeWidth);
    form->addRow(QString::fromUtf8("有效高度"), activeHeight);
    form->addRow(QString::fromUtf8("适用场景"), bestFor);
    outer->addLayout(form);

    QDialogButtonBox *box = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel);
    QObject::connect(box, &QDialogButtonBox::accepted, &dialog, &QDialog::accept);
    QObject::connect(box, &QDialogButtonBox::rejected, &dialog, &QDialog::reject);
    outer->addWidget(box);
    dialog.resize(520, 520);

    if (dialog.exec() != QDialog::Accepted)
        return false;

    light->model = model->text().trimmed();
    light->manufacturer = manufacturer->text().trimmed();
    light->lightType = lightTypeFromString(type->currentText().trimmed());
    light->color = color->currentText().trimmed();
    light->wavelengthNm = wavelength->value();
    light->mode = mode->currentText().trimmed();
    light->activeWidthMm = activeWidth->value();
    light->activeHeightMm = activeHeight->value();
    light->bestFor = bestFor->toPlainText().trimmed();
    return true;
}

QString allManufacturersText()
{
    return QString::fromUtf8("全部厂家");
}

QString allInterfacesText()
{
    return QString::fromUtf8("全部接口");
}

QString allTypesText()
{
    return QString::fromUtf8("全部类型");
}

QString allMountsText()
{
    return QString::fromUtf8("全部接口");
}

QString allModesText()
{
    return QString::fromUtf8("全部模式");
}

void fillComboPreservingText(QComboBox *combo, const QString &allText, const QStringList &values)
{
    if (!combo)
        return;
    const QString previous = combo->currentText();
    QSignalBlocker blocker(combo);
    combo->clear();
    combo->addItem(allText);
    QStringList sorted = values;
    sorted.removeDuplicates();
    sorted.sort(Qt::CaseInsensitive);
    for (const QString &value : sorted) {
        if (!value.trimmed().isEmpty())
            combo->addItem(value);
    }
    const int index = combo->findText(previous, Qt::MatchFixedString);
    combo->setCurrentIndex(index >= 0 ? index : 0);
}

bool textMatches(const QString &needle, const QStringList &fields)
{
    if (needle.trimmed().isEmpty())
        return true;
    const QString lowered = needle.trimmed().toLower();
    for (const QString &field : fields) {
        if (field.toLower().contains(lowered))
            return true;
    }
    return false;
}
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    QString error;
    if (!m_catalog.loadDefaults(&error))
        QMessageBox::critical(this, QString::fromUtf8("\345\217\202\346\225\260\345\272\223\345\212\240\350\275\275\345\244\261\350\264\245"), error);

    buildUi();
    calculate();
}

void MainWindow::buildUi()
{
    QWidget *root = new QWidget(this);
    QHBoxLayout *rootLayout = new QHBoxLayout(root);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    rootLayout->addWidget(createSidebar());

    m_pages = new QStackedWidget(root);
    m_pages->addWidget(createInputPage());
    m_pages->addWidget(createCalculationPage());
    m_pages->addWidget(createResultsPage());
    m_pages->addWidget(createComparisonPage());
    m_pages->addWidget(createCatalogPage());
    m_pages->addWidget(createReportPage());
    rootLayout->addWidget(m_pages, 1);

    setCentralWidget(root);
    setWindowTitle(QString::fromUtf8("VisionSelect - \345\267\245\344\270\232\346\234\272\345\231\250\350\247\206\350\247\211\351\200\211\345\236\213\345\212\251\346\211\213"));
    setActivePage(0);
}

QWidget *MainWindow::createSidebar()
{
    QFrame *sidebar = new QFrame;
    sidebar->setObjectName(QStringLiteral("Sidebar"));
    sidebar->setFixedWidth(248);

    QVBoxLayout *layout = new QVBoxLayout(sidebar);
    layout->setContentsMargins(18, 18, 18, 18);
    layout->setSpacing(10);

    QFrame *brand = new QFrame(sidebar);
    brand->setObjectName(QStringLiteral("SidebarBrand"));
    QVBoxLayout *brandLayout = new QVBoxLayout(brand);
    brandLayout->setContentsMargins(14, 14, 14, 14);
    brandLayout->setSpacing(6);
    QLabel *title = new QLabel(QStringLiteral("VisionSelect"));
    title->setObjectName(QStringLiteral("AppTitle"));
    QLabel *subtitle = new QLabel(QString::fromUtf8("\345\267\245\344\270\232\346\234\272\345\231\250\350\247\206\350\247\211\351\200\211\345\236\213\345\212\251\346\211\213"));
    subtitle->setObjectName(QStringLiteral("AppSubtitle"));
    subtitle->setWordWrap(true);
    QLabel *badge = new QLabel(QString::fromUtf8("\351\234\200\346\261\202 \302\267 \350\256\241\347\256\227 \302\267 \351\200\211\345\236\213"));
    badge->setObjectName(QStringLiteral("AppBadge"));
    brandLayout->addWidget(title);
    brandLayout->addWidget(subtitle);
    brandLayout->addWidget(badge, 0, Qt::AlignLeft);
    layout->addWidget(brand);

    QLabel *navTitle = new QLabel(QString::fromUtf8("\345\267\245\344\275\234\345\217\260"));
    navTitle->setObjectName(QStringLiteral("SidebarSectionLabel"));
    layout->addWidget(navTitle);

    const QStringList pages = {
        QString::fromUtf8("\351\234\200\346\261\202\350\276\223\345\205\245"),
        QString::fromUtf8("\350\256\241\347\256\227\345\212\251\346\211\213"),
        QString::fromUtf8("\346\216\250\350\215\220\347\273\223\346\236\234"),
        QString::fromUtf8("方案对比"),
        QString::fromUtf8("\345\217\202\346\225\260\345\272\223"),
        QString::fromUtf8("PDF \346\212\245\345\221\212")
    };
    const QVector<QStyle::StandardPixmap> icons = {
        QStyle::SP_FileDialogContentsView,
        QStyle::SP_FileDialogDetailedView,
        QStyle::SP_DialogApplyButton,
        QStyle::SP_ArrowRight,
        QStyle::SP_DirIcon,
        QStyle::SP_FileIcon
    };
    for (int i = 0; i < pages.size(); ++i) {
        QPushButton *button = new QPushButton(pages.at(i));
        button->setObjectName(QStringLiteral("NavButton"));
        button->setCursor(Qt::PointingHandCursor);
        button->setMinimumHeight(44);
        button->setIcon(style()->standardIcon(icons.value(i, QStyle::SP_FileIcon)));
        button->setIconSize(QSize(16, 16));
        button->setFocusPolicy(Qt::NoFocus);
        connect(button, &QPushButton::clicked, this, [this, i]() { setActivePage(i); });
        m_navButtons.append(button);
        layout->addWidget(button);
    }

    layout->addStretch();
    QFrame *summaryBox = new QFrame(sidebar);
    summaryBox->setObjectName(QStringLiteral("SidebarSummary"));
    QVBoxLayout *summaryLayout = new QVBoxLayout(summaryBox);
    summaryLayout->setContentsMargins(12, 12, 12, 12);
    summaryLayout->setSpacing(5);
    QLabel *summaryTitle = new QLabel(QString::fromUtf8("\344\272\247\345\223\201\345\272\223"));
    summaryTitle->setObjectName(QStringLiteral("SidebarSummaryTitle"));
    m_summaryLabel = new QLabel(m_catalog.summary());
    m_summaryLabel->setObjectName(QStringLiteral("SidebarSummaryValue"));
    m_summaryLabel->setWordWrap(true);
    summaryLayout->addWidget(summaryTitle);
    summaryLayout->addWidget(m_summaryLabel);
    layout->addWidget(summaryBox);

    return sidebar;
}

QWidget *MainWindow::createInputPage()
{
    QWidget *page = new QWidget;
    QVBoxLayout *outer = new QVBoxLayout(page);
    outer->setContentsMargins(28, 24, 28, 24);
    outer->setSpacing(16);
    outer->addWidget(pageTitle(QString::fromUtf8("\351\234\200\346\261\202\350\276\223\345\205\245")));

    QScrollArea *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    QWidget *content = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(content);
    layout->setContentsMargins(0, 0, 12, 0);
    layout->setSpacing(14);

    QGroupBox *objectBox = new QGroupBox(QString::fromUtf8("\345\267\245\344\273\266\344\270\216\347\262\276\345\272\246"));
    QFormLayout *objectLayout = new QFormLayout(objectBox);
    objectLayout->setLabelAlignment(Qt::AlignLeft);
    m_widthSpin = makeSpin(0.1, 2000.0, 20.0, QStringLiteral(" mm"));
    m_heightSpin = makeSpin(0.1, 2000.0, 20.0, QStringLiteral(" mm"));
    m_marginSpin = makeSpin(0.0, 200.0, 2.0, QStringLiteral(" mm"));
    m_minFeatureSpin = makeSpin(0.1, 10000.0, 50.0, QStringLiteral(" um"));
    m_toleranceSpin = makeSpin(0.1, 10000.0, 10.0, QStringLiteral(" um"));
    objectLayout->addRow(QString::fromUtf8("\345\267\245\344\273\266\345\256\275\345\272\246"), m_widthSpin);
    objectLayout->addRow(QString::fromUtf8("\345\267\245\344\273\266\351\253\230\345\272\246"), m_heightSpin);
    objectLayout->addRow(QString::fromUtf8("\345\256\232\344\275\215/\350\243\205\345\244\271\344\275\231\351\207\217"), m_marginSpin);
    objectLayout->addRow(QString::fromUtf8("\346\234\200\345\260\217\347\211\271\345\276\201\345\260\272\345\257\270"), m_minFeatureSpin);
    objectLayout->addRow(QString::fromUtf8("\345\205\201\350\256\270\346\265\213\351\207\217\350\257\257\345\267\256"), m_toleranceSpin);

    QGroupBox *processBox = new QGroupBox(QString::fromUtf8("\345\267\245\350\211\272\344\270\216\345\256\211\350\243\205"));
    QFormLayout *processLayout = new QFormLayout(processBox);
    m_detectionCombo = new QComboBox;
    m_detectionCombo->addItems({detectionTypeLabel(DetectionType::Measurement),
                                detectionTypeLabel(DetectionType::Positioning),
                                detectionTypeLabel(DetectionType::DefectInspection),
                                detectionTypeLabel(DetectionType::OcrCode)});
    m_surfaceCombo = new QComboBox;
    m_surfaceCombo->addItems({surfaceTypeLabel(SurfaceType::Matte),
                              surfaceTypeLabel(SurfaceType::ReflectiveMetal),
                              surfaceTypeLabel(SurfaceType::GlassTransparent),
                              surfaceTypeLabel(SurfaceType::PCB),
                              surfaceTypeLabel(SurfaceType::Plastic),
                              surfaceTypeLabel(SurfaceType::Mixed)});
    m_surfaceCombo->setCurrentIndex(1);
    m_wdSpin = makeSpin(5.0, 3000.0, 110.0, QStringLiteral(" mm"));
    m_heightVariationSpin = makeSpin(0.0, 200.0, 2.0, QStringLiteral(" mm"));
    m_speedSpin = makeSpin(0.0, 10000.0, 0.0, QStringLiteral(" mm/s"));
    m_fpsSpin = makeSpin(1.0, 1000.0, 20.0, QStringLiteral(" fps"));
    m_reflectiveCheck = new QCheckBox(QString::fromUtf8("\345\217\215\345\205\211/\351\253\230\345\205\211\350\241\250\351\235\242"));
    m_reflectiveCheck->setChecked(true);
    m_monoCheck = new QCheckBox(QString::fromUtf8("\344\274\230\345\205\210\351\273\221\347\231\275\347\233\270\346\234\272"));
    m_monoCheck->setChecked(true);
    m_allowTelecentricCheck = new QCheckBox(QString::fromUtf8("\345\205\201\350\256\270\350\277\234\345\277\203\351\225\234\345\244\264"));
    m_allowTelecentricCheck->setChecked(true);
    processLayout->addRow(QString::fromUtf8("\346\243\200\346\265\213\347\261\273\345\236\213"), m_detectionCombo);
    processLayout->addRow(QString::fromUtf8("\350\241\250\351\235\242\346\235\220\350\264\250"), m_surfaceCombo);
    processLayout->addRow(QString::fromUtf8("\345\267\245\344\275\234\350\267\235\347\246\273"), m_wdSpin);
    processLayout->addRow(QString::fromUtf8("\351\253\230\345\272\246\346\263\242\345\212\250"), m_heightVariationSpin);
    processLayout->addRow(QString::fromUtf8("\350\277\220\345\212\250\351\200\237\345\272\246"), m_speedSpin);
    processLayout->addRow(QString::fromUtf8("\350\212\202\346\213\215/\345\270\247\347\216\207"), m_fpsSpin);
    processLayout->addRow(QString(), m_reflectiveCheck);
    processLayout->addRow(QString(), m_monoCheck);
    processLayout->addRow(QString(), m_allowTelecentricCheck);

    QHBoxLayout *buttonLayout = new QHBoxLayout;
    QPushButton *calculateButton = new QPushButton(QString::fromUtf8("\350\256\241\347\256\227\346\216\250\350\215\220"));
    QPushButton *resultButton = new QPushButton(QString::fromUtf8("\346\237\245\347\234\213\347\273\223\346\236\234"));
    resultButton->setObjectName(QStringLiteral("SecondaryButton"));
    connect(calculateButton, &QPushButton::clicked, this, &MainWindow::calculate);
    connect(resultButton, &QPushButton::clicked, this, [this]() { setActivePage(2); });
    buttonLayout->addWidget(calculateButton);
    buttonLayout->addWidget(resultButton);
    buttonLayout->addStretch();

    layout->addWidget(objectBox);
    layout->addWidget(processBox);
    layout->addLayout(buttonLayout);
    layout->addStretch();
    scroll->setWidget(content);
    outer->addWidget(scroll, 1);

    return page;
}

QWidget *MainWindow::createCalculationPage()
{
    QWidget *page = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(28, 24, 28, 24);
    layout->setSpacing(14);

    layout->addWidget(pageTitle(QString::fromUtf8("\350\256\241\347\256\227\345\212\251\346\211\213")));

    QHBoxLayout *buttons = new QHBoxLayout;
    QPushButton *refreshButton = new QPushButton(QString::fromUtf8("\346\240\271\346\215\256\345\275\223\345\211\215\351\234\200\346\261\202\350\256\241\347\256\227"));
    QPushButton *inputButton = new QPushButton(QString::fromUtf8("\350\277\224\345\233\236\351\234\200\346\261\202\350\276\223\345\205\245"));
    inputButton->setObjectName(QStringLiteral("SecondaryButton"));
    connect(refreshButton, &QPushButton::clicked, this, [this]() {
        m_request = readRequest();
        refreshCalculationAssistant();
    });
    connect(inputButton, &QPushButton::clicked, this, [this]() { setActivePage(0); });
    buttons->addWidget(refreshButton);
    buttons->addWidget(inputButton);
    buttons->addStretch();
    layout->addLayout(buttons);

    m_assistantSummaryLabel = new QLabel;
    m_assistantSummaryLabel->setObjectName(QStringLiteral("SectionTitle"));
    m_assistantSummaryLabel->setWordWrap(true);
    layout->addWidget(m_assistantSummaryLabel);

    QLabel *cameraTitle = new QLabel(QString::fromUtf8("\347\233\270\346\234\272\344\274\260\347\256\227"));
    cameraTitle->setObjectName(QStringLiteral("SectionTitle"));
    layout->addWidget(cameraTitle);

    m_assistantCameraTable = new QTableWidget;
    setupTable(m_assistantCameraTable);
    m_assistantCameraTable->setColumnCount(10);
    m_assistantCameraTable->setHorizontalHeaderLabels({
        QString::fromUtf8("\347\233\270\346\234\272"), QString::fromUtf8("\345\216\202\345\256\266"),
        QString::fromUtf8("\345\210\206\350\276\250\347\216\207"), QString::fromUtf8("\345\203\217\345\205\203"),
        QString::fromUtf8("\344\274\240\346\204\237\345\231\250"), QString::fromUtf8("\347\211\251\346\226\271\345\203\217\347\264\240"),
        QString::fromUtf8("\346\231\256\351\200\232\347\204\246\350\267\235"), QStringLiteral("PMAG"),
        QString::fromUtf8("\345\270\246\345\256\275"), QString::fromUtf8("\345\210\244\346\226\255")
    });
    m_assistantCameraTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_assistantCameraTable->horizontalHeader()->setSectionResizeMode(9, QHeaderView::Stretch);
    connect(m_assistantCameraTable, &QTableWidget::cellClicked, this, [this](int row, int) {
        const int sourceRow = rowSourceIndex(m_assistantCameraTable, row);
        m_assistantSelectedCameraRow = sourceRow >= 0 ? sourceRow : row;
        refreshAssistantLensTable();
    });
    layout->addWidget(m_assistantCameraTable, 1);

    QLabel *lensTitle = new QLabel(QString::fromUtf8("\351\225\234\345\244\264\345\200\231\351\200\211"));
    lensTitle->setObjectName(QStringLiteral("SectionTitle"));
    layout->addWidget(lensTitle);

    m_assistantLensTable = new QTableWidget;
    setupTable(m_assistantLensTable);
    m_assistantLensTable->setColumnCount(10);
    m_assistantLensTable->setHorizontalHeaderLabels({
        QString::fromUtf8("\347\261\273\345\236\213"), QString::fromUtf8("\345\216\202\345\256\266"),
        QString::fromUtf8("\351\225\234\345\244\264"), QString::fromUtf8("\346\216\245\345\217\243"),
        QString::fromUtf8("\347\204\246\350\267\235/PMAG"), QStringLiteral("FOV"),
        QString::fromUtf8("\347\211\251\346\226\271\345\203\217\347\264\240"), QStringLiteral("WD/DOF"),
        QString::fromUtf8("\345\203\217\345\234\206"), QString::fromUtf8("\345\210\244\346\226\255")
    });
    m_assistantLensTable->horizontalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);
    m_assistantLensTable->horizontalHeader()->setSectionResizeMode(9, QHeaderView::Stretch);
    layout->addWidget(m_assistantLensTable, 1);

    m_assistantDetails = new QTextEdit;
    m_assistantDetails->setReadOnly(true);
    m_assistantDetails->setMinimumHeight(120);
    layout->addWidget(m_assistantDetails);

    return page;
}

QWidget *MainWindow::createResultsPage()
{
    QWidget *page = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(28, 24, 28, 24);
    layout->setSpacing(14);

    layout->addWidget(pageTitle(QString::fromUtf8("\346\216\250\350\215\220\347\273\223\346\236\234")));
    m_resultSummaryLabel = new QLabel;
    m_resultSummaryLabel->setObjectName(QStringLiteral("SectionTitle"));
    layout->addWidget(m_resultSummaryLabel);

    QHBoxLayout *resultActions = new QHBoxLayout;
    QPushButton *compareButton = new QPushButton(QString::fromUtf8("查看方案对比"));
    compareButton->setObjectName(QStringLiteral("SecondaryButton"));
    connect(compareButton, &QPushButton::clicked, this, [this]() { setActivePage(3); });
    resultActions->addWidget(compareButton);
    resultActions->addStretch();
    layout->addLayout(resultActions);

    m_resultTable = new QTableWidget;
    setupTable(m_resultTable);
    m_resultTable->setColumnCount(10);
    m_resultTable->setHorizontalHeaderLabels({
        QString::fromUtf8("\347\261\273\345\236\213"), QString::fromUtf8("\345\276\227\345\210\206"), QString::fromUtf8("\347\233\270\346\234\272"),
        QString::fromUtf8("\351\225\234\345\244\264"), QString::fromUtf8("\345\205\211\346\272\220"), QStringLiteral("FOV(mm)"),
        QString::fromUtf8("\347\211\251\346\226\271\345\203\217\347\264\240"), QString::fromUtf8("\345\200\215\347\216\207/\347\204\246\350\267\235"), QStringLiteral("WD/DOF"), QString::fromUtf8("\351\243\216\351\231\251")
    });
    m_resultTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    const int resultColumnWidths[] = {46, 50, 150, 150, 145, 78, 74, 78, 85};
    for (int column = 0; column < 9; ++column)
        m_resultTable->setColumnWidth(column, resultColumnWidths[column]);
    m_resultTable->horizontalHeader()->setSectionResizeMode(9, QHeaderView::Stretch);
    connect(m_resultTable, &QTableWidget::cellClicked, this, [this](int row, int) {
        const int sourceRow = rowSourceIndex(m_resultTable, row);
        refreshResultDetails(sourceRow >= 0 ? sourceRow : row);
    });

    m_resultDetails = new QTextEdit;
    m_resultDetails->setReadOnly(true);
    m_resultDetails->setMinimumHeight(150);

    layout->addWidget(m_resultTable, 1);
    layout->addWidget(m_resultDetails);
    return page;
}

QWidget *MainWindow::createComparisonPage()
{
    QWidget *page = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(28, 24, 28, 24);
    layout->setSpacing(14);

    layout->addWidget(pageTitle(QString::fromUtf8("方案对比")));

    QHBoxLayout *actions = new QHBoxLayout;
    QPushButton *recalculateButton = new QPushButton(QString::fromUtf8("重新计算"));
    QPushButton *exportBomButton = new QPushButton(QString::fromUtf8("导出 BOM CSV"));
    exportBomButton->setObjectName(QStringLiteral("SecondaryButton"));
    connect(recalculateButton, &QPushButton::clicked, this, &MainWindow::calculate);
    connect(exportBomButton, &QPushButton::clicked, this, &MainWindow::exportBomCsv);
    actions->addWidget(recalculateButton);
    actions->addWidget(exportBomButton);
    actions->addStretch();
    layout->addLayout(actions);

    m_compareTable = new QTableWidget;
    setupTable(m_compareTable);
    m_compareTable->setColumnCount(12);
    m_compareTable->setHorizontalHeaderLabels({
        QString::fromUtf8("方案"), QString::fromUtf8("得分"), QString::fromUtf8("相机"),
        QString::fromUtf8("镜头"), QString::fromUtf8("光源"), QStringLiteral("FOV"),
        QString::fromUtf8("物方像素"), QString::fromUtf8("曝光上限"), QString::fromUtf8("带宽/存储"),
        QStringLiteral("DOF/畸变"), QString::fromUtf8("光源余量"), QString::fromUtf8("主要风险")
    });
    m_compareTable->horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
    const int compareWidths[] = {58, 44, 112, 112, 110, 75, 70, 75, 90, 88, 60};
    for (int column = 0; column < 11; ++column)
        m_compareTable->setColumnWidth(column, compareWidths[column]);
    m_compareTable->horizontalHeader()->setSectionResizeMode(11, QHeaderView::Stretch);
    connect(m_compareTable, &QTableWidget::cellClicked, this, [this](int row, int) {
        const int sourceRow = rowSourceIndex(m_compareTable, row);
        refreshComparisonDetails(sourceRow >= 0 ? sourceRow : row);
    });

    m_compareDetails = new QTextEdit;
    m_compareDetails->setReadOnly(true);
    m_compareDetails->setMinimumHeight(150);

    layout->addWidget(m_compareTable, 1);
    layout->addWidget(m_compareDetails);
    return page;
}

QWidget *MainWindow::createCatalogPage()
{
    QWidget *page = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(28, 24, 28, 24);
    layout->setSpacing(14);
    layout->addWidget(pageTitle(QString::fromUtf8("\345\217\202\346\225\260\345\272\223")));

    QTabWidget *tabs = new QTabWidget;

    QWidget *cameraPage = new QWidget;
    QVBoxLayout *cameraLayout = new QVBoxLayout(cameraPage);
    cameraLayout->setContentsMargins(0, 0, 0, 0);
    cameraLayout->setSpacing(8);
    QHBoxLayout *cameraFilters = new QHBoxLayout;
    m_cameraSearchEdit = new QLineEdit;
    m_cameraSearchEdit->setPlaceholderText(QString::fromUtf8("搜索型号/厂家/接口"));
    m_cameraManufacturerFilter = new QComboBox;
    m_cameraInterfaceFilter = new QComboBox;
    QPushButton *clearCameraFilterButton = new QPushButton(QString::fromUtf8("清空筛选"));
    clearCameraFilterButton->setObjectName(QStringLiteral("SecondaryButton"));
    cameraFilters->addWidget(m_cameraSearchEdit, 2);
    cameraFilters->addWidget(m_cameraManufacturerFilter);
    cameraFilters->addWidget(m_cameraInterfaceFilter);
    cameraFilters->addWidget(clearCameraFilterButton);
    cameraLayout->addLayout(cameraFilters);
    QHBoxLayout *cameraActions = new QHBoxLayout;
    QPushButton *addCameraButton = new QPushButton(QString::fromUtf8("新增相机"));
    QPushButton *editCameraButton = new QPushButton(QString::fromUtf8("编辑"));
    QPushButton *removeCameraButton = new QPushButton(QString::fromUtf8("删除"));
    QPushButton *importCameraButton = new QPushButton(QString::fromUtf8("导入 CSV"));
    QPushButton *exportCameraButton = new QPushButton(QString::fromUtf8("导出 CSV"));
    QPushButton *exportFilteredCameraButton = new QPushButton(QString::fromUtf8("导出筛选"));
    QPushButton *resetCameraButton = new QPushButton(QString::fromUtf8("重置内置"));
    editCameraButton->setObjectName(QStringLiteral("SecondaryButton"));
    removeCameraButton->setObjectName(QStringLiteral("SecondaryButton"));
    importCameraButton->setObjectName(QStringLiteral("SecondaryButton"));
    exportCameraButton->setObjectName(QStringLiteral("SecondaryButton"));
    exportFilteredCameraButton->setObjectName(QStringLiteral("SecondaryButton"));
    resetCameraButton->setObjectName(QStringLiteral("SecondaryButton"));
    cameraActions->addWidget(addCameraButton);
    cameraActions->addWidget(editCameraButton);
    cameraActions->addWidget(removeCameraButton);
    cameraActions->addWidget(importCameraButton);
    cameraActions->addWidget(exportCameraButton);
    cameraActions->addWidget(exportFilteredCameraButton);
    cameraActions->addWidget(resetCameraButton);
    cameraActions->addStretch();
    cameraLayout->addLayout(cameraActions);
    m_cameraTable = new QTableWidget;
    setupTable(m_cameraTable);
    cameraLayout->addWidget(m_cameraTable, 1);
    tabs->addTab(cameraPage, QString::fromUtf8("\347\233\270\346\234\272"));

    QWidget *lensPage = new QWidget;
    QVBoxLayout *lensLayout = new QVBoxLayout(lensPage);
    lensLayout->setContentsMargins(0, 0, 0, 0);
    lensLayout->setSpacing(8);
    QHBoxLayout *lensFilters = new QHBoxLayout;
    m_lensSearchEdit = new QLineEdit;
    m_lensSearchEdit->setPlaceholderText(QString::fromUtf8("搜索型号/厂家/备注"));
    m_lensManufacturerFilter = new QComboBox;
    m_lensTypeFilter = new QComboBox;
    m_lensMountFilter = new QComboBox;
    QPushButton *clearLensFilterButton = new QPushButton(QString::fromUtf8("清空筛选"));
    clearLensFilterButton->setObjectName(QStringLiteral("SecondaryButton"));
    lensFilters->addWidget(m_lensSearchEdit, 2);
    lensFilters->addWidget(m_lensManufacturerFilter);
    lensFilters->addWidget(m_lensTypeFilter);
    lensFilters->addWidget(m_lensMountFilter);
    lensFilters->addWidget(clearLensFilterButton);
    lensLayout->addLayout(lensFilters);
    QHBoxLayout *lensActions = new QHBoxLayout;
    QPushButton *addLensButton = new QPushButton(QString::fromUtf8("新增镜头"));
    QPushButton *editLensButton = new QPushButton(QString::fromUtf8("编辑"));
    QPushButton *removeLensButton = new QPushButton(QString::fromUtf8("删除"));
    QPushButton *importLensButton = new QPushButton(QString::fromUtf8("导入 CSV"));
    QPushButton *exportLensButton = new QPushButton(QString::fromUtf8("导出 CSV"));
    QPushButton *exportFilteredLensButton = new QPushButton(QString::fromUtf8("导出筛选"));
    QPushButton *resetLensButton = new QPushButton(QString::fromUtf8("重置内置"));
    editLensButton->setObjectName(QStringLiteral("SecondaryButton"));
    removeLensButton->setObjectName(QStringLiteral("SecondaryButton"));
    importLensButton->setObjectName(QStringLiteral("SecondaryButton"));
    exportLensButton->setObjectName(QStringLiteral("SecondaryButton"));
    exportFilteredLensButton->setObjectName(QStringLiteral("SecondaryButton"));
    resetLensButton->setObjectName(QStringLiteral("SecondaryButton"));
    lensActions->addWidget(addLensButton);
    lensActions->addWidget(editLensButton);
    lensActions->addWidget(removeLensButton);
    lensActions->addWidget(importLensButton);
    lensActions->addWidget(exportLensButton);
    lensActions->addWidget(exportFilteredLensButton);
    lensActions->addWidget(resetLensButton);
    lensActions->addStretch();
    lensLayout->addLayout(lensActions);
    m_lensTable = new QTableWidget;
    setupTable(m_lensTable);
    lensLayout->addWidget(m_lensTable, 1);
    tabs->addTab(lensPage, QString::fromUtf8("\351\225\234\345\244\264"));

    QWidget *lightPage = new QWidget;
    QVBoxLayout *lightLayout = new QVBoxLayout(lightPage);
    lightLayout->setContentsMargins(0, 0, 0, 0);
    lightLayout->setSpacing(8);
    QHBoxLayout *lightFilters = new QHBoxLayout;
    m_lightSearchEdit = new QLineEdit;
    m_lightSearchEdit->setPlaceholderText(QString::fromUtf8("搜索型号/厂家/场景"));
    m_lightManufacturerFilter = new QComboBox;
    m_lightTypeFilter = new QComboBox;
    m_lightModeFilter = new QComboBox;
    QPushButton *clearLightFilterButton = new QPushButton(QString::fromUtf8("清空筛选"));
    clearLightFilterButton->setObjectName(QStringLiteral("SecondaryButton"));
    lightFilters->addWidget(m_lightSearchEdit, 2);
    lightFilters->addWidget(m_lightManufacturerFilter);
    lightFilters->addWidget(m_lightTypeFilter);
    lightFilters->addWidget(m_lightModeFilter);
    lightFilters->addWidget(clearLightFilterButton);
    lightLayout->addLayout(lightFilters);
    QHBoxLayout *lightActions = new QHBoxLayout;
    QPushButton *addLightButton = new QPushButton(QString::fromUtf8("新增光源"));
    QPushButton *editLightButton = new QPushButton(QString::fromUtf8("编辑"));
    QPushButton *removeLightButton = new QPushButton(QString::fromUtf8("删除"));
    QPushButton *importLightButton = new QPushButton(QString::fromUtf8("导入 CSV"));
    QPushButton *exportLightButton = new QPushButton(QString::fromUtf8("导出 CSV"));
    QPushButton *exportFilteredLightButton = new QPushButton(QString::fromUtf8("导出筛选"));
    QPushButton *resetLightButton = new QPushButton(QString::fromUtf8("重置内置"));
    editLightButton->setObjectName(QStringLiteral("SecondaryButton"));
    removeLightButton->setObjectName(QStringLiteral("SecondaryButton"));
    importLightButton->setObjectName(QStringLiteral("SecondaryButton"));
    exportLightButton->setObjectName(QStringLiteral("SecondaryButton"));
    exportFilteredLightButton->setObjectName(QStringLiteral("SecondaryButton"));
    resetLightButton->setObjectName(QStringLiteral("SecondaryButton"));
    lightActions->addWidget(addLightButton);
    lightActions->addWidget(editLightButton);
    lightActions->addWidget(removeLightButton);
    lightActions->addWidget(importLightButton);
    lightActions->addWidget(exportLightButton);
    lightActions->addWidget(exportFilteredLightButton);
    lightActions->addWidget(resetLightButton);
    lightActions->addStretch();
    lightLayout->addLayout(lightActions);
    m_lightTable = new QTableWidget;
    setupTable(m_lightTable);
    lightLayout->addWidget(m_lightTable, 1);
    tabs->addTab(lightPage, QString::fromUtf8("\345\205\211\346\272\220"));

    connect(m_cameraSearchEdit, &QLineEdit::textChanged, this, &MainWindow::refreshCameraTable);
    connect(m_cameraManufacturerFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::refreshCameraTable);
    connect(m_cameraInterfaceFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::refreshCameraTable);
    connect(clearCameraFilterButton, &QPushButton::clicked, this, &MainWindow::clearCameraFilters);
    connect(addCameraButton, &QPushButton::clicked, this, &MainWindow::addCamera);
    connect(editCameraButton, &QPushButton::clicked, this, &MainWindow::editCamera);
    connect(removeCameraButton, &QPushButton::clicked, this, &MainWindow::removeCamera);
    connect(importCameraButton, &QPushButton::clicked, this, &MainWindow::importCameras);
    connect(exportCameraButton, &QPushButton::clicked, this, &MainWindow::exportCameras);
    connect(exportFilteredCameraButton, &QPushButton::clicked, this, &MainWindow::exportFilteredCameras);
    connect(resetCameraButton, &QPushButton::clicked, this, &MainWindow::resetCameras);
    connect(m_cameraTable, &QTableWidget::cellDoubleClicked, this, [this](int, int) { editCamera(); });

    connect(m_lensSearchEdit, &QLineEdit::textChanged, this, &MainWindow::refreshLensTable);
    connect(m_lensManufacturerFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::refreshLensTable);
    connect(m_lensTypeFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::refreshLensTable);
    connect(m_lensMountFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::refreshLensTable);
    connect(clearLensFilterButton, &QPushButton::clicked, this, &MainWindow::clearLensFilters);
    connect(addLensButton, &QPushButton::clicked, this, &MainWindow::addLens);
    connect(editLensButton, &QPushButton::clicked, this, &MainWindow::editLens);
    connect(removeLensButton, &QPushButton::clicked, this, &MainWindow::removeLens);
    connect(importLensButton, &QPushButton::clicked, this, &MainWindow::importLenses);
    connect(exportLensButton, &QPushButton::clicked, this, &MainWindow::exportLenses);
    connect(exportFilteredLensButton, &QPushButton::clicked, this, &MainWindow::exportFilteredLenses);
    connect(resetLensButton, &QPushButton::clicked, this, &MainWindow::resetLenses);
    connect(m_lensTable, &QTableWidget::cellDoubleClicked, this, [this](int, int) { editLens(); });

    connect(m_lightSearchEdit, &QLineEdit::textChanged, this, &MainWindow::refreshLightTable);
    connect(m_lightManufacturerFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::refreshLightTable);
    connect(m_lightTypeFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::refreshLightTable);
    connect(m_lightModeFilter, QOverload<int>::of(&QComboBox::currentIndexChanged), this, &MainWindow::refreshLightTable);
    connect(clearLightFilterButton, &QPushButton::clicked, this, &MainWindow::clearLightFilters);
    connect(addLightButton, &QPushButton::clicked, this, &MainWindow::addLight);
    connect(editLightButton, &QPushButton::clicked, this, &MainWindow::editLight);
    connect(removeLightButton, &QPushButton::clicked, this, &MainWindow::removeLight);
    connect(importLightButton, &QPushButton::clicked, this, &MainWindow::importLights);
    connect(exportLightButton, &QPushButton::clicked, this, &MainWindow::exportLights);
    connect(exportFilteredLightButton, &QPushButton::clicked, this, &MainWindow::exportFilteredLights);
    connect(resetLightButton, &QPushButton::clicked, this, &MainWindow::resetLights);
    connect(m_lightTable, &QTableWidget::cellDoubleClicked, this, [this](int, int) { editLight(); });

    layout->addWidget(tabs, 1);

    return page;
}

QWidget *MainWindow::createReportPage()
{
    QWidget *page = new QWidget;
    QVBoxLayout *layout = new QVBoxLayout(page);
    layout->setContentsMargins(28, 24, 28, 24);
    layout->setSpacing(14);
    layout->addWidget(pageTitle(QString::fromUtf8("PDF \346\212\245\345\221\212")));

    QHBoxLayout *buttons = new QHBoxLayout;
    QPushButton *exportButton = new QPushButton(QString::fromUtf8("\345\257\274\345\207\272 PDF"));
    QPushButton *exportBomButton = new QPushButton(QString::fromUtf8("导出 BOM CSV"));
    QPushButton *recalculateButton = new QPushButton(QString::fromUtf8("\351\207\215\346\226\260\350\256\241\347\256\227"));
    exportBomButton->setObjectName(QStringLiteral("SecondaryButton"));
    recalculateButton->setObjectName(QStringLiteral("SecondaryButton"));
    connect(exportButton, &QPushButton::clicked, this, &MainWindow::exportPdf);
    connect(exportBomButton, &QPushButton::clicked, this, &MainWindow::exportBomCsv);
    connect(recalculateButton, &QPushButton::clicked, this, &MainWindow::calculate);
    buttons->addWidget(exportButton);
    buttons->addWidget(exportBomButton);
    buttons->addWidget(recalculateButton);
    buttons->addStretch();
    layout->addLayout(buttons);

    m_reportPreview = new QTextEdit;
    m_reportPreview->setReadOnly(true);
    layout->addWidget(m_reportPreview, 1);
    return page;
}

QDoubleSpinBox *MainWindow::makeSpin(double min, double max, double value, const QString &suffix, int decimals)
{
    QDoubleSpinBox *spin = new QDoubleSpinBox;
    spin->setRange(min, max);
    spin->setValue(value);
    spin->setDecimals(decimals);
    spin->setSuffix(suffix);
    spin->setKeyboardTracking(false);
    return spin;
}

void MainWindow::setActivePage(int index)
{
    if (!m_pages || index < 0 || index >= m_pages->count())
        return;
    m_pages->setCurrentIndex(index);
    for (int i = 0; i < m_navButtons.size(); ++i) {
        m_navButtons.at(i)->setProperty("active", i == index);
        m_navButtons.at(i)->style()->unpolish(m_navButtons.at(i));
        m_navButtons.at(i)->style()->polish(m_navButtons.at(i));
    }
}

void MainWindow::calculate()
{
    m_request = readRequest();
    SelectionEngine engine;
    m_results = engine.select(m_request, m_catalog.cameras(), m_catalog.lenses(), m_catalog.lights(), 20);
    refreshCalculationAssistant();
    refreshResultTable();
    refreshComparisonPage();
    refreshCatalogTables();
    refreshReportPreview();
    if (m_summaryLabel)
        m_summaryLabel->setText(m_catalog.summary());
}

SelectionRequest MainWindow::readRequest() const
{
    SelectionRequest request;
    request.objectWidthMm = m_widthSpin->value();
    request.objectHeightMm = m_heightSpin->value();
    request.placementMarginMm = m_marginSpin->value();
    request.minFeatureUm = m_minFeatureSpin->value();
    request.measurementToleranceUm = m_toleranceSpin->value();
    request.workingDistanceMm = m_wdSpin->value();
    request.heightVariationMm = m_heightVariationSpin->value();
    request.motionSpeedMmS = m_speedSpin->value();
    request.requiredFps = m_fpsSpin->value();
    request.detectionType = detectionTypeFromIndex(m_detectionCombo->currentIndex());
    request.surfaceType = surfaceTypeFromIndex(m_surfaceCombo->currentIndex());
    request.reflective = m_reflectiveCheck->isChecked();
    request.preferMono = m_monoCheck->isChecked();
    request.allowTelecentric = m_allowTelecentricCheck->isChecked();
    return request;
}

void MainWindow::refreshCalculationAssistant()
{
    if (!m_assistantSummaryLabel || !m_assistantCameraTable || !m_assistantLensTable || !m_assistantDetails)
        return;

    const RequirementEstimate requirement = CalculationAssistant::estimateRequirement(m_request);
    m_assistantCameraEstimates = CalculationAssistant::estimateCameras(m_request, m_catalog.cameras(), 12);

    m_assistantSummaryLabel->setText(QString::fromUtf8("\351\234\200\346\261\202 FOV\357\274\232%1 x %2 mm\357\274\214\347\233\256\346\240\207\347\211\251\346\226\271\345\203\217\347\264\240\357\274\232%3 um/px\357\274\214\347\233\270\346\234\272\344\270\213\351\231\220\357\274\232%4 x %5\357\274\210%6 MP\357\274\211")
        .arg(requirement.requiredFovWidthMm, 0, 'f', 2)
        .arg(requirement.requiredFovHeightMm, 0, 'f', 2)
        .arg(requirement.targetObjectPixelUm, 0, 'f', 2)
        .arg(requirement.requiredResolutionX)
        .arg(requirement.requiredResolutionY)
        .arg(requirement.requiredMegapixels, 0, 'f', 2));

    m_assistantCameraTable->setSortingEnabled(false);
    m_assistantCameraTable->setRowCount(m_assistantCameraEstimates.size());
    for (int row = 0; row < m_assistantCameraEstimates.size(); ++row) {
        const CameraCalculationEstimate &estimate = m_assistantCameraEstimates.at(row);
        const CameraSpec &camera = estimate.camera;
        QStringList verdict;
        verdict.append(estimate.meetsSampling
            ? QString::fromUtf8("\345\203\217\347\264\240\346\273\241\350\266\263")
            : QString::fromUtf8("\345\203\217\347\264\240\344\270\215\350\266\263"));
        verdict.append(estimate.meetsFps
            ? QString::fromUtf8("\345\270\247\347\216\207\346\273\241\350\266\263")
            : QString::fromUtf8("\345\270\247\347\216\207\344\270\215\350\266\263"));
        if (estimate.globalShutterRecommended)
            verdict.append(QString::fromUtf8("\345\273\272\350\256\256\345\205\250\345\261\200\345\277\253\351\227\250"));

        m_assistantCameraTable->setItem(row, 0, indexedItem(camera.model, row));
        m_assistantCameraTable->setItem(row, 1, item(camera.manufacturer));
        m_assistantCameraTable->setItem(row, 2, item(QStringLiteral("%1 x %2").arg(camera.resolutionX).arg(camera.resolutionY)));
        m_assistantCameraTable->setItem(row, 3, item(QStringLiteral("%1 um").arg(camera.pixelSizeUm, 0, 'f', 2)));
        m_assistantCameraTable->setItem(row, 4, item(QStringLiteral("%1 mm").arg(estimate.sensorDiagonalMm, 0, 'f', 2)));
        m_assistantCameraTable->setItem(row, 5, item(QStringLiteral("%1 um/px").arg(estimate.objectPixelSizeUm, 0, 'f', 2)));
        m_assistantCameraTable->setItem(row, 6, item(QStringLiteral("%1 mm").arg(estimate.fixedFocalLengthMm, 0, 'f', 1)));
        m_assistantCameraTable->setItem(row, 7, item(estimate.telecentricFeasible
            ? QStringLiteral("%1 - %2x").arg(estimate.telecentricPmagMin, 0, 'f', 3).arg(estimate.telecentricPmagMax, 0, 'f', 3)
            : QString::fromUtf8("\344\270\215\345\273\272\350\256\256")));
        m_assistantCameraTable->setItem(row, 8, item(QStringLiteral("%1 MB/s").arg(estimate.bandwidthRequiredMBps, 0, 'f', 1)));
        m_assistantCameraTable->setItem(row, 9, item(verdict.join(QString::fromUtf8("\357\274\233"))));
    }
    m_assistantCameraTable->setSortingEnabled(true);

    if (m_assistantCameraEstimates.isEmpty()) {
        m_assistantSelectedCameraRow = -1;
    } else if (m_assistantSelectedCameraRow < 0 || m_assistantSelectedCameraRow >= m_assistantCameraEstimates.size()) {
        m_assistantSelectedCameraRow = 0;
    }
    if (m_assistantSelectedCameraRow >= 0)
        selectRowBySourceIndex(m_assistantCameraTable, m_assistantSelectedCameraRow);
    refreshAssistantLensTable();
}

void MainWindow::refreshAssistantLensTable()
{
    if (!m_assistantLensTable || !m_assistantDetails)
        return;

    const RequirementEstimate requirement = CalculationAssistant::estimateRequirement(m_request);
    QString details;
    details += QString::fromUtf8("\345\217\202\346\225\260\350\246\201\346\261\202\n");
    details += QString::fromUtf8("- \346\234\200\344\275\216\345\210\206\350\276\250\347\216\207\357\274\232%1 x %2\357\274\214\345\273\272\350\256\256\344\270\215\344\275\216\344\272\216 %3 MP\343\200\202\n")
        .arg(requirement.requiredResolutionX)
        .arg(requirement.requiredResolutionY)
        .arg(requirement.requiredMegapixels, 0, 'f', 2);
    details += QString::fromUtf8("- 12 bit \345\216\237\345\247\213\346\225\260\346\215\256\345\270\246\345\256\275\344\274\260\347\256\227\357\274\232%1 MB/s @ %2 fps\343\200\202\n")
        .arg(requirement.requiredBandwidthMBps12Bit, 0, 'f', 1)
        .arg(m_request.requiredFps, 0, 'f', 1);
    details += QString::fromUtf8("- \351\225\234\345\244\264\347\261\273\345\236\213\345\200\276\345\220\221\357\274\232%1\343\200\202\n")
        .arg(requirement.telecentricPreferred
            ? QString::fromUtf8("\351\253\230\347\262\276\345\272\246/\351\253\230\345\272\246\346\263\242\345\212\250\357\274\214\344\274\230\345\205\210\350\257\204\344\274\260\350\277\234\345\277\203\351\225\234\345\244\264")
            : QString::fromUtf8("\346\231\256\351\200\232\345\267\245\344\270\232\351\225\234\345\244\264\345\217\257\344\273\245\345\205\210\347\262\227\347\256\227"));
    details += QString::fromUtf8("- \350\277\220\345\212\250\346\233\235\345\205\211\344\270\212\351\231\220\357\274\232%1\343\200\202\n")
        .arg(requirement.hasMotionConstraint
            ? QStringLiteral("%1 us").arg(requirement.maxExposureUsForOnePixelBlur, 0, 'f', 1)
            : QString::fromUtf8("\346\227\240\350\277\220\345\212\250\346\250\241\347\263\212\347\272\246\346\235\237"));

    if (m_assistantSelectedCameraRow < 0 || m_assistantSelectedCameraRow >= m_assistantCameraEstimates.size()) {
        m_assistantLensTable->setRowCount(0);
        details += QString::fromUtf8("\n\351\225\234\345\244\264\345\200\231\351\200\211\357\274\232\346\232\202\346\227\240\345\217\257\347\224\250\347\233\270\346\234\272\344\274\260\347\256\227\343\200\202");
        m_assistantDetails->setPlainText(details);
        return;
    }

    const CameraSpec camera = m_assistantCameraEstimates.at(m_assistantSelectedCameraRow).camera;
    m_assistantLensEstimates = CalculationAssistant::estimateLenses(m_request, camera, m_catalog.lenses(), 12);
    m_assistantLensTable->setSortingEnabled(false);
    m_assistantLensTable->setRowCount(m_assistantLensEstimates.size());
    for (int row = 0; row < m_assistantLensEstimates.size(); ++row) {
        const LensCalculationEstimate &estimate = m_assistantLensEstimates.at(row);
        const LensSpec &lens = estimate.lens;
        const QString focalOrPmag = lens.isTelecentric()
            ? QStringLiteral("%1x").arg(estimate.magnification, 0, 'f', 3)
            : QStringLiteral("%1 mm").arg(lens.focalLengthMm, 0, 'f', 1);
        const QString wdOrDof = lens.isTelecentric()
            ? QStringLiteral("WD %1 / DOF %2").arg(lens.nominalWorkingDistanceMm, 0, 'f', 0).arg(lens.dofMm, 0, 'f', 1)
            : QStringLiteral("min WD %1 / DOF %2").arg(lens.minWorkingDistanceMm, 0, 'f', 0).arg(estimate.estimatedDofMm, 0, 'f', 1);
        QStringList verdict;
        verdict.append(estimate.fovOk ? QString::fromUtf8("FOV \346\273\241\350\266\263") : QString::fromUtf8("FOV \344\270\215\350\266\263"));
        verdict.append(estimate.samplingOk ? QString::fromUtf8("\345\203\217\347\264\240\346\273\241\350\266\263") : QString::fromUtf8("\345\203\217\347\264\240\344\270\215\350\266\263"));
        if (!estimate.mountOk)
            verdict.append(QString::fromUtf8("\346\216\245\345\217\243\344\270\215\345\214\271\351\205\215"));
        if (!estimate.workingDistanceOk)
            verdict.append(QStringLiteral("WD"));
        if (!estimate.dofOk)
            verdict.append(QStringLiteral("DOF"));

        m_assistantLensTable->setItem(row, 0, indexedItem(lens.typeLabel(), row));
        m_assistantLensTable->setItem(row, 1, item(lens.manufacturer));
        m_assistantLensTable->setItem(row, 2, item(lens.model));
        m_assistantLensTable->setItem(row, 3, item(lens.lensMount));
        m_assistantLensTable->setItem(row, 4, item(focalOrPmag));
        m_assistantLensTable->setItem(row, 5, item(QStringLiteral("%1 x %2").arg(estimate.effectiveFovWidthMm, 0, 'f', 1).arg(estimate.effectiveFovHeightMm, 0, 'f', 1)));
        m_assistantLensTable->setItem(row, 6, item(QStringLiteral("%1 um").arg(estimate.objectPixelSizeUm, 0, 'f', 2)));
        m_assistantLensTable->setItem(row, 7, item(wdOrDof));
        m_assistantLensTable->setItem(row, 8, item(QStringLiteral("%1 mm").arg(lens.imageCircleMm, 0, 'f', 1)));
        m_assistantLensTable->setItem(row, 9, item(verdict.join(QString::fromUtf8("\357\274\233"))));
    }
    m_assistantLensTable->setSortingEnabled(true);

    details += QString::fromUtf8("\n\345\275\223\345\211\215\351\225\234\345\244\264\345\200\231\351\200\211\345\237\272\344\272\216\347\233\270\346\234\272\357\274\232%1\343\200\202\n")
        .arg(productLabel(camera.manufacturer, camera.model));
    if (!m_assistantLensEstimates.isEmpty()) {
        const LensCalculationEstimate &top = m_assistantLensEstimates.first();
        details += QString::fromUtf8("- \351\246\226\351\200\211\351\225\234\345\244\264\357\274\232%1 %2\357\274\214FOV %3 x %4 mm\357\274\214\347\211\251\346\226\271\345\203\217\347\264\240 %5 um/px\343\200\202\n")
            .arg(top.lens.manufacturer, top.lens.model)
            .arg(top.effectiveFovWidthMm, 0, 'f', 2)
            .arg(top.effectiveFovHeightMm, 0, 'f', 2)
            .arg(top.objectPixelSizeUm, 0, 'f', 2);
        details += QString::fromUtf8("- 工程估算：DOF %1 mm，畸变边缘误差约 %2 um。\n")
            .arg(top.estimatedDofMm, 0, 'f', 2)
            .arg(top.distortionErrorUm, 0, 'f', 2);
        details += QString::fromUtf8("- \345\205\254\345\274\217\357\274\232%1\343\200\202\n").arg(top.formulaSummary);
        details += QString::fromUtf8("- \346\216\250\350\215\220\347\220\206\347\224\261\357\274\232%1\343\200\202\n")
            .arg(top.reasons.isEmpty() ? QString::fromUtf8("\346\214\211\347\273\274\345\220\210\345\217\202\346\225\260\346\216\222\345\220\215") : top.reasons.join(QString::fromUtf8("\357\274\233")));
        details += QString::fromUtf8("- \351\243\216\351\231\251\357\274\232%1")
            .arg(top.risks.isEmpty() ? QString::fromUtf8("\346\227\240\344\270\273\350\246\201\351\243\216\351\231\251") : top.risks.join(QString::fromUtf8("\357\274\233")));
    } else {
        details += QString::fromUtf8("- \346\262\241\346\234\211\347\254\246\345\220\210\345\275\223\345\211\215\351\231\220\345\210\266\347\232\204\351\225\234\345\244\264\345\200\231\351\200\211\343\200\202");
    }
    m_assistantDetails->setPlainText(details);
}

void MainWindow::refreshResultTable()
{
    if (!m_resultTable)
        return;

    m_resultSummaryLabel->setText(QString::fromUtf8("\351\234\200\346\261\202 FOV\357\274\232%1 x %2 mm\357\274\214\347\233\256\346\240\207\347\211\251\346\226\271\345\203\217\347\264\240\357\274\232%3 um/px\357\274\214\345\200\231\351\200\211\346\226\271\346\241\210\357\274\232%4 \344\270\252")
        .arg(SelectionEngine::requiredFovWidth(m_request), 0, 'f', 2)
        .arg(SelectionEngine::requiredFovHeight(m_request), 0, 'f', 2)
        .arg(SelectionEngine::targetObjectPixelUm(m_request), 0, 'f', 2)
        .arg(m_results.size()));

    m_resultTable->setSortingEnabled(false);
    m_resultTable->setRowCount(m_results.size());
    for (int row = 0; row < m_results.size(); ++row) {
        const SelectionResult &r = m_results.at(row);
        m_resultTable->setItem(row, 0, indexedItem(r.isTelecentric() ? QString::fromUtf8("\350\277\234\345\277\203") : QString::fromUtf8("\346\231\256\351\200\232"), row));
        m_resultTable->setItem(row, 1, item(number(r.score.score, 1)));
        m_resultTable->setItem(row, 2, item(productLabel(r.camera.manufacturer, r.camera.model)));
        m_resultTable->setItem(row, 3, item(productLabel(r.lens.manufacturer, r.lens.model)));
        m_resultTable->setItem(row, 4, item(productLabel(r.light.manufacturer, r.light.model)));
        m_resultTable->setItem(row, 5, item(QStringLiteral("%1 x %2")
            .arg(r.effectiveFovWidthMm, 0, 'f', 1)
            .arg(r.effectiveFovHeightMm, 0, 'f', 1)));
        m_resultTable->setItem(row, 6, item(QStringLiteral("%1 um").arg(r.objectPixelSizeUm, 0, 'f', 2)));
        m_resultTable->setItem(row, 7, item(r.isTelecentric()
            ? QStringLiteral("%1x").arg(r.magnification, 0, 'f', 3)
            : QStringLiteral("%1 mm").arg(r.lens.focalLengthMm, 0, 'f', 1)));
        m_resultTable->setItem(row, 8, item(r.isTelecentric()
            ? QStringLiteral("WD %1 / DOF %2").arg(r.lens.nominalWorkingDistanceMm, 0, 'f', 0).arg(r.estimatedDofMm, 0, 'f', 1)
            : QStringLiteral("min WD %1 / DOF %2").arg(r.lens.minWorkingDistanceMm, 0, 'f', 0).arg(r.estimatedDofMm, 0, 'f', 1)));
        m_resultTable->setItem(row, 9, item(r.score.risks.isEmpty()
            ? QString::fromUtf8("\346\227\240\344\270\273\350\246\201\351\243\216\351\231\251")
            : r.score.risks.join(QString::fromUtf8("\357\274\233"))));
    }
    m_resultTable->setSortingEnabled(true);
    if (!m_results.isEmpty()) {
        selectRowBySourceIndex(m_resultTable, 0);
        refreshResultDetails(0);
    }
}

void MainWindow::refreshResultDetails(int row)
{
    if (!m_resultDetails || row < 0 || row >= m_results.size())
        return;

    const SelectionResult &r = m_results.at(row);
    QString text;
    text += QStringLiteral("<h3>") + r.schemeTitle + QString::fromUtf8("\357\274\232")
        + productLabel(r.camera.manufacturer, r.camera.model) + QStringLiteral(" + ")
        + productLabel(r.lens.manufacturer, r.lens.model) + QStringLiteral(" + ")
        + productLabel(r.light.manufacturer, r.light.model) + QStringLiteral("</h3>");
    text += QString::fromUtf8("<p><b>\345\205\254\345\274\217\357\274\232</b>%1</p>").arg(r.formulaSummary);
    text += QString::fromUtf8("<p><b>\346\234\211\346\225\210 FOV\357\274\232</b>%1 x %2 mm\357\274\233<b>\347\211\251\346\226\271\345\203\217\347\264\240\357\274\232</b>%3 um/px\357\274\233<b>\346\216\245\345\217\243\345\270\246\345\256\275\357\274\232</b>%4 MB/s\343\200\202</p>")
        .arg(r.effectiveFovWidthMm, 0, 'f', 2)
        .arg(r.effectiveFovHeightMm, 0, 'f', 2)
        .arg(r.objectPixelSizeUm, 0, 'f', 2)
        .arg(r.bandwidthRequiredMBps, 0, 'f', 1);
    text += QString::fromUtf8("<p><b>接口/存储：</b>单帧 %1 MB；接口余量 %2 MB/s；带宽利用率 %3%；原始存储约 %4 GB/h；镜头 MP 利用率 %5%。</p>")
        .arg(r.framePayloadMB, 0, 'f', 2)
        .arg(r.interfaceCapacityMBps, 0, 'f', 1)
        .arg(r.bandwidthUtilizationPercent, 0, 'f', 0)
        .arg(r.storagePerHourGB, 0, 'f', 0)
        .arg(r.lensMegapixelUtilizationPercent, 0, 'f', 0);
    if (r.maxExposureUsForOnePixelBlur > 0.0) {
        text += QString::fromUtf8("<p><b>运动模糊：</b>建议曝光不高于 %1 us 才能控制在约 1 个目标物方像素内。</p>")
            .arg(r.maxExposureUsForOnePixelBlur, 0, 'f', 1);
    }
    text += QString::fromUtf8("<p><b>工程估算：</b>DOF %1 mm；畸变边缘误差约 %2 um；光源覆盖余量 %3%。</p>")
        .arg(r.estimatedDofMm, 0, 'f', 2)
        .arg(r.distortionErrorUm, 0, 'f', 2)
        .arg(r.lightCoverageMarginPercent, 0, 'f', 0);
    if (r.isTelecentric()) {
        text += QString::fromUtf8("<p><b>\350\277\234\345\277\203\345\217\202\346\225\260\357\274\232</b>PMAG %1x\357\274\214\346\240\207\347\247\260 WD %2 mm\357\274\214\350\277\234\345\277\203\345\272\246 %3 deg\357\274\214\347\225\270\345\217\230 %4%\357\274\214DOF %5 mm\357\274\214\346\256\213\344\275\231\350\247\206\345\267\256\344\274\260\347\256\227 %6 um\343\200\202</p>")
            .arg(r.magnification, 0, 'f', 3)
            .arg(r.lens.nominalWorkingDistanceMm, 0, 'f', 1)
            .arg(r.lens.telecentricityDeg, 0, 'f', 3)
            .arg(r.lens.distortionPercent, 0, 'f', 3)
            .arg(r.estimatedDofMm, 0, 'f', 2)
            .arg(r.residualTelecentricErrorUm, 0, 'f', 2);
    }
    text += QString::fromUtf8("<p><b>\346\216\250\350\215\220\347\220\206\347\224\261\357\274\232</b>%1</p>").arg(r.score.reasons.join(QString::fromUtf8("\357\274\233")));
    text += QString::fromUtf8("<p><b>\351\243\216\351\231\251\346\217\220\347\244\272\357\274\232</b>%1</p>").arg(r.score.risks.isEmpty()
        ? QString::fromUtf8("\346\227\240\344\270\273\350\246\201\351\243\216\351\231\251\357\274\214\344\273\215\345\273\272\350\256\256\347\273\223\345\220\210\345\216\202\345\256\266 MTF/DOF \344\270\216\347\216\260\345\234\272\345\205\211\346\272\220\345\256\236\346\265\213\347\241\256\350\256\244\343\200\202")
        : r.score.risks.join(QString::fromUtf8("\357\274\233")));
    m_resultDetails->setHtml(text);
}

void MainWindow::refreshComparisonPage()
{
    if (!m_compareTable)
        return;

    const int count = qMin(5, m_results.size());
    m_compareTable->setSortingEnabled(false);
    m_compareTable->setRowCount(count);
    for (int row = 0; row < count; ++row) {
        const SelectionResult &r = m_results.at(row);
        m_compareTable->setItem(row, 0, indexedItem(QStringLiteral("#%1 %2")
            .arg(row + 1)
            .arg(r.isTelecentric() ? QString::fromUtf8("远心") : QString::fromUtf8("普通")), row));
        m_compareTable->setItem(row, 1, item(number(r.score.score, 1)));
        m_compareTable->setItem(row, 2, item(productLabel(r.camera.manufacturer, r.camera.model)));
        m_compareTable->setItem(row, 3, item(productLabel(r.lens.manufacturer, r.lens.model)));
        m_compareTable->setItem(row, 4, item(productLabel(r.light.manufacturer, r.light.model)));
        m_compareTable->setItem(row, 5, item(QStringLiteral("%1 x %2")
            .arg(r.effectiveFovWidthMm, 0, 'f', 1)
            .arg(r.effectiveFovHeightMm, 0, 'f', 1)));
        m_compareTable->setItem(row, 6, item(QStringLiteral("%1 um").arg(r.objectPixelSizeUm, 0, 'f', 2)));
        m_compareTable->setItem(row, 7, item(exposureText(r.maxExposureUsForOnePixelBlur)));
        m_compareTable->setItem(row, 8, item(QStringLiteral("%1% / %2 GB/h")
            .arg(r.bandwidthUtilizationPercent, 0, 'f', 0)
            .arg(r.storagePerHourGB, 0, 'f', 0)));
        m_compareTable->setItem(row, 9, item(QStringLiteral("%1 mm / %2 um")
            .arg(r.estimatedDofMm, 0, 'f', 2)
            .arg(r.distortionErrorUm, 0, 'f', 1)));
        m_compareTable->setItem(row, 10, item(QStringLiteral("%1%")
            .arg(r.lightCoverageMarginPercent, 0, 'f', 0)));
        m_compareTable->setItem(row, 11, item(riskSummary(r)));
    }
    m_compareTable->setSortingEnabled(true);

    if (count > 0) {
        selectRowBySourceIndex(m_compareTable, 0);
        refreshComparisonDetails(0);
    } else if (m_compareDetails) {
        m_compareDetails->setPlainText(QString::fromUtf8("暂无方案，请先计算推荐结果。"));
    }
}

void MainWindow::refreshComparisonDetails(int row)
{
    if (!m_compareDetails || row < 0 || row >= m_results.size())
        return;

    const SelectionResult &r = m_results.at(row);
    QString text;
    text += QStringLiteral("<h3>") + QString::fromUtf8("方案 #") + QString::number(row + 1)
        + QStringLiteral(" - ") + r.schemeTitle + QStringLiteral("</h3>");
    text += QString::fromUtf8("<p><b>BOM：</b>相机 %1；镜头 %2；光源 %3。</p>")
        .arg(productLabel(r.camera.manufacturer, r.camera.model),
             productLabel(r.lens.manufacturer, r.lens.model),
             productLabel(r.light.manufacturer, r.light.model));
    text += QString::fromUtf8("<p><b>计算结果：</b>FOV %1 x %2 mm；物方像素 %3 um/px；曝光上限 %4；DOF %5 mm；畸变误差 %6 um。</p>")
        .arg(r.effectiveFovWidthMm, 0, 'f', 2)
        .arg(r.effectiveFovHeightMm, 0, 'f', 2)
        .arg(r.objectPixelSizeUm, 0, 'f', 2)
        .arg(exposureText(r.maxExposureUsForOnePixelBlur))
        .arg(r.estimatedDofMm, 0, 'f', 2)
        .arg(r.distortionErrorUm, 0, 'f', 2);
    text += QString::fromUtf8("<p><b>接口与存储：</b>单帧原始数据 %1 MB；吞吐 %2 MB/s；接口余量 %3 MB/s；带宽利用率 %4%；原始存储约 %5 GB/h。</p>")
        .arg(r.framePayloadMB, 0, 'f', 2)
        .arg(r.bandwidthRequiredMBps, 0, 'f', 1)
        .arg(r.interfaceCapacityMBps, 0, 'f', 1)
        .arg(r.bandwidthUtilizationPercent, 0, 'f', 0)
        .arg(r.storagePerHourGB, 0, 'f', 0);
    text += QString::fromUtf8("<p><b>镜头/光源余量：</b>镜头 MP 利用率 %1%；光源覆盖余量 %2%。</p>")
        .arg(r.lensMegapixelUtilizationPercent, 0, 'f', 0)
        .arg(r.lightCoverageMarginPercent, 0, 'f', 0);
    text += QString::fromUtf8("<p><b>推荐理由：</b>%1</p>").arg(r.score.reasons.join(QString::fromUtf8("；")));
    text += QString::fromUtf8("<p><b>风险：</b>%1</p>").arg(riskSummary(r));
    m_compareDetails->setHtml(text);
}

void MainWindow::refreshCatalogTables()
{
    if (!m_cameraTable || !m_lensTable || !m_lightTable)
        return;
    refreshCatalogFilterOptions();
    refreshCameraTable();
    refreshLensTable();
    refreshLightTable();
}

void MainWindow::refreshCatalogFilterOptions()
{
    QStringList cameraManufacturers;
    QStringList cameraInterfaces;
    for (const CameraSpec &camera : m_catalog.cameras()) {
        cameraManufacturers.append(camera.manufacturer);
        cameraInterfaces.append(camera.interfaceType);
    }
    fillComboPreservingText(m_cameraManufacturerFilter, allManufacturersText(), cameraManufacturers);
    fillComboPreservingText(m_cameraInterfaceFilter, allInterfacesText(), cameraInterfaces);

    QStringList lensManufacturers;
    QStringList lensTypes;
    QStringList lensMounts;
    for (const LensSpec &lens : m_catalog.lenses()) {
        lensManufacturers.append(lens.manufacturer);
        lensTypes.append(lens.typeLabel());
        lensMounts.append(lens.lensMount);
    }
    fillComboPreservingText(m_lensManufacturerFilter, allManufacturersText(), lensManufacturers);
    fillComboPreservingText(m_lensTypeFilter, allTypesText(), lensTypes);
    fillComboPreservingText(m_lensMountFilter, allMountsText(), lensMounts);

    QStringList lightManufacturers;
    QStringList lightTypes;
    QStringList lightModes;
    for (const LightSpec &light : m_catalog.lights()) {
        lightManufacturers.append(light.manufacturer);
        lightTypes.append(light.typeLabel());
        lightModes.append(light.mode);
    }
    fillComboPreservingText(m_lightManufacturerFilter, allManufacturersText(), lightManufacturers);
    fillComboPreservingText(m_lightTypeFilter, allTypesText(), lightTypes);
    fillComboPreservingText(m_lightModeFilter, allModesText(), lightModes);
}

void MainWindow::refreshCameraTable()
{
    if (!m_cameraTable)
        return;
    const QString previousKey = selectedCatalogKey(m_cameraTable);
    const int previousRow = m_cameraTable->currentRow();
    m_cameraTable->setSortingEnabled(false);
    m_cameraTable->setColumnCount(10);
    m_cameraTable->setHorizontalHeaderLabels({QString::fromUtf8("\345\236\213\345\217\267"), QString::fromUtf8("\345\216\202\345\256\266"), QString::fromUtf8("\345\210\206\350\276\250\347\216\207"), QString::fromUtf8("\345\203\217\345\205\203"),
        QString::fromUtf8("\344\274\240\346\204\237\345\231\250"), QString::fromUtf8("\351\235\266\351\235\242"), QString::fromUtf8("\345\277\253\351\227\250"), QStringLiteral("fps"),
        QString::fromUtf8("\346\216\245\345\217\243"), QString::fromUtf8("\351\225\234\345\244\264\345\217\243")});
    m_cameraTable->setRowCount(0);
    m_cameraRowMap.clear();
    const QString search = m_cameraSearchEdit ? m_cameraSearchEdit->text() : QString();
    const QString manufacturer = m_cameraManufacturerFilter && m_cameraManufacturerFilter->currentIndex() > 0
        ? m_cameraManufacturerFilter->currentText() : QString();
    const QString interfaceType = m_cameraInterfaceFilter && m_cameraInterfaceFilter->currentIndex() > 0
        ? m_cameraInterfaceFilter->currentText() : QString();
    for (int i = 0; i < m_catalog.cameras().size(); ++i) {
        const CameraSpec &c = m_catalog.cameras().at(i);
        if (!manufacturer.isEmpty() && c.manufacturer != manufacturer)
            continue;
        if (!interfaceType.isEmpty() && c.interfaceType != interfaceType)
            continue;
        if (!textMatches(search, {c.model, c.manufacturer, c.interfaceType, c.lensMount, c.sensorFormat}))
            continue;
        const int row = m_cameraTable->rowCount();
        m_cameraTable->insertRow(row);
        m_cameraRowMap.append(i);
        m_cameraTable->setItem(row, 0, indexedItem(c.model, i));
        m_cameraTable->setItem(row, 1, item(c.manufacturer));
        m_cameraTable->setItem(row, 2, item(QStringLiteral("%1 x %2").arg(c.resolutionX).arg(c.resolutionY)));
        m_cameraTable->setItem(row, 3, item(QStringLiteral("%1 um").arg(c.pixelSizeUm, 0, 'f', 2)));
        m_cameraTable->setItem(row, 4, item(QStringLiteral("%1 x %2 mm").arg(c.sensorWidthMm(), 0, 'f', 2).arg(c.sensorHeightMm(), 0, 'f', 2)));
        m_cameraTable->setItem(row, 5, item(c.sensorFormat));
        m_cameraTable->setItem(row, 6, item(c.shutterType));
        m_cameraTable->setItem(row, 7, item(number(c.maxFps, 1)));
        m_cameraTable->setItem(row, 8, item(c.interfaceType));
        m_cameraTable->setItem(row, 9, item(c.lensMount));
    }
    m_cameraTable->setSortingEnabled(true);
    selectCatalogRow(m_cameraTable, previousKey, previousRow);
}

void MainWindow::refreshLensTable()
{
    if (!m_lensTable)
        return;
    const QString previousKey = selectedCatalogKey(m_lensTable);
    const int previousRow = m_lensTable->currentRow();
    m_lensTable->setSortingEnabled(false);
    m_lensTable->setColumnCount(12);
    m_lensTable->setHorizontalHeaderLabels({QString::fromUtf8("\345\236\213\345\217\267"), QString::fromUtf8("\345\216\202\345\256\266"), QString::fromUtf8("\347\261\273\345\236\213"), QString::fromUtf8("\346\216\245\345\217\243"),
        QString::fromUtf8("\347\204\246\350\267\235"), QStringLiteral("PMAG"), QStringLiteral("WD"), QString::fromUtf8("\345\203\217\345\234\206"),
        QString::fromUtf8("\350\277\234\345\277\203\345\272\246"), QString::fromUtf8("\347\225\270\345\217\230"), QStringLiteral("DOF"), QString::fromUtf8("\345\220\214\350\275\264")});
    m_lensTable->setRowCount(0);
    m_lensRowMap.clear();
    const QString search = m_lensSearchEdit ? m_lensSearchEdit->text() : QString();
    const QString manufacturer = m_lensManufacturerFilter && m_lensManufacturerFilter->currentIndex() > 0
        ? m_lensManufacturerFilter->currentText() : QString();
    const QString type = m_lensTypeFilter && m_lensTypeFilter->currentIndex() > 0
        ? m_lensTypeFilter->currentText() : QString();
    const QString mount = m_lensMountFilter && m_lensMountFilter->currentIndex() > 0
        ? m_lensMountFilter->currentText() : QString();
    for (int i = 0; i < m_catalog.lenses().size(); ++i) {
        const LensSpec &l = m_catalog.lenses().at(i);
        if (!manufacturer.isEmpty() && l.manufacturer != manufacturer)
            continue;
        if (!type.isEmpty() && l.typeLabel() != type)
            continue;
        if (!mount.isEmpty() && l.lensMount != mount)
            continue;
        if (!textMatches(search, {l.model, l.manufacturer, l.typeLabel(), l.lensMount, l.notes}))
            continue;
        const int row = m_lensTable->rowCount();
        m_lensTable->insertRow(row);
        m_lensRowMap.append(i);
        m_lensTable->setItem(row, 0, indexedItem(l.model, i));
        m_lensTable->setItem(row, 1, item(l.manufacturer));
        m_lensTable->setItem(row, 2, item(l.typeLabel()));
        m_lensTable->setItem(row, 3, item(l.lensMount));
        m_lensTable->setItem(row, 4, item(l.isTelecentric() ? QStringLiteral("-") : QStringLiteral("%1 mm").arg(l.focalLengthMm, 0, 'f', 1)));
        m_lensTable->setItem(row, 5, item(l.isTelecentric() ? QStringLiteral("%1x").arg(l.pmag, 0, 'f', 3) : QStringLiteral("-")));
        m_lensTable->setItem(row, 6, item(l.isTelecentric() ? QStringLiteral("%1 mm").arg(l.nominalWorkingDistanceMm, 0, 'f', 1) : QStringLiteral(">=%1 mm").arg(l.minWorkingDistanceMm, 0, 'f', 1)));
        m_lensTable->setItem(row, 7, item(QStringLiteral("%1 mm").arg(l.imageCircleMm, 0, 'f', 1)));
        m_lensTable->setItem(row, 8, item(l.isTelecentric() ? QStringLiteral("%1 deg").arg(l.telecentricityDeg, 0, 'f', 3) : QStringLiteral("-")));
        m_lensTable->setItem(row, 9, item(QStringLiteral("%1%").arg(l.distortionPercent, 0, 'f', 3)));
        m_lensTable->setItem(row, 10, item(l.isTelecentric() ? QStringLiteral("%1 mm").arg(l.dofMm, 0, 'f', 1) : QStringLiteral("-")));
        m_lensTable->setItem(row, 11, item(boolLabel(l.coaxialIllumination)));
    }
    m_lensTable->setSortingEnabled(true);
    selectCatalogRow(m_lensTable, previousKey, previousRow);
}

void MainWindow::refreshLightTable()
{
    if (!m_lightTable)
        return;
    const QString previousKey = selectedCatalogKey(m_lightTable);
    const int previousRow = m_lightTable->currentRow();
    m_lightTable->setSortingEnabled(false);
    m_lightTable->setColumnCount(8);
    m_lightTable->setHorizontalHeaderLabels({QString::fromUtf8("\345\236\213\345\217\267"), QString::fromUtf8("\345\216\202\345\256\266"), QString::fromUtf8("\347\261\273\345\236\213"), QString::fromUtf8("\351\242\234\350\211\262"),
        QString::fromUtf8("\346\263\242\351\225\277"), QString::fromUtf8("\346\250\241\345\274\217"), QString::fromUtf8("\346\234\211\346\225\210\351\235\242\347\247\257"), QString::fromUtf8("\351\200\202\347\224\250\345\234\272\346\231\257")});
    m_lightTable->setRowCount(0);
    m_lightRowMap.clear();
    const QString search = m_lightSearchEdit ? m_lightSearchEdit->text() : QString();
    const QString manufacturer = m_lightManufacturerFilter && m_lightManufacturerFilter->currentIndex() > 0
        ? m_lightManufacturerFilter->currentText() : QString();
    const QString type = m_lightTypeFilter && m_lightTypeFilter->currentIndex() > 0
        ? m_lightTypeFilter->currentText() : QString();
    const QString mode = m_lightModeFilter && m_lightModeFilter->currentIndex() > 0
        ? m_lightModeFilter->currentText() : QString();
    for (int i = 0; i < m_catalog.lights().size(); ++i) {
        const LightSpec &l = m_catalog.lights().at(i);
        if (!manufacturer.isEmpty() && l.manufacturer != manufacturer)
            continue;
        if (!type.isEmpty() && l.typeLabel() != type)
            continue;
        if (!mode.isEmpty() && l.mode != mode)
            continue;
        if (!textMatches(search, {l.model, l.manufacturer, l.typeLabel(), l.color, l.mode, l.bestFor}))
            continue;
        const int row = m_lightTable->rowCount();
        m_lightTable->insertRow(row);
        m_lightRowMap.append(i);
        m_lightTable->setItem(row, 0, indexedItem(l.model, i));
        m_lightTable->setItem(row, 1, item(l.manufacturer));
        m_lightTable->setItem(row, 2, item(l.typeLabel()));
        m_lightTable->setItem(row, 3, item(l.color));
        m_lightTable->setItem(row, 4, item(l.wavelengthNm > 0 ? QStringLiteral("%1 nm").arg(l.wavelengthNm) : QString::fromUtf8("\345\256\275\350\260\261")));
        m_lightTable->setItem(row, 5, item(l.mode));
        m_lightTable->setItem(row, 6, item(QStringLiteral("%1 x %2 mm").arg(l.activeWidthMm, 0, 'f', 0).arg(l.activeHeightMm, 0, 'f', 0)));
        m_lightTable->setItem(row, 7, item(l.bestFor));
    }
    m_lightTable->setSortingEnabled(true);
    selectCatalogRow(m_lightTable, previousKey, previousRow);
}

void MainWindow::refreshReportPreview()
{
    if (!m_reportPreview)
        return;
    QString text;
    text += QString::fromUtf8("PDF \345\260\206\345\214\205\345\220\253\357\274\232\n");
    text += QString::fromUtf8("- \351\234\200\346\261\202\350\276\223\345\205\245\343\200\201\347\233\256\346\240\207 FOV\343\200\201\347\233\256\346\240\207\347\211\251\346\226\271\345\203\217\347\264\240\n");
    text += QString::fromUtf8("- \346\231\256\351\200\232\351\225\234\345\244\264\344\270\216\350\277\234\345\277\203\351\225\234\345\244\264\347\232\204\345\205\263\351\224\256\345\205\254\345\274\217\n");
    text += QString::fromUtf8("- Top 推荐方案、方案对比、BOM、带宽/存储/曝光/DOF/畸变风险\n\n");
    if (!m_results.isEmpty()) {
        const SelectionResult &top = m_results.first();
        text += QString::fromUtf8("\345\275\223\345\211\215\351\246\226\351\200\211\357\274\232") + top.schemeTitle
            + QString::fromUtf8("\n\347\233\270\346\234\272\357\274\232") + productLabel(top.camera.manufacturer, top.camera.model)
            + QString::fromUtf8("\n\351\225\234\345\244\264\357\274\232") + productLabel(top.lens.manufacturer, top.lens.model)
            + QString::fromUtf8("\n\345\205\211\346\272\220\357\274\232") + productLabel(top.light.manufacturer, top.light.model)
            + QString::fromUtf8("\n\345\276\227\345\210\206\357\274\232") + QString::number(top.score.score, 'f', 1)
            + QString::fromUtf8("\n带宽/存储：") + QStringLiteral("%1 MB/s, %2 GB/h")
                .arg(top.bandwidthRequiredMBps, 0, 'f', 1)
                .arg(top.storagePerHourGB, 0, 'f', 0)
            + QString::fromUtf8("\nBOM：相机、镜头、光源 3 项")
            + QStringLiteral("\n");
    }
    m_reportPreview->setPlainText(text);
}

void MainWindow::importCameras()
{
    const QString path = QFileDialog::getOpenFileName(this, QString::fromUtf8("\345\257\274\345\205\245\347\233\270\346\234\272 CSV"), QString(), QStringLiteral("CSV (*.csv)"));
    if (path.isEmpty())
        return;
    QString error;
    if (!m_catalog.loadCameraCsv(path, &error)) {
        showError(error);
        return;
    }
    calculate();
}

void MainWindow::importLenses()
{
    const QString path = QFileDialog::getOpenFileName(this, QString::fromUtf8("\345\257\274\345\205\245\351\225\234\345\244\264 CSV"), QString(), QStringLiteral("CSV (*.csv)"));
    if (path.isEmpty())
        return;
    QString error;
    if (!m_catalog.loadLensCsv(path, &error)) {
        showError(error);
        return;
    }
    calculate();
}

void MainWindow::importLights()
{
    const QString path = QFileDialog::getOpenFileName(this, QString::fromUtf8("\345\257\274\345\205\245\345\205\211\346\272\220 CSV"), QString(), QStringLiteral("CSV (*.csv)"));
    if (path.isEmpty())
        return;
    QString error;
    if (!m_catalog.loadLightCsv(path, &error)) {
        showError(error);
        return;
    }
    calculate();
}

void MainWindow::exportCameras()
{
    const QString path = QFileDialog::getSaveFileName(this, QString::fromUtf8("导出相机 CSV"), QStringLiteral("cameras.csv"), QStringLiteral("CSV (*.csv)"));
    if (path.isEmpty())
        return;
    QString error;
    if (!m_catalog.exportCameraCsv(path, &error)) {
        showError(error);
        return;
    }
    QMessageBox::information(this, QString::fromUtf8("导出完成"), path);
}

void MainWindow::exportLenses()
{
    const QString path = QFileDialog::getSaveFileName(this, QString::fromUtf8("导出镜头 CSV"), QStringLiteral("lenses.csv"), QStringLiteral("CSV (*.csv)"));
    if (path.isEmpty())
        return;
    QString error;
    if (!m_catalog.exportLensCsv(path, &error)) {
        showError(error);
        return;
    }
    QMessageBox::information(this, QString::fromUtf8("导出完成"), path);
}

void MainWindow::exportLights()
{
    const QString path = QFileDialog::getSaveFileName(this, QString::fromUtf8("导出光源 CSV"), QStringLiteral("lights.csv"), QStringLiteral("CSV (*.csv)"));
    if (path.isEmpty())
        return;
    QString error;
    if (!m_catalog.exportLightCsv(path, &error)) {
        showError(error);
        return;
    }
    QMessageBox::information(this, QString::fromUtf8("导出完成"), path);
}

void MainWindow::exportFilteredCameras()
{
    const QString path = QFileDialog::getSaveFileName(this, QString::fromUtf8("导出筛选相机 CSV"), QStringLiteral("cameras_filtered.csv"), QStringLiteral("CSV (*.csv)"));
    if (path.isEmpty())
        return;
    QString error;
    if (!m_catalog.exportCameraCsv(path, visibleCameraCatalogIndexes(), &error)) {
        showError(error);
        return;
    }
    QMessageBox::information(this, QString::fromUtf8("导出完成"), path);
}

void MainWindow::exportFilteredLenses()
{
    const QString path = QFileDialog::getSaveFileName(this, QString::fromUtf8("导出筛选镜头 CSV"), QStringLiteral("lenses_filtered.csv"), QStringLiteral("CSV (*.csv)"));
    if (path.isEmpty())
        return;
    QString error;
    if (!m_catalog.exportLensCsv(path, visibleLensCatalogIndexes(), &error)) {
        showError(error);
        return;
    }
    QMessageBox::information(this, QString::fromUtf8("导出完成"), path);
}

void MainWindow::exportFilteredLights()
{
    const QString path = QFileDialog::getSaveFileName(this, QString::fromUtf8("导出筛选光源 CSV"), QStringLiteral("lights_filtered.csv"), QStringLiteral("CSV (*.csv)"));
    if (path.isEmpty())
        return;
    QString error;
    if (!m_catalog.exportLightCsv(path, visibleLightCatalogIndexes(), &error)) {
        showError(error);
        return;
    }
    QMessageBox::information(this, QString::fromUtf8("导出完成"), path);
}

void MainWindow::addCamera()
{
    CameraSpec camera;
    camera.resolutionX = 2448;
    camera.resolutionY = 2048;
    camera.pixelSizeUm = 3.45;
    camera.sensorFormat = QStringLiteral("2/3\"");
    camera.colorMode = QStringLiteral("Mono");
    camera.shutterType = QStringLiteral("Global");
    camera.maxFps = 30.0;
    camera.interfaceType = QStringLiteral("USB3");
    camera.bandwidthMBps = 380.0;
    camera.bitDepth = 12.0;
    camera.dynamicRangeDb = 60.0;
    camera.lensMount = QStringLiteral("C");
    if (!editCameraDialog(this, &camera, QString::fromUtf8("新增相机")))
        return;
    QString error;
    if (!m_catalog.addCamera(camera, &error)) {
        showError(error);
        return;
    }
    calculate();
}

void MainWindow::editCamera()
{
    const int index = selectedCameraCatalogIndex();
    if (index < 0)
        return;
    CameraSpec camera = m_catalog.cameras().at(index);
    if (!editCameraDialog(this, &camera, QString::fromUtf8("编辑相机")))
        return;
    QString error;
    if (!m_catalog.updateCamera(index, camera, &error)) {
        showError(error);
        return;
    }
    calculate();
}

void MainWindow::removeCamera()
{
    const int index = selectedCameraCatalogIndex();
    if (index < 0)
        return;
    const CameraSpec camera = m_catalog.cameras().at(index);
    if (QMessageBox::question(this, QString::fromUtf8("删除相机"),
            QString::fromUtf8("确定删除相机：%1？").arg(productLabel(camera.manufacturer, camera.model)))
        != QMessageBox::Yes) {
        return;
    }
    QString error;
    if (!m_catalog.removeCamera(index, &error)) {
        showError(error);
        return;
    }
    calculate();
}

void MainWindow::addLens()
{
    LensSpec lens;
    lens.lensType = LensType::FixedFocal;
    lens.lensMount = QStringLiteral("C");
    lens.focalLengthMm = 25.0;
    lens.minWorkingDistanceMm = 100.0;
    lens.imageCircleMm = 11.0;
    lens.megapixelRating = 5.0;
    lens.recommendedMinPixelUm = 3.45;
    lens.fNumber = 2.8;
    if (!editLensDialog(this, &lens, QString::fromUtf8("新增镜头")))
        return;
    QString error;
    if (!m_catalog.addLens(lens, &error)) {
        showError(error);
        return;
    }
    calculate();
}

void MainWindow::editLens()
{
    const int index = selectedLensCatalogIndex();
    if (index < 0)
        return;
    LensSpec lens = m_catalog.lenses().at(index);
    if (!editLensDialog(this, &lens, QString::fromUtf8("编辑镜头")))
        return;
    QString error;
    if (!m_catalog.updateLens(index, lens, &error)) {
        showError(error);
        return;
    }
    calculate();
}

void MainWindow::removeLens()
{
    const int index = selectedLensCatalogIndex();
    if (index < 0)
        return;
    const LensSpec lens = m_catalog.lenses().at(index);
    if (QMessageBox::question(this, QString::fromUtf8("删除镜头"),
            QString::fromUtf8("确定删除镜头：%1？").arg(productLabel(lens.manufacturer, lens.model)))
        != QMessageBox::Yes) {
        return;
    }
    QString error;
    if (!m_catalog.removeLens(index, &error)) {
        showError(error);
        return;
    }
    calculate();
}

void MainWindow::addLight()
{
    LightSpec light;
    light.model = QStringLiteral("NEW-LIGHT");
    light.manufacturer = QStringLiteral("VisionSelect Sample");
    light.lightType = LightType::Ring;
    light.color = QStringLiteral("White");
    light.mode = QStringLiteral("Continuous");
    light.activeWidthMm = 100.0;
    light.activeHeightMm = 100.0;
    if (!editLightDialog(this, &light, QString::fromUtf8("新增光源")))
        return;
    QString error;
    if (!m_catalog.addLight(light, &error)) {
        showError(error);
        return;
    }
    calculate();
}

void MainWindow::editLight()
{
    const int index = selectedLightCatalogIndex();
    if (index < 0)
        return;
    LightSpec light = m_catalog.lights().at(index);
    if (!editLightDialog(this, &light, QString::fromUtf8("编辑光源")))
        return;
    QString error;
    if (!m_catalog.updateLight(index, light, &error)) {
        showError(error);
        return;
    }
    calculate();
}

void MainWindow::removeLight()
{
    const int index = selectedLightCatalogIndex();
    if (index < 0)
        return;
    const LightSpec light = m_catalog.lights().at(index);
    if (QMessageBox::question(this, QString::fromUtf8("删除光源"),
            QString::fromUtf8("确定删除光源：%1？").arg(productLabel(light.manufacturer, light.model)))
        != QMessageBox::Yes) {
        return;
    }
    QString error;
    if (!m_catalog.removeLight(index, &error)) {
        showError(error);
        return;
    }
    calculate();
}

void MainWindow::resetCameras()
{
    if (QMessageBox::question(this, QString::fromUtf8("重置相机库"),
            QString::fromUtf8("确定用内置相机库覆盖本地相机维护数据？")) != QMessageBox::Yes) {
        return;
    }
    QString error;
    if (!m_catalog.resetCamerasToBuiltIn(&error)) {
        showError(error);
        return;
    }
    calculate();
}

void MainWindow::resetLenses()
{
    if (QMessageBox::question(this, QString::fromUtf8("重置镜头库"),
            QString::fromUtf8("确定用内置镜头库覆盖本地镜头维护数据？")) != QMessageBox::Yes) {
        return;
    }
    QString error;
    if (!m_catalog.resetLensesToBuiltIn(&error)) {
        showError(error);
        return;
    }
    calculate();
}

void MainWindow::resetLights()
{
    if (QMessageBox::question(this, QString::fromUtf8("重置光源库"),
            QString::fromUtf8("确定用内置光源库覆盖本地光源维护数据？")) != QMessageBox::Yes) {
        return;
    }
    QString error;
    if (!m_catalog.resetLightsToBuiltIn(&error)) {
        showError(error);
        return;
    }
    calculate();
}

void MainWindow::clearCameraFilters()
{
    if (m_cameraSearchEdit)
        m_cameraSearchEdit->clear();
    if (m_cameraManufacturerFilter)
        m_cameraManufacturerFilter->setCurrentIndex(0);
    if (m_cameraInterfaceFilter)
        m_cameraInterfaceFilter->setCurrentIndex(0);
    refreshCameraTable();
}

void MainWindow::clearLensFilters()
{
    if (m_lensSearchEdit)
        m_lensSearchEdit->clear();
    if (m_lensManufacturerFilter)
        m_lensManufacturerFilter->setCurrentIndex(0);
    if (m_lensTypeFilter)
        m_lensTypeFilter->setCurrentIndex(0);
    if (m_lensMountFilter)
        m_lensMountFilter->setCurrentIndex(0);
    refreshLensTable();
}

void MainWindow::clearLightFilters()
{
    if (m_lightSearchEdit)
        m_lightSearchEdit->clear();
    if (m_lightManufacturerFilter)
        m_lightManufacturerFilter->setCurrentIndex(0);
    if (m_lightTypeFilter)
        m_lightTypeFilter->setCurrentIndex(0);
    if (m_lightModeFilter)
        m_lightModeFilter->setCurrentIndex(0);
    refreshLightTable();
}

int MainWindow::selectedCameraCatalogIndex() const
{
    if (!m_cameraTable)
        return -1;
    return rowSourceIndex(m_cameraTable, m_cameraTable->currentRow());
}

int MainWindow::selectedLensCatalogIndex() const
{
    if (!m_lensTable)
        return -1;
    return rowSourceIndex(m_lensTable, m_lensTable->currentRow());
}

int MainWindow::selectedLightCatalogIndex() const
{
    if (!m_lightTable)
        return -1;
    return rowSourceIndex(m_lightTable, m_lightTable->currentRow());
}

QVector<int> MainWindow::visibleCameraCatalogIndexes() const
{
    QVector<int> indexes;
    if (!m_cameraTable)
        return indexes;
    for (int row = 0; row < m_cameraTable->rowCount(); ++row) {
        const int index = rowSourceIndex(m_cameraTable, row);
        if (index >= 0)
            indexes.append(index);
    }
    return indexes;
}

QVector<int> MainWindow::visibleLensCatalogIndexes() const
{
    QVector<int> indexes;
    if (!m_lensTable)
        return indexes;
    for (int row = 0; row < m_lensTable->rowCount(); ++row) {
        const int index = rowSourceIndex(m_lensTable, row);
        if (index >= 0)
            indexes.append(index);
    }
    return indexes;
}

QVector<int> MainWindow::visibleLightCatalogIndexes() const
{
    QVector<int> indexes;
    if (!m_lightTable)
        return indexes;
    for (int row = 0; row < m_lightTable->rowCount(); ++row) {
        const int index = rowSourceIndex(m_lightTable, row);
        if (index >= 0)
            indexes.append(index);
    }
    return indexes;
}

void MainWindow::exportBomCsv()
{
    if (m_results.isEmpty())
        calculate();
    if (m_results.isEmpty()) {
        showError(QString::fromUtf8("暂无推荐方案，无法导出 BOM。"));
        return;
    }

    const QString defaultName = QStringLiteral("VisionSelect_BOM_%1.csv")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
    const QString path = QFileDialog::getSaveFileName(this, QString::fromUtf8("导出 BOM CSV"), defaultName, QStringLiteral("CSV (*.csv)"));
    if (path.isEmpty())
        return;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        showError(QString::fromUtf8("无法写入 BOM CSV：%1").arg(path));
        return;
    }

    QTextStream out(&file);
    out.setCodec("UTF-8");
    out << "scheme,rank,category,manufacturer,model,key_specs,notes\n";
    const int count = qMin(5, m_results.size());
    for (int i = 0; i < count; ++i) {
        const SelectionResult &r = m_results.at(i);
        const QString scheme = QStringLiteral("#%1 %2").arg(i + 1).arg(r.schemeTitle);
        out << csvCell(scheme) << "," << (i + 1) << ","
            << csvCell(QString::fromUtf8("相机")) << ","
            << csvCell(r.camera.manufacturer) << ","
            << csvCell(r.camera.model) << ","
            << csvCell(bomSpecForCamera(r.camera, r)) << ","
            << csvCell(QStringLiteral("FOV %1 x %2 mm; object pixel %3 um")
                .arg(r.effectiveFovWidthMm, 0, 'f', 2)
                .arg(r.effectiveFovHeightMm, 0, 'f', 2)
                .arg(r.objectPixelSizeUm, 0, 'f', 2))
            << "\n";
        out << csvCell(scheme) << "," << (i + 1) << ","
            << csvCell(QString::fromUtf8("镜头")) << ","
            << csvCell(r.lens.manufacturer) << ","
            << csvCell(r.lens.model) << ","
            << csvCell(bomSpecForLens(r.lens, r)) << ","
            << csvCell(QStringLiteral("distortion %1 um; lens MP utilization %2%")
                .arg(r.distortionErrorUm, 0, 'f', 2)
                .arg(r.lensMegapixelUtilizationPercent, 0, 'f', 0))
            << "\n";
        out << csvCell(scheme) << "," << (i + 1) << ","
            << csvCell(QString::fromUtf8("光源")) << ","
            << csvCell(r.light.manufacturer) << ","
            << csvCell(r.light.model) << ","
            << csvCell(bomSpecForLight(r.light, r)) << ","
            << csvCell(riskSummary(r))
            << "\n";
    }
    file.close();
    QMessageBox::information(this, QString::fromUtf8("导出完成"), path);
}

void MainWindow::exportPdf()
{
    if (m_results.isEmpty())
        calculate();

    const QString defaultName = QStringLiteral("VisionSelect_%1.pdf")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
    const QString path = QFileDialog::getSaveFileName(this, QString::fromUtf8("\345\257\274\345\207\272 PDF \346\212\245\345\221\212"), defaultName, QStringLiteral("PDF (*.pdf)"));
    if (path.isEmpty())
        return;

    PdfReportWriter writer;
    QString error;
    if (!writer.write(path, m_request, m_results, &error)) {
        showError(error);
        return;
    }
    QMessageBox::information(this, QString::fromUtf8("\345\257\274\345\207\272\345\256\214\346\210\220"), QString::fromUtf8("PDF \346\212\245\345\221\212\345\267\262\347\224\237\346\210\220\357\274\232\n%1").arg(path));
}

void MainWindow::showError(const QString &message)
{
    QMessageBox::warning(this, QString::fromUtf8("\346\223\215\344\275\234\345\244\261\350\264\245"), message);
}
