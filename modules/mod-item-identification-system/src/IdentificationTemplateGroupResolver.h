#ifndef IDENTIFICATION_TEMPLATE_GROUP_RESOLVER_H
#define IDENTIFICATION_TEMPLATE_GROUP_RESOLVER_H

#include <set>

inline unsigned int ResolveIdentificationTemplateGroup(unsigned int requestedGroup, std::set<unsigned int> const& availableGroups)
{
    if (availableGroups.find(requestedGroup) != availableGroups.end())
        return requestedGroup;

    constexpr unsigned int FormulaTemplateGroup = 1;
    if (availableGroups.find(FormulaTemplateGroup) != availableGroups.end())
        return FormulaTemplateGroup;

    return requestedGroup;
}

#endif
