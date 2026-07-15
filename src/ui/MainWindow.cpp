#include "ui/MainWindow.h"

#include "i18n/LanguageManager.h"
#include "license/LicenseManager.h"
#include "report/PdfReportWriter.h"
#include "selection/CalculationAssistant.h"
#include "selection/SelectionService.h"
#include "ui/CatalogDialogs.h"
#include "ui/UiHelpers.h"
#include "ui/pages/CalculationPage.h"
#include "ui/pages/CatalogPage.h"
#include "ui/pages/InputPage.h"
#include "ui/pages/PureCalculationPage.h"
#include "ui/pages/ResultsPage.h"
#include "ui/pages/ThreeDCameraPage.h"

#include <QDateTime>
#include <QAbstractItemView>
#include <QAbstractSpinBox>
#include <QCheckBox>
#include <QComboBox>
#include <QDialog>
#include <QDialogButtonBox>
#include <QFile>
#include <QFileDialog>
#include <QFrame>
#include <QGridLayout>
#include <QHBoxLayout>
#include <QIcon>
#include <QLabel>
#include <QLineEdit>
#include <QMessageBox>
#include <QMouseEvent>
#include <QPainterPath>
#include <QPushButton>
#include <QResizeEvent>
#include <QRegion>
#include <QSize>
#include <QSizePolicy>
#include <QStackedWidget>
#include <QSpinBox>
#include <QStyle>
#include <QTabWidget>
#include <QTextEdit>
#include <QTextStream>
#include <QToolButton>
#include <QVBoxLayout>
#include <QWidget>
#include <QtConcurrent/QtConcurrent>

#ifdef Q_OS_WIN
#include <windows.h>
#include <windowsx.h>
#endif

using namespace UiHelpers;

namespace {
const int kPureCalculationPageIndex = 1;
const int kCalculationPageIndex = 2;
const int kThreeDCameraPageIndex = 3;
const int kResultsPageIndex = 4;
const int kCatalogPageIndex = 5;

struct ComboState
{
    int index = -1;
    QString text;
};

struct ViewState
{
    int row = -1;
    int column = -1;
};

struct PageUiState
{
    QVector<double> doubleSpinValues;
    QVector<int> spinValues;
    QVector<ComboState> comboValues;
    QVector<bool> checkValues;
    QVector<QString> lineEditValues;
    QVector<QString> textEditValues;
    QVector<int> tabIndexes;
    QVector<ViewState> viewIndexes;
};

PageUiState capturePageUiState(QWidget *page)
{
    PageUiState state;
    if (!page)
        return state;

    for (QDoubleSpinBox *spin : page->findChildren<QDoubleSpinBox *>())
        state.doubleSpinValues.append(spin->value());
    for (QSpinBox *spin : page->findChildren<QSpinBox *>())
        state.spinValues.append(spin->value());
    for (QComboBox *combo : page->findChildren<QComboBox *>())
        state.comboValues.append({combo->currentIndex(), combo->currentText()});
    for (QCheckBox *check : page->findChildren<QCheckBox *>())
        state.checkValues.append(check->isChecked());
    for (QLineEdit *edit : page->findChildren<QLineEdit *>()) {
        if (edit->isReadOnly()
            || qobject_cast<QAbstractSpinBox *>(edit->parentWidget())
            || qobject_cast<QComboBox *>(edit->parentWidget()))
            continue;
        state.lineEditValues.append(edit->text());
    }
    for (QTextEdit *edit : page->findChildren<QTextEdit *>()) {
        if (!edit->isReadOnly())
            state.textEditValues.append(edit->toPlainText());
    }
    for (QTabWidget *tabs : page->findChildren<QTabWidget *>())
        state.tabIndexes.append(tabs->currentIndex());
    for (QAbstractItemView *view : page->findChildren<QAbstractItemView *>()) {
        const QModelIndex index = view->currentIndex();
        state.viewIndexes.append({index.row(), index.column()});
    }
    return state;
}

void restorePageUiState(QWidget *page, const PageUiState &state)
{
    if (!page)
        return;

    const QList<QDoubleSpinBox *> doubleSpins = page->findChildren<QDoubleSpinBox *>();
    for (int i = 0; i < doubleSpins.size() && i < state.doubleSpinValues.size(); ++i)
        doubleSpins.at(i)->setValue(state.doubleSpinValues.at(i));
    const QList<QSpinBox *> spins = page->findChildren<QSpinBox *>();
    for (int i = 0; i < spins.size() && i < state.spinValues.size(); ++i)
        spins.at(i)->setValue(state.spinValues.at(i));
    const QList<QComboBox *> combos = page->findChildren<QComboBox *>();
    for (int i = 0; i < combos.size() && i < state.comboValues.size(); ++i) {
        QComboBox *combo = combos.at(i);
        const ComboState &value = state.comboValues.at(i);
        if (value.index >= 0 && value.index < combo->count())
            combo->setCurrentIndex(value.index);
        if (combo->isEditable())
            combo->setEditText(value.text);
    }
    const QList<QCheckBox *> checks = page->findChildren<QCheckBox *>();
    for (int i = 0; i < checks.size() && i < state.checkValues.size(); ++i)
        checks.at(i)->setChecked(state.checkValues.at(i));

    int lineEditIndex = 0;
    for (QLineEdit *edit : page->findChildren<QLineEdit *>()) {
        if (edit->isReadOnly()
            || qobject_cast<QAbstractSpinBox *>(edit->parentWidget())
            || qobject_cast<QComboBox *>(edit->parentWidget()))
            continue;
        if (lineEditIndex < state.lineEditValues.size())
            edit->setText(state.lineEditValues.at(lineEditIndex));
        ++lineEditIndex;
    }
    int textEditIndex = 0;
    for (QTextEdit *edit : page->findChildren<QTextEdit *>()) {
        if (edit->isReadOnly())
            continue;
        if (textEditIndex < state.textEditValues.size())
            edit->setPlainText(state.textEditValues.at(textEditIndex));
        ++textEditIndex;
    }
    const QList<QTabWidget *> tabs = page->findChildren<QTabWidget *>();
    for (int i = 0; i < tabs.size() && i < state.tabIndexes.size(); ++i)
        tabs.at(i)->setCurrentIndex(state.tabIndexes.at(i));
    const QList<QAbstractItemView *> views = page->findChildren<QAbstractItemView *>();
    for (int i = 0; i < views.size() && i < state.viewIndexes.size(); ++i) {
        const ViewState &value = state.viewIndexes.at(i);
        if (value.row < 0 || !views.at(i)->model())
            continue;
        const QModelIndex index = views.at(i)->model()->index(value.row, qMax(0, value.column));
        if (index.isValid())
            views.at(i)->setCurrentIndex(index);
    }
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

void replaceStackPage(QStackedWidget *pages, int index, QWidget *page)
{
    if (!pages || !page || index < 0 || index >= pages->count())
        return;

    QWidget *placeholder = pages->widget(index);
    pages->removeWidget(placeholder);
    if (placeholder)
        placeholder->deleteLater();
    pages->insertWidget(index, page);
}

SelectionJobResult runSelectionJob(const QString &storageDirectory, const SelectionRequest &request,
                                  int limit, const QString &languageCode)
{
    SelectionJobResult result;
    result.request = request;

    CatalogRepository workerCatalog;
    workerCatalog.setStorageDirectory(storageDirectory);
    if (!workerCatalog.initializeDatabase(&result.error))
        return result;

    SelectionService service(&workerCatalog);
    result.results = service.select(request, limit, &result.error, languageCode);
    return result;
}

class WindowChromeBar : public QFrame
{
public:
    explicit WindowChromeBar(QWidget *parent = nullptr)
        : QFrame(parent)
    {
        setMouseTracking(true);
    }

protected:
    void mousePressEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton && window()) {
            m_dragOffset = event->globalPos() - window()->frameGeometry().topLeft();
            m_dragging = !window()->isMaximized();
            event->accept();
            return;
        }
        QFrame::mousePressEvent(event);
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        if (m_dragging && window() && !window()->isMaximized() && (event->buttons() & Qt::LeftButton)) {
            window()->move(event->globalPos() - m_dragOffset);
            event->accept();
            return;
        }
        QFrame::mouseMoveEvent(event);
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        m_dragging = false;
        QFrame::mouseReleaseEvent(event);
    }

