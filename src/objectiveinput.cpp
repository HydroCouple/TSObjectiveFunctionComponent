#include "stdafx.h"
#include "objectiveinput.h"
#include "temporal/timeseries.h"
#include "spatial/geometry.h"
#include "temporal/timedata.h"
#include "core/valuedefinition.h"

using namespace HydroCouple::Spatial;


ObjectiveInput::ObjectiveInput(const QString &id,
                               TimeSeries *timeSeries,
                               Dimension *timeDimension,
                               Dimension *geometryDimension,
                               HydroCouple::Spatial::IGeometry::GeometryType geometryType,
                               Quantity *valueDefinition,
                               TSObjectiveFunctionComponent *component)
  : TimeGeometryInputDouble(id, geometryType, timeDimension, geometryDimension, valueDefinition, component),
    m_startDateTimeIndex(0),
    m_endDateTimeIndex(0),
    m_nextDateTimeIndex(0),
    m_timeSeries(timeSeries),
    m_objectiveFunctionComponent(component)
{
}

ObjectiveInput::~ObjectiveInput()
{

}

void ObjectiveInput::initialize()
{

  double startTime = m_objectiveFunctionComponent->timeHorizon()->julianDay();
  double endTime = startTime + m_objectiveFunctionComponent->timeHorizon()->duration();

  for(int i = 0 ; i < m_timeSeries->numRows() - 1; i++)
  {
    double dateTime = m_timeSeries->dateTime(i);

    if(dateTime >= startTime)
    {
      m_startDateTimeIndex = i;
      m_nextDateTimeIndex = i;
      m_currentDateTime = dateTime;

      addTime(new SDKTemporal::DateTime(m_currentDateTime, nullptr));

      for(int j = i; j < m_timeSeries->numRows(); j++)
      {
        dateTime = m_timeSeries->dateTime(j);

        if(dateTime <= endTime)
        {
          m_endDateTimeIndex = j;
        }
        else
        {
          break;
        }
      }
      break;
    }
  }
}

int ObjectiveInput::startDateTimeIndex() const
{
  return m_startDateTimeIndex;
}

int ObjectiveInput::recordLength() const
{
  return m_endDateTimeIndex - m_startDateTimeIndex + 1;
}

double ObjectiveInput::currentDateTime() const
{
  return m_currentDateTime;
}

void ObjectiveInput::moveToNextDateTime()
{
  if(provider()->modelComponent()->status() == HydroCouple::IModelComponent::ComponentStatus::Updated)
  {
    //unnecessary fix later
    int nextIndex = m_nextDateTimeIndex + 1;

    if( nextIndex <= m_endDateTimeIndex &&
        nextIndex < m_timeSeries->numRows())
    {
      m_nextDateTimeIndex++;
      m_currentDateTime = m_timeSeries->dateTime(m_nextDateTimeIndex);
    }
    else
    {
      m_currentDateTime = m_objectiveFunctionComponent->timeHorizon()->julianDay() + m_objectiveFunctionComponent->timeHorizon()->duration() + 0.000001;
    }
  }
  else
  {
    m_currentDateTime = m_objectiveFunctionComponent->timeHorizon()->julianDay() + m_objectiveFunctionComponent->timeHorizon()->duration() + 0.000001;
  }
}

bool ObjectiveInput::setProvider(HydroCouple::IOutput *provider)
{
  m_geometryMapping.clear();

  if(AbstractInput::setProvider(provider) && provider)
  {
    ITimeGeometryComponentDataItem *timeGeometryDataItem = dynamic_cast<ITimeGeometryComponentDataItem*>(provider);

    if(timeGeometryDataItem->geometryCount())
    {
      for(int i = 0; i < geometryCount() ; i++)
      {
        HCGeometry *geometry = getGeometry(i);

        for(int j = 0; j < timeGeometryDataItem->geometryCount() ; j++)
        {
          IGeometry *providerGeometry = timeGeometryDataItem->geometry(j);

          if(equalsGeometry(geometry, providerGeometry))
          {
            m_geometryMapping[i] = j;
            break;
          }
        }
      }
    }

    return true;
  }

  return false;
}

