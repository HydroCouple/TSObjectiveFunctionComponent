#ifndef OBJECTIVEINPUT_H
#define OBJECTIVEINPUT_H

#include "tsobjectivefunctioncomponent_global.h"
#include "spatiotemporal/timegeometryinput.h"
#include "tsobjectivefunctioncomponent.h"

#include <unordered_map>

class TimeSeries;
class Quantity;

/*!
 * \brief The ObjectiveInput class
 * \todo Can currently only account of linestrings
 */
class TSOBJECTIVEFUNCTIONCOMPONENT_EXPORT ObjectiveInput : public TimeGeometryInputDouble
{
    Q_OBJECT

  public:

    ObjectiveInput(const QString &id,
                   TimeSeries *timeSeries,
                   Dimension *timeDimension,
                   Dimension *geometryDimension,
                   HydroCouple::Spatial::IGeometry::GeometryType geometryType,
                   Quantity *valueDefinition,
                   TSObjectiveFunctionComponent *component);

    virtual ~ObjectiveInput();

    void initialize();

    int startDateTimeIndex() const;

    int recordLength() const;

    double currentDateTime() const;

    void moveToNextDateTime();

    bool setProvider(HydroCouple::IOutput *provider) override;

    bool canConsume(HydroCouple::IOutput *provider, QString &message) const override;

    void retrieveValuesFromProvider() override;

    void applyData() override;

    TimeSeries *timeSeries() const;

  private:

    static bool equalsGeometry(HydroCouple::Spatial::IGeometry *geom1, HydroCouple::Spatial::IGeometry *geom2, double epsilon = 0.00001);

  private:

    double m_currentDateTime;
    int m_startDateTimeIndex, m_validLength, m_nextDateTimeIndex;
    std::unordered_map<int,int> m_geometryMapping;
    TimeSeries *m_timeSeries;
    TSObjectiveFunctionComponent *m_objectiveFunctionComponent;
};

#endif // OBJECTIVEINPUT_H