    void mouseDoubleClickEvent(QMouseEvent *event) override
    {
        if (event->button() == Qt::LeftButton && window()) {
            if (window()->isMaximized())
                window()->showNormal();
            else
                window()->showMaximized();
            event->accept();
            return;
        }
        QFrame::mouseDoubleClickEvent(event);
    }

private:
    QPoint m_dragOffset;
    bool m_dragging = false;
};
}

MainWindow::MainWindow(QWidget *parent)
    : QMainWindow(parent)
{
    setWindowFlags(Qt::Window | Qt::FramelessWindowHint);
    setAttribute(Qt::WA_TranslucentBackground);

    QString error;
    if (!m_catalog.initializeDatabase(&error))
        QMessageBox::critical(this, tr("Catalog Load Failed"), error);

    m_selectionWatcher = new QFutureWatcher<SelectionJobResult>(this);
    connect(m_selectionWatcher, &QFutureWatcher<SelectionJobResult>::finished,
            this, &MainWindow::finishSelectionCalculation);

    buildUi();
    connect(&LanguageManager::instance(), &LanguageManager::languageChanged, this, &MainWindow::rebuildPagesForLanguage);
}

void MainWindow::resizeEvent(QResizeEvent *event)
{
    QMainWindow::resizeEvent(event);
    updateWindowMask();
}

bool MainWindow::nativeEvent(const QByteArray &eventType, void *message, long *result)
{
#ifdef Q_OS_WIN
    Q_UNUSED(eventType)
    MSG *nativeMessage = static_cast<MSG *>(message);
    if (nativeMessage && nativeMessage->message == WM_NCHITTEST
        && !isMaximized() && !isFullScreen()) {
        RECT windowRect;
        if (GetWindowRect(reinterpret_cast<HWND>(winId()), &windowRect)) {
            const LONG x = GET_X_LPARAM(nativeMessage->lParam);
            const LONG y = GET_Y_LPARAM(nativeMessage->lParam);
            const int border = qMax(6, qRound(8.0 * devicePixelRatioF()));
            const bool left = x >= windowRect.left && x < windowRect.left + border;
            const bool right = x <= windowRect.right && x > windowRect.right - border;
            const bool top = y >= windowRect.top && y < windowRect.top + border;
            const bool bottom = y <= windowRect.bottom && y > windowRect.bottom - border;

            if (top && left)
                *result = HTTOPLEFT;
            else if (top && right)
                *result = HTTOPRIGHT;
            else if (bottom && left)
                *result = HTBOTTOMLEFT;
            else if (bottom && right)
                *result = HTBOTTOMRIGHT;
            else if (left)
                *result = HTLEFT;
            else if (right)
                *result = HTRIGHT;
            else if (top)
                *result = HTTOP;
            else if (bottom)
                *result = HTBOTTOM;
            else
                return QMainWindow::nativeEvent(eventType, message, result);
            return true;
        }
    }
#endif
    return QMainWindow::nativeEvent(eventType, message, result);
}

void MainWindow::updateWindowMask()
{
    if (isMaximized() || isFullScreen()) {
        clearMask();
        return;
    }

    QPainterPath roundedRect;
    roundedRect.addRoundedRect(rect(), 12.0, 12.0);
    setMask(QRegion(roundedRect.toFillPolygon().toPolygon()));
}

void MainWindow::buildUi()
{
    QWidget *root = new QWidget(this);
    root->setObjectName(QStringLiteral("ApplicationShell"));
    QVBoxLayout *shellLayout = new QVBoxLayout(root);
    shellLayout->setContentsMargins(0, 0, 0, 0);
    shellLayout->setSpacing(0);
    shellLayout->addWidget(createTopBar());

    QWidget *workspace = new QWidget(root);
    workspace->setObjectName(QStringLiteral("ShellWorkspace"));
    QHBoxLayout *rootLayout = new QHBoxLayout(workspace);
    rootLayout->setContentsMargins(0, 0, 0, 0);
    rootLayout->setSpacing(0);

    rootLayout->addWidget(createSidebar());

    m_pages = new QStackedWidget(root);
    m_inputPage = new InputPage;
    connect(m_inputPage, &InputPage::calculateRequested, this, &MainWindow::calculate);
    connect(m_inputPage, &InputPage::resultsRequested, this, [this]() {
        calculate();
        setActivePage(kResultsPageIndex);
    });
    m_pages->addWidget(m_inputPage);
    for (int i = 1; i <= kCatalogPageIndex; ++i)
        m_pages->addWidget(new QWidget);
    rootLayout->addWidget(m_pages, 1);
    shellLayout->addWidget(workspace, 1);
    shellLayout->addWidget(createStatusBar());

    setCentralWidget(root);
    retranslateUi();
    setActivePage(0);
}

