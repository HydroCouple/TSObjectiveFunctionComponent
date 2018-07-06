#include "stdafx.h"
#include "tsobjectivefunctioncomponent.h"
#include "tsobjectivefunctioncomponentinfo.h"
#include "core/valuedefinition.h"
#include "core/dimension.h"
#include "core/idbasedargument.h"
#include "core/abstractoutput.h"
#include "temporal/timedata.h"
#include "progresschecker.h"
#include "spatial/envelope.h"
#include "spatial/geometryfactory.h"
#include "objectiveinput.h"
#include "objectiveoutput.h"

#include <QTextStream>

using namespace std;

TSObjectiveFunctionComponent::TSObjectiveFunctionComponent(const QString &id, TSObjectiveFunctionComponentInfo *modelComponentInfo)
  : AbstractTimeModelComponent(id, modelComponentInfo),
    m_parent(nullptr),
    m_inputFilesArgument(nullptr)
{
  m_timeDimension = new Dimension("TimeDimension",this);
  m_geometryDimension = new Dimension("ElementGeometryDimension", this);

  createArguments();
}

TSObjectiveFunctionComponent::~TSObjectiveFunctionComponent()
{
  if(m_parent)
  {
    m_parent->removeClone(this);
  }

  while (m_clones.size())
  {
    TSObjectiveFunctionComponent *clone =  dynamic_cast<TSObjectiveFunctionComponent*>(m_clones.first());
    removeClone(clone);
    delete clone;
  }
}

QList<QString> TSObjectiveFunctionComponent::validate()
{
  return QList<QString>();
}

void TSObjectiveFunctionComponent::prepare()
{
  if(!isPrepared() && isInitialized())
  {
    for(auto output :  outputsInternal())
    {
      for(auto adaptedOutput : output->adaptedOutputs())
      {
        adaptedOutput->initialize();
      }
    }

    progressChecker()->reset(m_startDate, m_endDate);



    updateOutputValues(QList<HydroCouple::IOutput*>());

    setStatus(IModelComponent::Updated ,"Finished preparing model");
    setPrepared(true);
  }
  else
  {
    setPrepared(false);
    setStatus(IModelComponent::Failed ,"Error occured when preparing model");
  }
}

void TSObjectiveFunctionComponent::update(const QList<HydroCouple::IOutput *> &requiredOutputs)
{
  if(status() == IModelComponent::Updated)
  {
    setStatus(IModelComponent::Updating);

    applyInputValues();

    double minDate = getMinDate();

    updateOutputValues(requiredOutputs);

    currentDateTimeInternal()->setJulianDay(minDate);

    if(minDate >=  m_endDate)
    {
      writeOutput();
      setStatus(IModelComponent::Done , "Simulation finished successfully", 100);
    }
    else
    {
      for(ObjectiveInput *input : m_objectiveInputs)
      {
        input->moveToNextDateTime();
      }

      if(progressChecker()->performStep(minDate))
      {
        setStatus(IModelComponent::Updated , "Simulation performed time-step | DateTime: " + QString::number(minDate) , progressChecker()->progress());
      }
      else
      {
        setStatus(IModelComponent::Updated);
      }
    }
  }
}

void TSObjectiveFunctionComponent::finish()
{
  if(isPrepared())
  {
    setStatus(IModelComponent::Finishing , "TSObjectiveFunctionComponent with id " + id() + " is being disposed" , 100);

    initializeFailureCleanUp();

    setPrepared(false);
    setInitialized(false);

    setStatus(IModelComponent::Finished , "TSObjectiveFunctionComponent with id " + id() + " has been disposed" , 100);
    setStatus(IModelComponent::Created , "TSObjectiveFunctionComponent with id " + id() + " ran successfully and has been re-created" , 100);
  }
}

HydroCouple::ICloneableModelComponent *TSObjectiveFunctionComponent::parent() const
{
  return m_parent;
}

