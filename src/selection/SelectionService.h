#ifndef SELECTIONSERVICE_H
#define SELECTIONSERVICE_H

#include "core/SelectionTypes.h"

#include <QVector>

class CatalogRepository;

class SelectionService
{
public:
    explicit SelectionService(const CatalogRepository *catalog);

    QVector<SelectionResult> select(const SelectionRequest &request, int limit, QString *errorMessage = nullptr) const;

private:
    const CatalogRepository *m_catalog = nullptr;
};

#endif