QWidget *MainWindow::createTopBar()
{
    WindowChromeBar *bar = new WindowChromeBar;
    bar->setObjectName(QStringLiteral("ShellTopBar"));
    bar->setFixedHeight(54);

    QHBoxLayout *layout = new QHBoxLayout(bar);
    layout->setContentsMargins(16, 0, 16, 0);
    layout->setSpacing(12);

    QLabel *appIcon = new QLabel(bar);
    appIcon->setObjectName(QStringLiteral("TopBarIcon"));
    appIcon->setAttribute(Qt::WA_TransparentForMouseEvents);
    appIcon->setPixmap(QIcon(QStringLiteral(":/icons/visionselect_icon_64.png")).pixmap(26, 26));
    layout->addWidget(appIcon, 0, Qt::AlignVCenter);

    QLabel *brand = new QLabel(QStringLiteral("VisionSelect"), bar);
    brand->setObjectName(QStringLiteral("TopBarBrand"));
    brand->setAttribute(Qt::WA_TransparentForMouseEvents);
    layout->addWidget(brand, 0, Qt::AlignVCenter);

    m_topProductLabel = new QLabel(bar);
    m_topProductLabel->setObjectName(QStringLiteral("TopBarProduct"));
    m_topProductLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    layout->addWidget(m_topProductLabel, 0, Qt::AlignVCenter);

    m_topPageLabel = new QLabel(bar);
    m_topPageLabel->setObjectName(QStringLiteral("TopBarPage"));
    m_topPageLabel->setAttribute(Qt::WA_TransparentForMouseEvents);
    layout->addWidget(m_topPageLabel, 0, Qt::AlignVCenter);
    layout->addStretch(1);

    m_workflowStepLabels.clear();
    const QStringList initialSteps = {
        localizedText("1  需求建模", "1  Requirements"),
        localizedText("2  候选计算", "2  Candidate Calculation"),
        localizedText("3  方案评审", "3  Solution Review")
    };
    for (const QString &step : initialSteps) {
        QLabel *label = new QLabel(step, bar);
        label->setObjectName(QStringLiteral("WorkflowStep"));
        label->setProperty("state", QStringLiteral("pending"));
        label->setAlignment(Qt::AlignCenter);
        label->setAttribute(Qt::WA_TransparentForMouseEvents);
        m_workflowStepLabels.append(label);
        layout->addWidget(label, 0, Qt::AlignVCenter);
    }

    QToolButton *minimizeButton = new QToolButton(bar);
    minimizeButton->setObjectName(QStringLiteral("WindowControlButton"));
    minimizeButton->setText(QStringLiteral("—"));
    minimizeButton->setToolTip(localizedText("最小化", "Minimize"));
    connect(minimizeButton, &QToolButton::clicked, this, &MainWindow::showMinimized);
    layout->addWidget(minimizeButton, 0, Qt::AlignVCenter);

    QToolButton *maximizeButton = new QToolButton(bar);
    maximizeButton->setObjectName(QStringLiteral("WindowControlButton"));
    maximizeButton->setText(QStringLiteral("□"));
    maximizeButton->setToolTip(localizedText("最大化", "Maximize"));
    connect(maximizeButton, &QToolButton::clicked, this, [this, maximizeButton]() {
        if (isMaximized()) {
            showNormal();
            maximizeButton->setText(QStringLiteral("□"));
            maximizeButton->setToolTip(localizedText("最大化", "Maximize"));
        } else {
            showMaximized();
            maximizeButton->setText(QStringLiteral("❐"));
            maximizeButton->setToolTip(localizedText("还原", "Restore"));
        }
    });
    layout->addWidget(maximizeButton, 0, Qt::AlignVCenter);

    QToolButton *closeButton = new QToolButton(bar);
    closeButton->setObjectName(QStringLiteral("WindowCloseButton"));
    closeButton->setText(QStringLiteral("×"));
    closeButton->setToolTip(localizedText("关闭", "Close"));
    connect(closeButton, &QToolButton::clicked, this, &MainWindow::close);
    layout->addWidget(closeButton, 0, Qt::AlignVCenter);
    return bar;
}

QWidget *MainWindow::createSidebar()
{
    QFrame *sidebar = new QFrame;
    sidebar->setObjectName(QStringLiteral("Sidebar"));
    sidebar->setFixedWidth(104);

    QVBoxLayout *layout = new QVBoxLayout(sidebar);
    layout->setContentsMargins(8, 12, 8, 10);
    layout->setSpacing(6);

    QLabel *railMark = new QLabel(sidebar);
    railMark->setObjectName(QStringLiteral("RailMark"));
    railMark->setPixmap(QIcon(QStringLiteral(":/icons/visionselect_icon_64.png")).pixmap(34, 34));
    railMark->setAlignment(Qt::AlignCenter);
    railMark->setFixedHeight(44);
    layout->addWidget(railMark);

    m_languageCombo = new QComboBox(sidebar);
    m_languageCombo->setObjectName(QStringLiteral("SidebarLanguage"));
    for (const QString &language : LanguageManager::instance().availableLanguages())
        m_languageCombo->addItem(LanguageManager::instance().displayName(language), language);
    connect(m_languageCombo, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this](int index) {
        const QString language = m_languageCombo->itemData(index).toString();
        if (!language.isEmpty() && language != LanguageManager::instance().currentLanguage())
            LanguageManager::instance().setLanguage(language);
    });

    m_navButtons.clear();
    m_navButtons.resize(kCatalogPageIndex + 1);
    m_navSectionLabels.clear();
    const auto addSection = [this, layout](const QString &text) {
        QLabel *label = new QLabel(text);
        label->setObjectName(QStringLiteral("SidebarSectionLabel"));
        m_navSectionLabels.append(label);
        layout->addWidget(label);
    };
    const auto addNav = [this, layout](int pageIndex, const QString &text, const QString &iconPath) {
        QPushButton *button = new QPushButton(text);
        button->setObjectName(QStringLiteral("NavButton"));
        button->setCursor(Qt::PointingHandCursor);
        button->setFixedHeight(44);
        button->setIcon(QIcon(iconPath));
        button->setIconSize(QSize(20, 20));
        button->setFocusPolicy(Qt::NoFocus);
        button->setToolTip(navigationLabels().at(pageIndex));
        connect(button, &QPushButton::clicked, this, [this, pageIndex]() { setActivePage(pageIndex); });
        m_navButtons[pageIndex] = button;
        layout->addWidget(button);
    };

    const QStringList railLabels = railNavigationLabels();
    addSection(localizedText("选型", "SELECT"));
    addNav(0, railLabels.at(0), QStringLiteral(":/icons/ui/requirement.png"));
    addNav(kCalculationPageIndex, railLabels.at(kCalculationPageIndex), QStringLiteral(":/icons/ui/assistant.png"));
    addNav(kResultsPageIndex, railLabels.at(kResultsPageIndex), QStringLiteral(":/icons/ui/results.png"));

    addSection(localizedText("工具", "TOOLS"));
    addNav(kPureCalculationPageIndex, railLabels.at(kPureCalculationPageIndex), QStringLiteral(":/icons/ui/calculate.png"));
    addNav(kThreeDCameraPageIndex, railLabels.at(kThreeDCameraPageIndex), QStringLiteral(":/icons/ui/camera3d.png"));

    addSection(localizedText("数据", "DATA"));
    addNav(kCatalogPageIndex, railLabels.at(kCatalogPageIndex), QStringLiteral(":/icons/ui/catalog.png"));

    layout->addStretch();
    m_licenseButton = new QPushButton(sidebar);
    m_licenseButton->setObjectName(QStringLiteral("SidebarLicenseButton"));
    m_licenseButton->setIcon(QIcon(QStringLiteral(":/icons/ui/info.png")));
    m_licenseButton->setIconSize(QSize(18, 18));
    m_licenseButton->setCursor(Qt::PointingHandCursor);
    connect(m_licenseButton, &QPushButton::clicked, this, &MainWindow::showLicenseInfo);
    layout->addWidget(m_licenseButton);

    layout->addWidget(m_languageCombo);

    return sidebar;
}

QWidget *MainWindow::createStatusBar()
{
    QFrame *bar = new QFrame;
    bar->setObjectName(QStringLiteral("ShellStatusBar"));
    bar->setFixedHeight(38);
    QHBoxLayout *layout = new QHBoxLayout(bar);
    layout->setContentsMargins(12, 0, 14, 0);
    layout->setSpacing(10);

    m_summaryStatusLabel = new QLabel(bar);
    m_summaryStatusLabel->setObjectName(QStringLiteral("ShellStatusPill"));
    layout->addWidget(m_summaryStatusLabel);
    m_summaryLabel = new QLabel(bar);
    m_summaryLabel->setObjectName(QStringLiteral("ShellStatusText"));
    layout->addWidget(m_summaryLabel);
    layout->addStretch(1);

    const auto addStat = [layout, bar](QLabel **label) {
        *label = new QLabel(bar);
        (*label)->setObjectName(QStringLiteral("ShellCatalogStat"));
        layout->addWidget(*label);
    };
    addStat(&m_cameraCountLabel);
    addStat(&m_lensCountLabel);
    addStat(&m_lightCountLabel);

    QLabel *units = new QLabel(localizedText("单位：mm · um · fps", "Units: mm · um · fps"), bar);
    units->setObjectName(QStringLiteral("ShellUnits"));
    layout->addWidget(units);
    return bar;
}