HydroCouple::ICloneableModelComponent *TSObjectiveFunctionComponent::clone()
{
  if(isInitialized())
  {
    TSObjectiveFunctionComponent *cloneComponent = dynamic_cast<TSObjectiveFunctionComponent*>(componentInfo()->createComponentInstance());
    cloneComponent->setReferenceDirectory(referenceDirectory());

    IdBasedArgumentString *identifierArg = identifierArgument();
    IdBasedArgumentString *cloneIndentifierArg = cloneComponent->identifierArgument();

    (*cloneIndentifierArg)["Id"] = QString((*identifierArg)["Id"]);
    (*cloneIndentifierArg)["Caption"] = QString((*identifierArg)["Caption"]);
    (*cloneIndentifierArg)["Description"] = QString((*identifierArg)["Description"]);

    QString appendName = "_clone_" + QString::number(m_clones.size()) + "_" + QUuid::createUuid().toString().replace("{","").replace("}","");

    //(*cloneComponent->m_inputFilesArgument)["Input File"] = QString((*m_inputFilesArgument)["Input File"]);

    QString inputFilePath = QString((*m_inputFilesArgument)["Input File"]);
    QFileInfo inputFile = getAbsoluteFilePath(inputFilePath);

    if(inputFile.absoluteDir().exists())
    {
      QString suffix = "." + inputFile.completeSuffix();
      inputFilePath = inputFile.absoluteFilePath().replace(suffix,"") + appendName + suffix;
      QFile::copy(inputFile.absoluteFilePath(), inputFilePath);
      (*cloneComponent->m_inputFilesArgument)["Input File"] = inputFilePath;
    }

    QString  outputCSVFilePath = QString((*m_inputFilesArgument)["Output CSV File"]);
    QFileInfo outputCSVFile = getAbsoluteFilePath(outputCSVFilePath);

    if(!outputCSVFilePath.isEmpty() && outputCSVFile.absoluteDir().exists())
    {
      QString suffix = "." + outputCSVFile.completeSuffix();
      outputCSVFilePath = outputCSVFile.absoluteFilePath().replace(suffix,"") + appendName + suffix;
      (*cloneComponent->m_inputFilesArgument)["Output CSV File"] = outputCSVFilePath;
    }


    cloneComponent->m_parent = this;
    m_clones.append(cloneComponent);

    emit propertyChanged("Clones");

    cloneComponent->initialize();

    return cloneComponent;
  }

  return nullptr;
}

QList<HydroCouple::ICloneableModelComponent*> TSObjectiveFunctionComponent::clones() const
{
  return m_clones;
}

bool TSObjectiveFunctionComponent::removeClone(TSObjectiveFunctionComponent *component)
{
  int removed;

#ifdef USE_OPENMP
#pragma omp critical
#endif
  {
    removed = m_clones.removeAll(component);
  }


  if(removed)
  {
    component->m_parent = nullptr;
    emit propertyChanged("Clones");
  }

  return removed;
}

void TSObjectiveFunctionComponent::initializeFailureCleanUp()
{
  m_objectiveNames.clear();
  m_algorithms.clear();

  for(TimeSeries *ts : m_inputTSFiles)
    delete ts;

  m_inputTSFiles.clear();

  m_objectiveOutputs.clear();
  m_objectiveInputs.clear();

  if (m_outputCSVStream.device() && m_outputCSVStream.device()->isOpen())
  {
    m_outputCSVStream.flush();
    m_outputCSVStream.device()->close();
    delete m_outputCSVStream.device();
    m_outputCSVStream.setDevice(nullptr);
  }

  m_geometries.clear();
}

void TSObjectiveFunctionComponent::createArguments()
{
  createInputFileArguments();
}

