#include "selection/SelectionService.h"

#include "catalog/CatalogRepository.h"
#include "selection/SelectionEngine.h"
#include <algorithm>

SelectionService::SelectionService(const CatalogRepository *catalog)
    : m_catalog(catalog)
{
}

QVector<SelectionResult> SelectionService::select(const SelectionRequest &request, int limit,
                                                  QString *errorMessage,
                                                  const QString &languageCode) const
{
    if (!m_catalog) {
        if (errorMessage)
            *errorMessage = QString::fromUtf8("产品库未初始化");
        return QVector<SelectionResult>();
    }

    QString localError;
    QString *error = errorMessage ? errorMessage : &localError;
    error->clear();
    const int totalCameras = m_catalog->productCount(CatalogDomain::Camera, error);
    if (!error->isEmpty()) return {};
    const int totalLenses = m_catalog->productCount(CatalogDomain::Lens, error);
    if (!error->isEmpty()) return {};
    int cameraLimit = 300, lensLimit = 500;
    SelectionEngine engine;
    for (;;) {
        const auto cameras = m_catalog->selectionCandidateCameras(request, cameraLimit, error);
        if (!error->isEmpty() || cameras.isEmpty()) return {};
        const auto lenses = m_catalog->selectionCandidateLenses(request, lensLimit, error);
        if (!error->isEmpty() || lenses.isEmpty()) return {};
        bool tele = false, coaxial = false;
        for (const auto &lens : lenses) { tele |= lens.isTelecentric(); coaxial |= lens.coaxialIllumination; }
        const auto lights = m_catalog->selectionCandidateLights(request, tele, coaxial, 300, error);
        if (!error->isEmpty() || lights.isEmpty()) return {};
        auto results = engine.select(request, cameras, lenses, lights, limit, languageCode);
        const bool feasible = std::any_of(results.cbegin(), results.cend(), [](const auto &r) { return r.hardConstraintsPassed; });
        const double pixel = SelectionEngine::targetObjectPixelUm(request);
        // 仓储先取满足分辨率的行；没有满足采样下限的相机时，扩大镜头集合也不能通过。
        const bool samplingPossible = pixel > 0 && std::any_of(cameras.cbegin(), cameras.cend(), [&](const auto &c) {
            return c.resolutionX * pixel + 1e-8 >= SelectionEngine::requiredFovWidth(request) * 1000
                && c.resolutionY * pixel + 1e-8 >= SelectionEngine::requiredFovHeight(request) * 1000;
        });
        if (feasible || (cameraLimit >= totalCameras && lensLimit >= totalLenses) || !samplingPossible) {
            for (auto &r : results) {
                r.searchedCameras = cameras.size(); r.catalogCameras = totalCameras;
                r.searchedLenses = lenses.size(); r.catalogLenses = totalLenses;
            }
            return results;
        }
        // 没有可行组合时扩大召回，不能把首批诊断候选当成全库无解。
        cameraLimit = qMin(totalCameras, cameraLimit > totalCameras / 2 ? totalCameras : cameraLimit * 2);
        lensLimit = qMin(totalLenses, lensLimit > totalLenses / 2 ? totalLenses : lensLimit * 2);
    }
}