QStringList MainWindow::navigationLabels() const
{
    return {
        localizedText("需求输入", "Requirement Input"),
        localizedText("视觉参数校算", "Vision Parameter Check"),
        localizedText("产品计算", "Calculation Assistant"),
        localizedText("3D 相机", "3D Camera"),
        localizedText("推荐结果", "Recommended Results"),
        localizedText("参数库", "Catalog")
    };
}

QStringList MainWindow::railNavigationLabels() const
{
    return {
        localizedText("2D 选型", "2D Select"),
        localizedText("参数校算", "Validate"),
        localizedText("候选计算", "Candidates"),
        localizedText("3D 相机", "3D Camera"),
        localizedText("方案评审", "Review"),
        localizedText("参数库", "Catalog")
    };
}

void MainWindow::syncLanguageCombo()
{
    if (!m_languageCombo)
        return;
    const QString language = LanguageManager::instance().currentLanguage();
    for (int i = 0; i < m_languageCombo->count(); ++i) {
        if (m_languageCombo->itemData(i).toString() == language) {
            m_languageCombo->blockSignals(true);
            m_languageCombo->setCurrentIndex(i);
            m_languageCombo->blockSignals(false);
            return;
        }
    }
}

void MainWindow::refreshSidebarSummary()
{
    if (selectionCalculationRunning()) {
        if (m_summaryStatusLabel) {
            m_summaryStatusLabel->setText(localizedText("计算中", "Calculating"));
            m_summaryStatusLabel->setProperty("state", QStringLiteral("busy"));
        }
        if (m_summaryLabel)
            m_summaryLabel->setText(localizedText("正在检索候选并生成推荐方案。",
                                                 "Fetching candidates and scoring recommendations."));
    } else {
        if (m_summaryStatusLabel) {
            m_summaryStatusLabel->setText(localizedText("就绪", "Ready"));
            m_summaryStatusLabel->setProperty("state", QStringLiteral("ready"));
        }
        if (m_summaryLabel)
            m_summaryLabel->setText(localizedText("推荐和导出可用。",
                                                 "Ready for recommendations and export."));
    }
    if (m_summaryStatusLabel) {
        m_summaryStatusLabel->style()->unpolish(m_summaryStatusLabel);
        m_summaryStatusLabel->style()->polish(m_summaryStatusLabel);
    }

    const auto statText = [](const QString &label, int value) {
        return QStringLiteral("%1  %2").arg(label, QString::number(value));
    };
    QString countError;
    const int cameraCount = m_catalog.productCount(CatalogDomain::Camera, &countError);
    countError.clear();
    const int lensCount = m_catalog.productCount(CatalogDomain::Lens, &countError);
    countError.clear();
    const int lightCount = m_catalog.productCount(CatalogDomain::Light, &countError);
    if (m_cameraCountLabel)
        m_cameraCountLabel->setText(statText(localizedText("相机", "Cameras"), cameraCount >= 0 ? cameraCount : m_catalog.cameras().size()));
    if (m_lensCountLabel)
        m_lensCountLabel->setText(statText(localizedText("镜头", "Lenses"), lensCount >= 0 ? lensCount : m_catalog.lenses().size()));
    if (m_lightCountLabel)
        m_lightCountLabel->setText(statText(localizedText("光源", "Lights"), lightCount >= 0 ? lightCount : m_catalog.lights().size()));
    if (m_languageLabel)
        m_languageLabel->setText(localizedText("界面语言", "Language"));
}

void MainWindow::retranslateUi()
{
    setWindowTitle(tr("VisionSelect - Industrial Machine Vision Selection Assistant"));
    if (m_topProductLabel)
        m_topProductLabel->setText(localizedText("工程控制台", "Engineering Control Console"));
    if (m_brandSubtitleLabel)
        m_brandSubtitleLabel->setText(tr("Industrial Machine Vision Selection Assistant"));
    if (m_brandBadgeLabel)
        m_brandBadgeLabel->setText(localizedText("需求 · 计算 · 选型", "Selection Workflow"));
    if (m_navTitleLabel)
        m_navTitleLabel->setText(localizedText("工作台", "Workbench"));
    const QStringList sections = {
        localizedText("工作流", "Workflow"),
        localizedText("工程工具", "Engineering Tools"),
        localizedText("数据与系统", "Data and System")
    };
    for (int i = 0; i < m_navSectionLabels.size() && i < sections.size(); ++i)
        m_navSectionLabels.at(i)->setText(sections.at(i));
    const QStringList labels = railNavigationLabels();
    for (int i = 0; i < m_navButtons.size() && i < labels.size(); ++i)
        if (m_navButtons.at(i)) {
            m_navButtons.at(i)->setText(labels.at(i));
            m_navButtons.at(i)->setToolTip(navigationLabels().at(i));
        }
    const QStringList steps = {
        localizedText("1  需求建模", "1  Requirements"),
        localizedText("2  候选计算", "2  Candidate Calculation"),
        localizedText("3  方案评审", "3  Solution Review")
    };
    for (int i = 0; i < m_workflowStepLabels.size() && i < steps.size(); ++i)
        m_workflowStepLabels.at(i)->setText(steps.at(i));
    refreshSidebarSummary();
    if (m_licenseButton)
        m_licenseButton->setText(localizedText("授权", "License"));
    syncLanguageCombo();
}

void MainWindow::rebuildPagesForLanguage()
{
    if (!m_pages)
        return;

    const int currentIndex = m_pages->currentIndex();
    const SelectionRequest savedRequest = m_inputPage ? m_inputPage->request() : m_request;
    const PageUiState pureCalculationState = capturePageUiState(m_pureCalculationPage);
    const PageUiState calculationState = capturePageUiState(m_calculationPage);
    const PageUiState threeDState = capturePageUiState(m_threeDCameraPage);
    const PageUiState resultsState = capturePageUiState(m_resultsPage);
    const PageUiState catalogState = capturePageUiState(m_catalogPage);
    const bool hadPureCalculationPage = m_pureCalculationPage != nullptr;
    const bool hadCalculationPage = m_calculationPage != nullptr;
    const bool hadThreeDPage = m_threeDCameraPage != nullptr;
    const bool hadResultsPage = m_resultsPage != nullptr;
    const bool hadCatalogPage = m_catalogPage != nullptr;
    const bool hadResults = !m_results.isEmpty();
    const bool selectionWasRunning = selectionCalculationRunning();

    m_inputPage = new InputPage;
    m_inputPage->setRequest(savedRequest);
    connect(m_inputPage, &InputPage::calculateRequested, this, &MainWindow::calculate);
    connect(m_inputPage, &InputPage::resultsRequested, this, [this]() {
        calculate();
        setActivePage(kResultsPageIndex);
    });
    replaceStackPage(m_pages, 0, m_inputPage);

    m_pureCalculationPage = nullptr;
    m_calculationPage = nullptr;
    m_threeDCameraPage = nullptr;
    m_resultsPage = nullptr;
    m_catalogPage = nullptr;
    m_catalogPageInitialized = false;
    m_request = savedRequest;

    if (hadPureCalculationPage)
        ensurePureCalculationPage();
    if (hadCalculationPage) {
        ensureCalculationPage();
        refreshCalculationAssistant();
    }
    if (hadThreeDPage) {
        ensureThreeDCameraPage();
        if (m_threeDCameraPage)
            m_threeDCameraPage->activate();
    }
    if (hadResultsPage)
        ensureResultsPage();
    if (hadCatalogPage)
        ensureCatalogPageInitialized();

    retranslateUi();
    setActivePage(currentIndex);
    restorePageUiState(m_pureCalculationPage, pureCalculationState);
    restorePageUiState(m_calculationPage, calculationState);
    restorePageUiState(m_threeDCameraPage, threeDState);
    restorePageUiState(m_resultsPage, resultsState);
    restorePageUiState(m_catalogPage, catalogState);

    if (hadResults || selectionWasRunning)
        startSelectionCalculation(m_request);
}