void TSObjectiveFunctionComponent::createInputFileArguments()
{
  QStringList fidentifiers;
  fidentifiers.append("Input File");
  fidentifiers.append("Output CSV File");

  Quantity *fquantity = Quantity::unitLessValues("InputFilesQuantity", QVariant::String, this);
  fquantity->setDefaultValue("");
  fquantity->setMissingValue("");

  Dimension *dimension = new Dimension("IdDimension","Dimension for identifiers",this);

  m_inputFilesArgument = new IdBasedArgumentString("InputFiles", fidentifiers, dimension, fquantity, this);
  m_inputFilesArgument->setCaption("Model Input Files");
  m_inputFilesArgument->addFileFilter("Input File (*.inp)");
  m_inputFilesArgument->setMatchIdentifiersWhenReading(true);

  addArgument(m_inputFilesArgument);
}

bool TSObjectiveFunctionComponent::initializeArguments(QString &message)
{
  message = "";

  initializeFailureCleanUp();

  bool initialized = initializeInputFilesArguments(message);

  return initialized;
}

bool TSObjectiveFunctionComponent::initializeInputFilesArguments(QString &message)
{
  message = "";

  QString inputFilePath = (*m_inputFilesArgument)["Input File"];
  QFileInfo inputFile = getAbsoluteFilePath(inputFilePath);

  m_objectiveNames.clear();
  m_algorithms.clear();

  for(TimeSeries *ts : m_inputTSFiles)
    delete ts;

  m_inputTSFiles.clear();

  if(inputFile.isFile() && inputFile.exists())
  {
    QFile file(inputFile.absoluteFilePath());

    if(file.open(QIODevice::ReadOnly))
    {
      QTextStream streamReader(&file);
      int currentFlag = -1;
      QRegExp delimiters = QRegExp("(\\,|\\t|\\;|\\s+)");

      int lineCount = 0;

      while (!streamReader.atEnd())
      {
        QString line = streamReader.readLine().trimmed();
        lineCount++;

        if (!line.isEmpty() && !line.isNull())
        {
          bool readSuccess = true;
          QString error = "";

          auto it = m_inputFileFlags.find(line.toStdString());

          if (it != m_inputFileFlags.cend())
          {
            currentFlag = it->second;
          }
          else if (!QStringRef::compare(QStringRef(&line, 0, 2), ";;"))
          {
            //commment do nothing
          }
          else
          {
            switch (currentFlag)
            {
              case 1:
                {
                  QStringList cols = line.split(delimiters, QString::SkipEmptyParts);

                  if(cols.size() == 3)
                  {
                    QDateTime dateTime;

                    if(cols[0] == "START_DATETIME" &&
                       SDKTemporal::DateTime::tryParse(cols[1] + " " + cols[2], dateTime))
                    {
                      m_startDate = SDKTemporal::DateTime::toJulianDays(dateTime);
                    }
                    else if( cols[0] == "END_DATETIME" &&
                             SDKTemporal::DateTime::tryParse(cols[1] + " " + cols[2], dateTime))
                    {
                      m_endDate = SDKTemporal::DateTime::toJulianDays(dateTime);
                    }
                    else
                    {
                      message = "Error reading date time";
                      return false;
                    }
                  }
                }
                break;
              case 2:
                {
                  QStringList cols = line.split(delimiters, QString::SkipEmptyParts);

                  if(cols.size() == 3)
                  {
                    QFileInfo tsFile = getAbsoluteFilePath(cols[2]);

                    if(tsFile.exists())
                    {
                      TimeSeries *timeSeriesObj = nullptr;

                      if((timeSeriesObj = TimeSeries::createTimeSeries(cols[0],tsFile, this)))
                      {
                        QString alg = cols[1];

                        Algorithm algorithm;

                        if(!alg.compare("NASH_SUTCLIFF", Qt::CaseInsensitive))
                        {
                          algorithm = Algorithm::NashSutcliff;
                        }
                        else if(!alg.compare("RMSE", Qt::CaseInsensitive))
                        {
                          algorithm = Algorithm::RMSE;
                        }
                        else if(!alg.compare("MAE", Qt::CaseInsensitive))
                        {
                          algorithm = Algorithm::MAE;
                        }
                        else
                        {
                          message = "Wrong algorithm specification";
                          return false;
                        }

                        m_objectiveNames.push_back(cols[0].toStdString());
                        m_algorithms.push_back(algorithm);
                        m_inputTSFiles.push_back(timeSeriesObj);
                      }
                      else
                      {
                        message = "Unable to read ts file";
                        return false;
                      }
                    }
                    else
                    {
                      message = "Time series file does not exist";
                      return false;
                    }
                  }
                }
                break;
              case 3:
                {
                  QStringList cols = line.split(delimiters,QString::SkipEmptyParts);

                  if(cols.size() == 3)
                  {
                    QString name = cols[0];
                    QString gtype = cols[1];
                    QString gsource = cols[2];
                    QList<HCGeometry*> geometries;

                    if(!gtype.compare("SHAPEFILE", Qt::CaseInsensitive))
                    {
                      QFileInfo path = getAbsoluteFilePath(gsource);
                      Envelope envp;

                      if(!GeometryFactory::readGeometryFromFile(path.absoluteFilePath(), geometries, envp, error))
                      {
                        return false;
                      }
                    }
                    else if (!gtype.compare("WKT", Qt::CaseInsensitive))
                    {
                      HCGeometry *geometry = GeometryFactory::importFromWkt(gsource);

                      if(!geometry)
                      {
                        return false;
                      }

                      geometries.push_back(geometry);
                    }

                    QList<QSharedPointer<HCGeometry>> sharedGeoms;

                    for(HCGeometry *geometry : geometries)
                    {
                      sharedGeoms.push_back(QSharedPointer<HCGeometry>(geometry));
                    }

                    m_geometries[name.toStdString()] = sharedGeoms;

                  }
                  else
                  {
                    error = "Expected 3 arguments";
                    return false;
                  }

                }
                break;
            }
          }

          if (!readSuccess)
          {
            message = "Line " + QString::number(lineCount) + " : " + error;
            file.close();
            return false;
            break;
          }
        }
      }

      file.close();
    }
  }
  else
  {
    message = "Input file does not exist: " + inputFile.absoluteFilePath();
    return false;
  }

  QString outputCSVFile = (*m_inputFilesArgument)["Output CSV File"];
  m_outputCSVFile = getAbsoluteFilePath(outputCSVFile);
  outputCSVFile = m_outputCSVFile.absoluteFilePath();

  if (!outputCSVFile.isEmpty() && !outputCSVFile.isNull() && !m_outputCSVFile.absoluteDir().exists())
  {
    message = "Output shapefile directory does not exist: " + outputCSVFile;
    return false;
  }

  if (!outputCSVFile.isEmpty() && !outputCSVFile.isNull())
  {
    if (m_outputCSVStream.device() == nullptr)
    {
      QFile *device = new QFile(outputCSVFile, this);
      m_outputCSVStream.setDevice(device);
    }

    if (m_outputCSVStream.device()->open(QIODevice::WriteOnly | QIODevice::Truncate))
    {
      m_outputCSVStream.setRealNumberPrecision(10);
      m_outputCSVStream.setRealNumberNotation(QTextStream::SmartNotation);
    }
  }

  if(m_startDate > m_endDate)
    return false;

  timeHorizonInternal()->setJulianDay(m_startDate);
  timeHorizonInternal()->setDuration(m_endDate - m_startDate);

  return true;
}

