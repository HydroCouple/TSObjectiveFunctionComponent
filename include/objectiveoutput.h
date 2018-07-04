#ifndef OBJECTIVEOUTPUT_H
#define OBJECTIVEOUTPUT_H

#include "tsobjectivefunctioncomponent_global.h"
#include "tsobjectivefunctioncomponent.h"
#include "spatial/geometryexchangeitems.h"
#include "objectiveinput.h"

class TSOBJECTIVEFUNCTIONCOMPONENT_EXPORT ObjectiveOutput : public GeometryOutputDouble
{
    Q_OBJECT

  public:

    ObjectiveOutput(const QString &id,
                    TSObjectiveFunctionComponent::Algorithm algorithm,
                    ObjectiveInput *objectiveInput,
                    TSObjectiveFunctionComponent *component);

    virtual ~ObjectiveOutput();

    void updateValues(HydroCouple::IInput *querySpecifier) override;

    void updateValues() override;

  private:

    ObjectiveInput *m_objectiveInput;
    TSObjectiveFunctionComponent::Algorithm m_algorithm;
    TSObjectiveFunctionComponent *m_objectiveFunctionComponent;
};


#endif // OBJECTIVEOUTPUT_H