void MainWindow::showLicenseInfo()
{
    LicenseManager manager;
    const LicenseStatus status = manager.currentStatus();
    if (!status.isValid()) {
        QMessageBox::warning(this, tr("License Info"), status.message);
        return;
    }

    const LicenseInfo &info = status.info;
    QMessageBox::information(this, tr("License Info"),
        tr("Licensee: %1\nSerial: %2\nMachine code: %3\nExpires: %4")
            .arg(info.licensee,
                 info.serial,
                 info.machineCode,
                 info.expiresAt.toString(Qt::ISODate)));
}

void MainWindow::setActivePage(int index)
{
    if (!m_pages || index < 0 || index >= m_pages->count())
        return;
    if (index == kPureCalculationPageIndex) {
        ensurePureCalculationPage();
        if (m_pureCalculationPage)
            m_pureCalculationPage->refresh();
    }
    if (index == kCalculationPageIndex) {
        ensureCalculationPage();
        m_request = m_inputPage->request();
        refreshCalculationAssistant();
    }
    if (index == kThreeDCameraPageIndex) {
        ensureThreeDCameraPage();
        if (m_threeDCameraPage)
            m_threeDCameraPage->activate();
    }
    if (index == kResultsPageIndex)
        ensureResultsPage();
    if (index == kCatalogPageIndex)
        ensureCatalogPageInitialized();
    if (index == kResultsPageIndex && m_results.isEmpty() && !selectionCalculationRunning()) {
        calculate();
    } else if (index == kResultsPageIndex && selectionCalculationRunning() && m_resultsPage) {
        m_resultsPage->setBusy(m_request);
    }
    m_pages->setCurrentIndex(index);
    if (m_topPageLabel)
        m_topPageLabel->setText(navigationLabels().at(index));

    int workflowStage = -1;
    if (index == 0)
        workflowStage = 0;
    else if (index == kCalculationPageIndex)
        workflowStage = 1;
    else if (index == kResultsPageIndex)
        workflowStage = 2;
    for (int i = 0; i < m_workflowStepLabels.size(); ++i) {
        QLabel *step = m_workflowStepLabels.at(i);
        const QString state = workflowStage < 0
            ? QStringLiteral("pending")
            : (i < workflowStage ? QStringLiteral("done")
                                 : (i == workflowStage ? QStringLiteral("active") : QStringLiteral("pending")));
        step->setProperty("state", state);
        step->style()->unpolish(step);
        step->style()->polish(step);
    }
    for (int i = 0; i < m_navButtons.size(); ++i) {
        if (!m_navButtons.at(i))
            continue;
        m_navButtons.at(i)->setProperty("active", i == index);
        m_navButtons.at(i)->style()->unpolish(m_navButtons.at(i));
        m_navButtons.at(i)->style()->polish(m_navButtons.at(i));
    }
}

void MainWindow::ensurePureCalculationPage()
{
    if (m_pureCalculationPage || !m_pages)
        return;

    m_pureCalculationPage = new PureCalculationPage;
    replaceStackPage(m_pages, kPureCalculationPageIndex, m_pureCalculationPage);
}

void MainWindow::ensureCalculationPage()
{
    if (m_calculationPage || !m_pages)
        return;

    m_calculationPage = new CalculationPage;
    connect(m_calculationPage, &CalculationPage::recalculateRequested, this, [this]() {
        m_request = m_inputPage->request();
        refreshCalculationAssistant();
    });
    connect(m_calculationPage, &CalculationPage::inputRequested, this, [this]() { setActivePage(0); });
    connect(m_calculationPage, &CalculationPage::cameraSelectionChanged, this, [this](int row) {
        m_assistantSelectedCameraRow = row;
        refreshAssistantLensTable();
    });
    replaceStackPage(m_pages, kCalculationPageIndex, m_calculationPage);
}

void MainWindow::ensureResultsPage()
{
    if (m_resultsPage || !m_pages)
        return;

    m_resultsPage = new ResultsPage;
    connect(m_resultsPage, &ResultsPage::exportPdfRequested, this, &MainWindow::exportReportPdf);
    connect(m_resultsPage, &ResultsPage::exportBomRequested, this, &MainWindow::exportBomCsv);
    replaceStackPage(m_pages, kResultsPageIndex, m_resultsPage);
    if (!m_results.isEmpty())
        m_resultsPage->setResults(m_results, m_request);
}

void MainWindow::ensureCatalogPage()
{
    if (m_catalogPage || !m_pages)
        return;

    m_catalogPage = new CatalogPage;
    connect(m_catalogPage, &CatalogPage::cameraAddRequested, this, &MainWindow::addCamera);
    connect(m_catalogPage, &CatalogPage::cameraEditRequested, this, &MainWindow::editCamera);
    connect(m_catalogPage, &CatalogPage::cameraRemoveRequested, this, &MainWindow::removeCamera);
    connect(m_catalogPage, &CatalogPage::cameraImportRequested, this, &MainWindow::importCameras);
    connect(m_catalogPage, &CatalogPage::cameraExportRequested, this, &MainWindow::exportCameras);
    connect(m_catalogPage, &CatalogPage::cameraExportFilteredRequested, this, &MainWindow::exportFilteredCameras);
    connect(m_catalogPage, &CatalogPage::cameraResetRequested, this, &MainWindow::resetCameras);
    connect(m_catalogPage, &CatalogPage::lensAddRequested, this, &MainWindow::addLens);
    connect(m_catalogPage, &CatalogPage::lensEditRequested, this, &MainWindow::editLens);
    connect(m_catalogPage, &CatalogPage::lensRemoveRequested, this, &MainWindow::removeLens);
    connect(m_catalogPage, &CatalogPage::lensImportRequested, this, &MainWindow::importLenses);
    connect(m_catalogPage, &CatalogPage::lensExportRequested, this, &MainWindow::exportLenses);
    connect(m_catalogPage, &CatalogPage::lensExportFilteredRequested, this, &MainWindow::exportFilteredLenses);
    connect(m_catalogPage, &CatalogPage::lensResetRequested, this, &MainWindow::resetLenses);
    connect(m_catalogPage, &CatalogPage::lightAddRequested, this, &MainWindow::addLight);
    connect(m_catalogPage, &CatalogPage::lightEditRequested, this, &MainWindow::editLight);
    connect(m_catalogPage, &CatalogPage::lightRemoveRequested, this, &MainWindow::removeLight);
    connect(m_catalogPage, &CatalogPage::lightImportRequested, this, &MainWindow::importLights);
    connect(m_catalogPage, &CatalogPage::lightExportRequested, this, &MainWindow::exportLights);
    connect(m_catalogPage, &CatalogPage::lightExportFilteredRequested, this, &MainWindow::exportFilteredLights);
    connect(m_catalogPage, &CatalogPage::lightResetRequested, this, &MainWindow::resetLights);
    replaceStackPage(m_pages, kCatalogPageIndex, m_catalogPage);
}

void MainWindow::ensureThreeDCameraPage()
{
    if (m_threeDCameraPage || !m_pages)
        return;

    m_threeDCameraPage = new ThreeDCameraPage;
    replaceStackPage(m_pages, kThreeDCameraPageIndex, m_threeDCameraPage);
}