void TSObjectiveFunctionComponent::createInputs()
{
  for(size_t i = 0; i < m_objectiveNames.size() ; i++)
  {
    QString name = QString::fromStdString(m_objectiveNames[i]);
    QList<QSharedPointer<HCGeometry>> geometries = m_geometries[name.toStdString()];
    Quantity *quantity = Quantity::unitLessValues("Unitless", QVariant::Double, this);

    ObjectiveInput *objectiveInput = new ObjectiveInput(name, m_inputTSFiles[i], m_timeDimension, m_geometryDimension, geometries[0]->geometryType(), quantity, this);
    objectiveInput->addGeometries(geometries);
    objectiveInput->setCaption(name);
    objectiveInput->initialize();

    m_objectiveInputs.push_back(objectiveInput);
    addInput(objectiveInput);
  }
}

void TSObjectiveFunctionComponent::createOutputs()
{
  for(size_t i = 0; i < m_objectiveInputs.size(); i++)
  {
    ObjectiveInput *objectiveInput = m_objectiveInputs[i];

    QString name = QString::fromStdString(m_objectiveNames[i]);
    QList<QSharedPointer<HCGeometry>> geometries = m_geometries[name.toStdString()];

    ObjectiveOutput *objectiveOutput = new ObjectiveOutput(objectiveInput->id(), m_algorithms[i], objectiveInput, this);
    objectiveOutput->addGeometries(geometries);
    objectiveOutput->setCaption(name);

    m_objectiveOutputs.push_back(objectiveOutput);
    addOutput(objectiveOutput);
  }
}