bool ObjectiveInput::canConsume(HydroCouple::IOutput *provider, QString &message) const
{
  ITimeGeometryComponentDataItem *timeGeometryDataItem = nullptr;

  if((timeGeometryDataItem = dynamic_cast<ITimeGeometryComponentDataItem*>(provider)) &&
     (timeGeometryDataItem->geometryType() == IGeometry::LineString ||
      timeGeometryDataItem->geometryType() == IGeometry::LineStringZ ||
      timeGeometryDataItem->geometryType() == IGeometry::LineStringZM) &&
     (provider->valueDefinition()->type() == QVariant::Double ||
      provider->valueDefinition()->type() == QVariant::Int))
  {
    return true;
  }

  message = "Provider must be a LineString";
  return false;
}

void ObjectiveInput::retrieveValuesFromProvider()
{

  if(provider()->modelComponent()->status() == HydroCouple::IModelComponent::ComponentStatus::Updated)
  {
    int numTimes = timeCount();
    double lastDateTime = time(numTimes - 1)->julianDay();

    if(m_currentDateTime != lastDateTime)
    {
      addTime(new SDKTemporal::DateTime(m_currentDateTime, nullptr));
    }

    provider()->updateValues(this);
  }
}

void ObjectiveInput::applyData()
{

  //  double currentTime = m_objectiveFunctionComponent->timeHorizon()->julianDay();
  ITimeGeometryComponentDataItem *timeGeometryDataItem = nullptr;

  if((timeGeometryDataItem = dynamic_cast<ITimeGeometryComponentDataItem*>(provider())))
  {
    int currentTimeIndex = timeGeometryDataItem->timeCount() - 1;
    int previousTimeIndex = std::max(0 , timeGeometryDataItem->timeCount() - 2);

    double providerCurrentTime = timeGeometryDataItem->time(currentTimeIndex)->julianDay();
    double providerPreviousTime = timeGeometryDataItem->time(previousTimeIndex)->julianDay();

    if(m_currentDateTime >=  providerPreviousTime && m_currentDateTime <= providerCurrentTime)
    {
      double factor = 0.0;

      if(providerCurrentTime > providerPreviousTime)
      {
        double denom = providerCurrentTime - providerPreviousTime;
        double numer = m_currentDateTime - providerPreviousTime;
        factor = numer / denom;
      }

      for(auto it : m_geometryMapping)
      {
        double value1 = 0;
        double value2 = 0;

        timeGeometryDataItem->getValue(currentTimeIndex,it.second, &value1);
        timeGeometryDataItem->getValue(previousTimeIndex,it.second, &value2);

        double interpVal = value2 + factor *(value1 - value2);
        setValue(timeCount() -1, it.first, &interpVal);
      }
    }
    else
    {
      for(auto it : m_geometryMapping)
      {
        double value = 0;
        timeGeometryDataItem->getValue(currentTimeIndex,it.second, &value);
        setValue(timeCount() -1, it.first, &value);
      }
    }
  }
}

TimeSeries *ObjectiveInput::timeSeries() const
{
  return m_timeSeries;
}

bool ObjectiveInput::equalsGeometry(IGeometry *geom1, IGeometry *geom2, double epsilon)
{
  if(geom1->geometryType() == geom2->geometryType())
  {
    switch (geom1->geometryType())
    {
      case IGeometry::LineString:
      case IGeometry::LineStringM:
      case IGeometry::LineStringZ:
      case IGeometry::LineStringZM:
        {
          ILineString *lineString1 = dynamic_cast<ILineString*>(geom1);
          ILineString *lineString2 = dynamic_cast<ILineString*>(geom2);

          if(lineString1 && lineString2 && lineString1->pointCount() == lineString2->pointCount())
          {
            for(int i = 0 ; i < lineString1->pointCount(); i++)
            {
              IPoint *p1 = lineString1->point(i);
              IPoint *p2 = lineString2->point(i);

              double dx = p1->x() - p2->x();
              double dy = p1->y() - p2->y();

              double dist = sqrt(dx * dx + dy * dy);

              if(dist < epsilon)
              {
                return true;
              }
            }
          }
        }
        break;
      default:
        {
          return geom1->equals(geom2);
        }
        break;
    }
  }

  return false;
}