void MainWindow::ensureCatalogPageInitialized()
{
    ensureCatalogPage();
    if (m_catalogPageInitialized || !m_catalogPage)
        return;

    m_catalogPage->setCatalog(&m_catalog);
    m_catalogPageInitialized = true;
}

void MainWindow::calculate()
{
    if (!m_inputPage)
        return;
    m_request = m_inputPage->request();
    startSelectionCalculation(m_request);
}

void MainWindow::startSelectionCalculation(const SelectionRequest &request)
{
    if (!m_selectionWatcher)
        return;

    if (m_selectionWatcher->isRunning()) {
        m_pendingSelectionRequest = request;
        m_hasPendingSelectionRequest = true;
        refreshSidebarSummary();
        if (m_resultsPage)
            m_resultsPage->setBusy(request);
        return;
    }

    m_request = request;
    m_results.clear();
    if (m_resultsPage)
        m_resultsPage->setBusy(request);
    if (m_pages && m_pages->currentIndex() == kCalculationPageIndex)
        refreshCalculationAssistant();

    const QString storageDirectory = m_catalog.storageDirectory();
    const QString languageCode = LanguageManager::instance().currentLanguage();
    m_selectionWatcher->setFuture(QtConcurrent::run(runSelectionJob, storageDirectory, request, 20, languageCode));
    refreshSidebarSummary();
}

void MainWindow::finishSelectionCalculation()
{
    if (!m_selectionWatcher)
        return;

    const SelectionJobResult result = m_selectionWatcher->result();
    m_request = result.request;
    m_results = result.results;

    if (!result.error.isEmpty())
        showError(result.error);
    if (m_pages && m_pages->currentIndex() == kCalculationPageIndex)
        refreshCalculationAssistant();
    if (m_resultsPage)
        m_resultsPage->setResults(m_results, m_request);
    refreshCatalogTables();
    refreshSidebarSummary();

    if (m_hasPendingSelectionRequest) {
        const SelectionRequest pending = m_pendingSelectionRequest;
        m_hasPendingSelectionRequest = false;
        startSelectionCalculation(pending);
    }
}

bool MainWindow::selectionCalculationRunning() const
{
    return m_selectionWatcher && m_selectionWatcher->isRunning();
}

void MainWindow::refreshCalculationAssistant()
{
    if (!m_calculationPage)
        return;

    const RequirementEstimate requirement = CalculationAssistant::estimateRequirement(m_request);
    const QVector<CameraSpec> cameraCandidates = m_catalog.selectionCandidateCameras(m_request, 300);
    m_assistantCameraEstimates = CalculationAssistant::estimateCameras(m_request, cameraCandidates, 12);

    m_calculationPage->setSummary(localizedText("需求 FOV：%1 x %2 mm，目标物方像素：%3 um/px，相机下限：%4 x %5（%6 MP）",
                                                "Required FOV: %1 x %2 mm, target object pixel: %3 um/px, minimum camera: %4 x %5 (%6 MP)")
        .arg(requirement.requiredFovWidthMm, 0, 'f', 2)
        .arg(requirement.requiredFovHeightMm, 0, 'f', 2)
        .arg(requirement.targetObjectPixelUm, 0, 'f', 2)
        .arg(requirement.requiredResolutionX)
        .arg(requirement.requiredResolutionY)
        .arg(requirement.requiredMegapixels, 0, 'f', 2));

    m_calculationPage->setCameraEstimates(m_assistantCameraEstimates);
    m_assistantSelectedCameraRow = m_calculationPage->selectedCameraEstimateRow();
    refreshAssistantLensTable();
}

void MainWindow::refreshAssistantLensTable()
{
    if (!m_calculationPage)
        return;

    const RequirementEstimate requirement = CalculationAssistant::estimateRequirement(m_request);
    QString details;
    details += localizedText("参数要求\n", "Requirements\n");
    details += localizedText("- 最低分辨率：%1 x %2，建议不低于 %3 MP。\n",
                             "- Minimum resolution: %1 x %2, recommended at least %3 MP.\n")
        .arg(requirement.requiredResolutionX)
        .arg(requirement.requiredResolutionY)
        .arg(requirement.requiredMegapixels, 0, 'f', 2);
    details += localizedText("- 12 bit 原始数据带宽估算：%1 MB/s @ %2 fps。\n",
                             "- Estimated 12-bit raw bandwidth: %1 MB/s @ %2 fps.\n")
        .arg(requirement.requiredBandwidthMBps12Bit, 0, 'f', 1)
        .arg(m_request.requiredFps, 0, 'f', 1);
    details += localizedText("- 镜头类型倾向：%1。\n", "- Lens type preference: %1.\n")
        .arg(requirement.telecentricPreferred
            ? localizedText("高精度/高度波动，优先评估远心镜头", "high precision / height variation, evaluate telecentric lenses first")
            : localizedText("普通工业镜头可以先粗算", "fixed-focal industrial lenses can be estimated first"));
    details += localizedText("- 运动曝光上限：%1。\n", "- Motion exposure limit: %1.\n")
        .arg(requirement.hasMotionConstraint
            ? QStringLiteral("%1 us").arg(requirement.maxExposureUsForOnePixelBlur, 0, 'f', 1)
            : localizedText("无运动模糊约束", "no motion blur constraint"));

    if (m_assistantSelectedCameraRow < 0 || m_assistantSelectedCameraRow >= m_assistantCameraEstimates.size()) {
        m_calculationPage->setLensEstimates(QVector<LensCalculationEstimate>());
        details += localizedText("\n镜头候选：暂无可用相机估算。", "\nLens candidates: no available camera estimate.");
        m_calculationPage->setDetails(details);
        return;
    }

    const CameraSpec camera = m_assistantCameraEstimates.at(m_assistantSelectedCameraRow).camera;
    const QVector<LensSpec> lensCandidates = m_catalog.selectionCandidateLenses(m_request, 500);
    m_assistantLensEstimates = CalculationAssistant::estimateLenses(m_request, camera, lensCandidates, 12);
    m_calculationPage->setLensEstimates(m_assistantLensEstimates);

    details += localizedText("\n当前镜头候选基于相机：%1。\n", "\nCurrent lens candidates are based on camera: %1.\n")
        .arg(productLabel(camera.manufacturer, camera.model));
    if (!m_assistantLensEstimates.isEmpty()) {
        const LensCalculationEstimate &top = m_assistantLensEstimates.first();
        details += localizedText("- 首选镜头：%1 %2，FOV %3 x %4 mm，物方像素 %5 um/px。\n",
                                 "- Top lens: %1 %2, FOV %3 x %4 mm, object pixel %5 um/px.\n")
            .arg(top.lens.manufacturer, top.lens.model)
            .arg(top.effectiveFovWidthMm, 0, 'f', 2)
            .arg(top.effectiveFovHeightMm, 0, 'f', 2)
            .arg(top.objectPixelSizeUm, 0, 'f', 2);
        details += localizedText("- 工程估算：DOF %1 mm，畸变边缘误差约 %2 um。\n",
                                 "- Engineering estimate: DOF %1 mm, distortion edge error about %2 um.\n")
            .arg(top.estimatedDofMm, 0, 'f', 2)
            .arg(top.distortionErrorUm, 0, 'f', 2);
        details += localizedText("- 公式：%1。\n", "- Formula: %1.\n").arg(top.formulaSummary);
        details += localizedText("- 推荐理由：%1。\n", "- Reasons: %1.\n")
            .arg(top.reasons.isEmpty() ? localizedText("按综合参数排名", "ranked by combined parameters") : top.reasons.join(localizedText("；", "; ")));
        details += localizedText("- 风险：%1", "- Risks: %1")
            .arg(top.risks.isEmpty() ? localizedText("无主要风险", "No major risk") : top.risks.join(localizedText("；", "; ")));
    } else {
        details += localizedText("- 没有符合当前限制的镜头候选。", "- No lens candidates match the current constraints.");
    }
    m_calculationPage->setDetails(details);
}
void MainWindow::refreshCatalogTables()
{
    if (m_catalogPageInitialized && m_catalogPage)
        m_catalogPage->setCatalog(&m_catalog);
    refreshSidebarSummary();
}

