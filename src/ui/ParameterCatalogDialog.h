#ifndef PARAMETERCATALOGDIALOG_H
#define PARAMETERCATALOGDIALOG_H

#include "core/SelectionTypes.h"
#include "selection/ParameterCalculator.h"
#include <optional>

class CatalogRepository;
class QWidget;

namespace ParameterCatalogDialog {
std::optional<CameraSpec> camera(QWidget *parent, const CatalogRepository &catalog);
std::optional<LensSpec> lens(QWidget *parent, const CatalogRepository &catalog,
                            const Parameters::SystemInput &context, bool matched);
}

#endif
