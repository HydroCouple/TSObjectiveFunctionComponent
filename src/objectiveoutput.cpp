#include "stdafx.h"
#include "objectiveoutput.h"
#include "hydrocoupletemporal.h"
#include "timeseries.h"
#include "core/dimension.h"
#include "core/valuedefinition.h"

#include <unordered_map>

using namespace HydroCouple;
using namespace HydroCouple::Temporal;

ObjectiveOutput::ObjectiveOutput(const QString &id,
                                 TSObjectiveFunctionComponent::Algorithm algorithm,
                                 ObjectiveInput *objectiveInput,
                                 TSObjectiveFunctionComponent *component)
  : GeometryOutputDouble(id,
                         objectiveInput->geometryType(),
                         dynamic_cast<Dimension*>(objectiveInput->geometryDimension()),
                         dynamic_cast<ValueDefinition*>(objectiveInput->valueDefinition()),
                         component),
    m_objectiveInput(objectiveInput),
    m_algorithm(algorithm),
    m_objectiveFunctionComponent(component)
{

}

ObjectiveOutput::~ObjectiveOutput()
{
}

void ObjectiveOutput::updateValues(HydroCouple::IInput *querySpecifier)
{

  if(!m_objectiveFunctionComponent->workflow())
  {
    ITimeComponentDataItem* timeExchangeItem = dynamic_cast<ITimeComponentDataItem*>(querySpecifier);
    QList<IOutput*>updateList;

    if(timeExchangeItem)
    {
      double queryTime = timeExchangeItem->time(timeExchangeItem->timeCount() - 1)->julianDay();

      while (m_objectiveFunctionComponent->timeHorizon()->julianDay() < queryTime &&
             m_objectiveFunctionComponent->status() == IModelComponent::Updated)
      {
        m_objectiveFunctionComponent->update(updateList);
      }
    }
    else
    {
      if(m_objectiveFunctionComponent->status() == IModelComponent::Updated)
      {
        m_objectiveFunctionComponent->update(updateList);
      }
    }
  }

  refreshAdaptedOutputs();
}

void ObjectiveOutput::updateValues()
{
  if(m_objectiveInput->currentDateTime() >= m_objectiveFunctionComponent->timeHorizon()->julianDay() + m_objectiveFunctionComponent->timeHorizon()->duration())
  {
    for(int g = 0; g < geometryCount(); g++)
    {
      std::vector<double> obsValues;
      std::vector<double> simValues;

      double obsValueSum = 0;
      double simValueSum = 0;

      for(int i = 0 ; i < m_objectiveInput->timeCount(); i++)
      {
        int index = m_objectiveInput->startDateTimeIndex() +  i;

        double obsDateTime = m_objectiveInput->timeSeries()->dateTime(index);
        double simDateTime = m_objectiveInput->time(i)->julianDay();

        if(obsDateTime == simDateTime)
        {
          double simValue = 0.0;
          m_objectiveInput->getValue(i, g, & simValue);
          simValueSum += simValue;
          simValues.push_back(simValue);

          double obsValue = m_objectiveInput->timeSeries()->value(index, g);
          obsValueSum += obsValue;
          obsValues.push_back(obsValue);
        }
      }

      double numValues = obsValues.size();

      switch (m_algorithm)
      {
        case TSObjectiveFunctionComponent::NashSutcliff:
          {
            double numer = 0.0;
            double denom = 0.0;

            for(size_t l = 0; l < obsValues.size(); l++)
            {
              double diffNumer = obsValues[l] - simValues[l];
              double diffDenom = obsValues[l] - obsValueSum / numValues;
              numer += diffNumer * diffNumer;
              denom += diffDenom * diffDenom;
            }

            double metric = numer/denom;
            metric = isinf(metric) || isnan(metric) ? std::numeric_limits<double>::max() : metric;
            setValue(g, &metric);
          }
          break;
        case TSObjectiveFunctionComponent::RMSE:
          {
            double sumDiffSqr = 0.0;

            for(size_t l = 0; l < obsValues.size(); l++)
            {
              double diff = obsValues[l] - simValues[l];
              sumDiffSqr += diff * diff;
            }

            double metric = sqrt(sumDiffSqr / numValues);
            metric = isinf(metric) || isnan(metric) ? std::numeric_limits<double>::max() : metric;
            setValue(g, &metric);
          }
          break;
        case TSObjectiveFunctionComponent::MAE:
          {
            double sumAbsDiff = 0.0;

            for(size_t l = 0; l < obsValues.size(); l++)
            {
              sumAbsDiff += fabs(obsValues[l] - simValues[l]);
            }

            double metric = sqrt(sumAbsDiff / numValues);
            metric = isinf(metric) || isnan(metric) ? std::numeric_limits<double>::max() : metric;
            setValue(g, &metric);
          }
          break;

      }
    }
  }
}