void MainWindow::handleCatalogMutation()
{
    refreshCatalogTables();
    calculate();
}

void MainWindow::importCameras()
{
    const QString path = QFileDialog::getOpenFileName(this, localizedText("导入相机 CSV", "Import Camera CSV"), QString(), QStringLiteral("CSV (*.csv)"));
    if (path.isEmpty())
        return;
    QString error;
    if (!m_catalog.loadCameraCsv(path, &error)) {
        showError(error);
        return;
    }
    handleCatalogMutation();
}

void MainWindow::importLenses()
{
    const QString path = QFileDialog::getOpenFileName(this, localizedText("导入镜头 CSV", "Import Lens CSV"), QString(), QStringLiteral("CSV (*.csv)"));
    if (path.isEmpty())
        return;
    QString error;
    if (!m_catalog.loadLensCsv(path, &error)) {
        showError(error);
        return;
    }
    handleCatalogMutation();
}

void MainWindow::importLights()
{
    const QString path = QFileDialog::getOpenFileName(this, localizedText("导入光源 CSV", "Import Light CSV"), QString(), QStringLiteral("CSV (*.csv)"));
    if (path.isEmpty())
        return;
    QString error;
    if (!m_catalog.loadLightCsv(path, &error)) {
        showError(error);
        return;
    }
    handleCatalogMutation();
}

void MainWindow::exportCameras()
{
    const QString path = QFileDialog::getSaveFileName(this, tr("Export Camera CSV"), QStringLiteral("cameras.csv"), QStringLiteral("CSV (*.csv)"));
    if (path.isEmpty())
        return;
    QString error;
    if (!m_catalog.exportCameraCsvByQuery(path, CatalogQuery(), &error)) {
        showError(error);
        return;
    }
    QMessageBox::information(this, tr("Export Complete"), path);
}

void MainWindow::exportLenses()
{
    const QString path = QFileDialog::getSaveFileName(this, tr("Export Lens CSV"), QStringLiteral("lenses.csv"), QStringLiteral("CSV (*.csv)"));
    if (path.isEmpty())
        return;
    QString error;
    if (!m_catalog.exportLensCsvByQuery(path, CatalogQuery(), &error)) {
        showError(error);
        return;
    }
    QMessageBox::information(this, tr("Export Complete"), path);
}

void MainWindow::exportLights()
{
    const QString path = QFileDialog::getSaveFileName(this, tr("Export Light CSV"), QStringLiteral("lights.csv"), QStringLiteral("CSV (*.csv)"));
    if (path.isEmpty())
        return;
    QString error;
    if (!m_catalog.exportLightCsvByQuery(path, CatalogQuery(), &error)) {
        showError(error);
        return;
    }
    QMessageBox::information(this, tr("Export Complete"), path);
}

void MainWindow::exportFilteredCameras()
{
    const QString path = QFileDialog::getSaveFileName(this, tr("Export Filtered Camera CSV"), QStringLiteral("cameras_filtered.csv"), QStringLiteral("CSV (*.csv)"));
    if (path.isEmpty())
        return;
    QString error;
    const CatalogQuery query = m_catalogPage ? m_catalogPage->currentCameraQuery() : CatalogQuery();
    if (!m_catalog.exportCameraCsvByQuery(path, query, &error)) {
        showError(error);
        return;
    }
    QMessageBox::information(this, tr("Export Complete"), path);
}

void MainWindow::exportFilteredLenses()
{
    const QString path = QFileDialog::getSaveFileName(this, tr("Export Filtered Lens CSV"), QStringLiteral("lenses_filtered.csv"), QStringLiteral("CSV (*.csv)"));
    if (path.isEmpty())
        return;
    QString error;
    const CatalogQuery query = m_catalogPage ? m_catalogPage->currentLensQuery() : CatalogQuery();
    if (!m_catalog.exportLensCsvByQuery(path, query, &error)) {
        showError(error);
        return;
    }
    QMessageBox::information(this, tr("Export Complete"), path);
}

void MainWindow::exportFilteredLights()
{
    const QString path = QFileDialog::getSaveFileName(this, tr("Export Filtered Light CSV"), QStringLiteral("lights_filtered.csv"), QStringLiteral("CSV (*.csv)"));
    if (path.isEmpty())
        return;
    QString error;
    const CatalogQuery query = m_catalogPage ? m_catalogPage->currentLightQuery() : CatalogQuery();
    if (!m_catalog.exportLightCsvByQuery(path, query, &error)) {
        showError(error);
        return;
    }
    QMessageBox::information(this, tr("Export Complete"), path);
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
    if (!editCameraDialog(this, &camera, tr("Add Camera")))
        return;
    QString error;
    if (!m_catalog.addCamera(camera, &error)) {
        showError(error);
        return;
    }
    handleCatalogMutation();
}

void MainWindow::editCamera()
{
    const qint64 id = m_catalogPage ? m_catalogPage->selectedCameraId() : -1;
    if (id < 0)
        return;
    CameraSpec camera;
    QString error;
    if (!m_catalog.cameraById(id, &camera, &error)) {
        showError(error);
        return;
    }
    if (!editCameraDialog(this, &camera, tr("Edit Camera")))
        return;
    if (!m_catalog.updateCameraById(id, camera, &error)) {
        showError(error);
        return;
    }
    handleCatalogMutation();
}

void MainWindow::removeCamera()
{
    const qint64 id = m_catalogPage ? m_catalogPage->selectedCameraId() : -1;
    if (id < 0)
        return;
    CameraSpec camera;
    QString error;
    if (!m_catalog.cameraById(id, &camera, &error)) {
        showError(error);
        return;
    }
    if (QMessageBox::question(this, tr("Delete Camera"),
            tr("Delete camera %1?").arg(productLabel(camera.manufacturer, camera.model))) != QMessageBox::Yes) {
        return;
    }
    if (!m_catalog.removeCameraById(id, &error)) {
        showError(error);
        return;
    }
    handleCatalogMutation();
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
    if (!editLensDialog(this, &lens, tr("Add Lens")))
        return;
    QString error;
    if (!m_catalog.addLens(lens, &error)) {
        showError(error);
        return;
    }
    handleCatalogMutation();
}

void MainWindow::editLens()
{
    const qint64 id = m_catalogPage ? m_catalogPage->selectedLensId() : -1;
    if (id < 0)
        return;
    LensSpec lens;
    QString error;
    if (!m_catalog.lensById(id, &lens, &error)) {
        showError(error);
        return;
    }
    if (!editLensDialog(this, &lens, tr("Edit Lens")))
        return;
    if (!m_catalog.updateLensById(id, lens, &error)) {
        showError(error);
        return;
    }
    handleCatalogMutation();
}

