#include "stdafx.h"
#include "tsobjectivefunctioncomponentinfo.h"
#include "spatial/geometryfactory.h"
#include "tsobjectivefunctioncomponent.h"

TSObjectiveFunctionComponentInfo::TSObjectiveFunctionComponentInfo(QObject *parent)
  : AbstractModelComponentInfo(parent)
{
  GeometryFactory::registerGDAL();

  setId("Time Series Objective Function 1.0.0");
  setCaption("TS Objective Function Component");
  setIconFilePath(":/TSObjectiveFunctionComponent/tsobjectivefunctioncomponenticon");
  setDescription("A time series objective function calculator");
  setCategory("Parameter Estimation & Uncertainty");
  setCopyright("");
  setVendor("");
  setUrl("www.hydrocouple.org");
  setEmail("caleb.buahin@gmail.com");
  setVersion("1.0.0");

  QStringList documentation;
  documentation << "Several sources";
  setDocumentation(documentation);
}

TSObjectiveFunctionComponentInfo::~TSObjectiveFunctionComponentInfo()
{

}

HydroCouple::IModelComponent *TSObjectiveFunctionComponentInfo::createComponentInstance()
{
  QString id =  QUuid::createUuid().toString();
  TSObjectiveFunctionComponent *component = new TSObjectiveFunctionComponent(id, this);
  component->setDescription("Timeseries Objective Function Model Instance");
  return component;
}
