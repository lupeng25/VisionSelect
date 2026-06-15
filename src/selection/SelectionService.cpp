#include "selection/SelectionService.h"

#include "catalog/CatalogRepository.h"
#include "selection/SelectionEngine.h"

SelectionService::SelectionService(const CatalogRepository *catalog)
    : m_catalog(catalog)
{
}

QVector<SelectionResult> SelectionService::select(const SelectionRequest &request, int limit, QString *errorMessage) const
{
    if (!m_catalog) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8("产品库未初始化");
        return QVector<SelectionResult>();
    }

    const QVector<CameraSpec> cameras = m_catalog->selectionCandidateCameras(request, 300, errorMessage);
    if (cameras.isEmpty())
        return QVector<SelectionResult>();
    const QVector<LensSpec> lenses = m_catalog->selectionCandidateLenses(request, 500, errorMessage);
    if (lenses.isEmpty())
        return QVector<SelectionResult>();
    bool hasTelecentricLens = false;
    bool hasCoaxialLens = false;
    for (const LensSpec &lens : lenses) {
        hasTelecentricLens = hasTelecentricLens || lens.isTelecentric();
        hasCoaxialLens = hasCoaxialLens || lens.coaxialIllumination;
    }
    const QVector<LightSpec> lights = m_catalog->selectionCandidateLights(request, hasTelecentricLens, hasCoaxialLens, 300, errorMessage);
    if (lights.isEmpty())
        return QVector<SelectionResult>();

    SelectionEngine engine;
    return engine.select(request, cameras, lenses, lights, limit);
}