double TSObjectiveFunctionComponent::getMinDate() const
{
  double minDate = std::numeric_limits<double>::max();

  for(size_t i = 0; i < m_objectiveInputs.size(); i++)
  {
    ObjectiveInput *objectiveInput = m_objectiveInputs[i];
    minDate = std::min(minDate, objectiveInput->currentDateTime());
  }

  return minDate;
}

void TSObjectiveFunctionComponent::writeOutput()
{
  if (m_outputCSVStream.device() && m_outputCSVStream.device()->isOpen())
  {
    if(m_objectiveInputs.size())
    {
      ObjectiveInput *objectiveInput = m_objectiveInputs[0];

      m_outputCSVStream << objectiveInput->id();

      for(int j = 1; j < objectiveInput->geometryCount() ; j++)
      {
        m_outputCSVStream <<  ", " << objectiveInput->id();
      }

      for(size_t i = 1; i < m_objectiveInputs.size(); i++)
      {
        objectiveInput = m_objectiveInputs[i];

        for(int j = 0; j < objectiveInput->geometryCount() ; j++)
        {
          m_outputCSVStream <<  ", " << objectiveInput->id();
        }
      }

      m_outputCSVStream << endl;

      ObjectiveOutput *objectiveOutput = m_objectiveOutputs[0];
      double value = 0;

      objectiveOutput->getValue(0,&value);
      m_outputCSVStream << value;

      for(int j = 1; j < objectiveOutput->geometryCount() ; j++)
      {
        objectiveOutput->getValue(j,&value);
        m_outputCSVStream <<  ", " << value;
      }

      for(int i = 1; i < m_objectiveOutputs.size(); i++)
      {
        objectiveOutput = m_objectiveOutputs[i];

        for(int j = 0; j < objectiveOutput->geometryCount() ; j++)
        {
          objectiveOutput->getValue(j,&value);
          m_outputCSVStream <<  ", " << value;
        }
      }

      m_outputCSVStream << endl;
      m_outputCSVStream.flush();
    }
  }
}

const unordered_map<string, int> TSObjectiveFunctionComponent::m_inputFileFlags({
                                                                                  {"[OPTIONS]", 1},
                                                                                  {"[OBJECTIVES]", 2},
                                                                                  {"[OBJECTIVE_GEOMETRIES]", 3},
                                                                                });

const unordered_map<string, int> TSObjectiveFunctionComponent::m_optionsFlags({
                                                                                {"START_DATETIME", 1},
                                                                                {"END_DATETIME", 2},
                                                                              });

const QRegExp TSObjectiveFunctionComponent::m_dateTimeDelim("(\\,|\\t|\\\n|\\/|\\s+|\\:)");