void MainWindow::removeLens()
{
    const qint64 id = m_catalogPage ? m_catalogPage->selectedLensId() : -1;
    if (id < 0)
        return;
    LensSpec lens;
    QString error;
    if (!m_catalog.lensById(id, &lens, &error)) {
        showError(error);
        return;
    }
    if (QMessageBox::question(this, tr("Delete Lens"),
            tr("Delete lens %1?").arg(productLabel(lens.manufacturer, lens.model))) != QMessageBox::Yes) {
        return;
    }
    if (!m_catalog.removeLensById(id, &error)) {
        showError(error);
        return;
    }
    handleCatalogMutation();
}

void MainWindow::addLight()
{
    LightSpec light;
    light.model = QStringLiteral("NEW-LIGHT");
    light.manufacturer = QStringLiteral("Manual");
    light.lightType = LightType::Ring;
    light.color = QStringLiteral("White");
    light.mode = QStringLiteral("Continuous");
    light.activeWidthMm = 100.0;
    light.activeHeightMm = 100.0;
    if (!editLightDialog(this, &light, tr("Add Light")))
        return;
    QString error;
    if (!m_catalog.addLight(light, &error)) {
        showError(error);
        return;
    }
    handleCatalogMutation();
}

void MainWindow::editLight()
{
    const qint64 id = m_catalogPage ? m_catalogPage->selectedLightId() : -1;
    if (id < 0)
        return;
    LightSpec light;
    QString error;
    if (!m_catalog.lightById(id, &light, &error)) {
        showError(error);
        return;
    }
    if (!editLightDialog(this, &light, tr("Edit Light")))
        return;
    if (!m_catalog.updateLightById(id, light, &error)) {
        showError(error);
        return;
    }
    handleCatalogMutation();
}

void MainWindow::removeLight()
{
    const qint64 id = m_catalogPage ? m_catalogPage->selectedLightId() : -1;
    if (id < 0)
        return;
    LightSpec light;
    QString error;
    if (!m_catalog.lightById(id, &light, &error)) {
        showError(error);
        return;
    }
    if (QMessageBox::question(this, tr("Delete Light"),
            tr("Delete light %1?").arg(productLabel(light.manufacturer, light.model))) != QMessageBox::Yes) {
        return;
    }
    if (!m_catalog.removeLightById(id, &error)) {
        showError(error);
        return;
    }
    handleCatalogMutation();
}

void MainWindow::resetCameras()
{
    if (QMessageBox::question(this, tr("Reset Camera Catalog"),
            tr("Overwrite local camera catalog data with the built-in camera catalog?")) != QMessageBox::Yes) {
        return;
    }
    QString error;
    if (!m_catalog.resetCamerasToBuiltIn(&error)) {
        showError(error);
        return;
    }
    handleCatalogMutation();
}

void MainWindow::resetLenses()
{
    if (QMessageBox::question(this, tr("Reset Lens Catalog"),
            tr("Overwrite local lens catalog data with the built-in lens catalog?")) != QMessageBox::Yes) {
        return;
    }
    QString error;
    if (!m_catalog.resetLensesToBuiltIn(&error)) {
        showError(error);
        return;
    }
    handleCatalogMutation();
}

void MainWindow::resetLights()
{
    if (QMessageBox::question(this, tr("Reset Light Catalog"),
            tr("Overwrite local light catalog data with the built-in light catalog?")) != QMessageBox::Yes) {
        return;
    }
    QString error;
    if (!m_catalog.resetLightsToBuiltIn(&error)) {
        showError(error);
        return;
    }
    handleCatalogMutation();
}

void MainWindow::exportBomCsv()
{
    if (selectionCalculationRunning()) {
        showError(tr("Recommendation calculation is still running. Please export after it completes."));
        return;
    }
    if (m_results.isEmpty()) {
        calculate();
        showError(tr("Recommendation calculation has started. Please export after it completes."));
        return;
    }

    const QString defaultName = QStringLiteral("VisionSelect_BOM_%1.csv")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
    const QString path = QFileDialog::getSaveFileName(this, tr("Export BOM CSV"), defaultName, QStringLiteral("CSV (*.csv)"));
    if (path.isEmpty())
        return;

    QFile file(path);
    if (!file.open(QIODevice::WriteOnly | QIODevice::Text)) {
        showError(tr("Unable to write BOM CSV: %1").arg(path));
        return;
    }

    QTextStream out(&file);
    out.setCodec("UTF-8");
    out << "scheme,rank,category,manufacturer,model,key_specs,notes,project_notes\n";
    const int count = qMin(5, m_results.size());
    for (int i = 0; i < count; ++i) {
        const SelectionResult &r = m_results.at(i);
        const QString scheme = QStringLiteral("#%1 %2").arg(i + 1).arg(r.schemeTitle);
        out << csvCell(scheme) << "," << (i + 1) << ","
            << csvCell(tr("Camera")) << ","
            << csvCell(r.camera.manufacturer) << ","
            << csvCell(r.camera.model) << ","
            << csvCell(bomSpecForCamera(r.camera, r)) << ","
            << csvCell(QStringLiteral("FOV %1 x %2 mm; object pixel %3 um")
                .arg(r.effectiveFovWidthMm, 0, 'f', 2)
                .arg(r.effectiveFovHeightMm, 0, 'f', 2)
                .arg(r.objectPixelSizeUm, 0, 'f', 2))
            << "," << csvCell(m_request.projectNotes) << "\n";
        out << csvCell(scheme) << "," << (i + 1) << ","
            << csvCell(tr("Lens")) << ","
            << csvCell(r.lens.manufacturer) << ","
            << csvCell(r.lens.model) << ","
            << csvCell(bomSpecForLens(r.lens, r)) << ","
            << csvCell(QStringLiteral("distortion %1 um; lens MP utilization %2%")
                .arg(r.distortionErrorUm, 0, 'f', 2)
                .arg(r.lensMegapixelUtilizationPercent, 0, 'f', 0))
            << "," << csvCell(m_request.projectNotes) << "\n";
        out << csvCell(scheme) << "," << (i + 1) << ","
            << csvCell(tr("Light")) << ","
            << csvCell(r.light.manufacturer) << ","
            << csvCell(r.light.model) << ","
            << csvCell(bomSpecForLight(r.light, r)) << ","
            << csvCell(riskSummary(r)) << ","
            << csvCell(m_request.projectNotes) << "\n";
    }
    file.close();
    QMessageBox::information(this, tr("Export Complete"), path);
}

void MainWindow::exportReportPdf()
{
    if (selectionCalculationRunning()) {
        showError(tr("Recommendation calculation is still running. Please export after it completes."));
        return;
    }
    if (m_results.isEmpty()) {
        calculate();
        showError(tr("Recommendation calculation has started. Please export after it completes."));
        return;
    }

    const QString defaultName = QStringLiteral("VisionSelect_%1.pdf")
        .arg(QDateTime::currentDateTime().toString(QStringLiteral("yyyyMMdd_HHmmss")));
    const QString path = QFileDialog::getSaveFileName(this, localizedText("导出 PDF 报告", "Export PDF Report"), defaultName, QStringLiteral("PDF (*.pdf)"));
    if (path.isEmpty())
        return;

    PdfReportWriter writer;
    QString error;
    if (!writer.write(path, m_request, m_results, &error)) {
        showError(error);
        return;
    }
    QMessageBox::information(this, localizedText("导出完成", "Export Complete"),
        localizedText("PDF 报告已生成：\n%1", "PDF report generated:\n%1").arg(path));
}

void MainWindow::showError(const QString &message)
{
    QMessageBox::warning(this, localizedText("操作失败", "Operation Failed"), message);
}
