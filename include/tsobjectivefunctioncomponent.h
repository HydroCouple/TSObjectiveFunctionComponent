

#ifndef TSOBJECTIVEFUNCTIONCOMPONENT_H
#define TSOBJECTIVEFUNCTIONCOMPONENT_H

#include "tsobjectivefunctioncomponent_global.h"
#include "temporal/abstracttimemodelcomponent.h"
#include "spatial/geometry.h"
#include "timeseries.h"

#include <unordered_map>
#include <QTextStream>

class TSObjectiveFunctionComponentInfo;
class Dimension;
class ObjectiveInput;
class ObjectiveOutput;

class TSOBJECTIVEFUNCTIONCOMPONENT_EXPORT TSObjectiveFunctionComponent : public AbstractTimeModelComponent,
    public virtual HydroCouple::ICloneableModelComponent
{
    Q_OBJECT

    Q_INTERFACES(HydroCouple::ICloneableModelComponent)

  public:

    enum Algorithm
    {
      NashSutcliff,
      RMSE,
      MAE,
    };

    /*!
     * \brief TSObjectiveFunctionComponent
     * \param id
     * \param modelComponentInfo
     */
    TSObjectiveFunctionComponent(const QString &id, TSObjectiveFunctionComponentInfo *modelComponentInfo = nullptr);

    /*!
     * \brief ~TSObjectiveFunctionComponent
     */
    virtual ~TSObjectiveFunctionComponent();

    /*!
     * \brief validate
     * \return
     */
    QList<QString> validate() override;

    /*!
     * \brief prepare
     */
    void prepare() override;

    /*!
     * \brief update
     * \param requiredOutputs
     */
    void update(const QList<HydroCouple::IOutput*> &requiredOutputs = QList<HydroCouple::IOutput*>()) override;

    /*!
     * \brief finish
     */
    void finish() override;

    /*!
     * \brief parent
     * \return
     */
    HydroCouple::ICloneableModelComponent* parent() const override;

    /*!
     * \brief clone
     * \return
     */
    HydroCouple::ICloneableModelComponent* clone() override;

    /*!
     * \brief clones
     * \return
     */
    QList<HydroCouple::ICloneableModelComponent*> clones() const override;


    void applyInputValues() override;

  protected:

    /*!
     * \brief removeClone
     * \param component
     * \return
     */
    bool removeClone(TSObjectiveFunctionComponent *component);

    /*!
     * \brief initializeFailureCleanUp
     */
    void initializeFailureCleanUp() override;

  private:

    /*!
     * \brief createArguments
     */
    void createArguments() override;

    /*!
     * \brief createInputFileArguments
     */
    void createInputFileArguments();

    /*!
     * \brief initializeArguments
     * \param message
     * \return
     */
    bool initializeArguments(QString &message) override;

    /*!
     * \brief initializeInputFilesArguments
     * \param message
     * \return
     */
    bool initializeInputFilesArguments(QString &message);

    /*!
     * \brief createInputs
     */
    void createInputs() override;

    /*!
     * \brief createOutputs
     */
    void createOutputs() override;


    /*!
     * \brief getMinDate
     * \return
     */
    double getMinDate() const;

    /*!
     * \brief writeObjectiveFunctionValues
     */
    void writeOutput();

  private:

    Dimension *m_timeDimension,
              *m_geometryDimension;

    TSObjectiveFunctionComponent *m_parent;
    QList<HydroCouple::ICloneableModelComponent*> m_clones;
    IdBasedArgumentString *m_inputFilesArgument;
    static const std::unordered_map<std::string,int> m_inputFileFlags;
    static const std::unordered_map<std::string,int> m_optionsFlags;

    std::vector<std::string> m_objectiveNames;
    std::vector<Algorithm> m_algorithms;
    std::vector<TimeSeries*> m_inputTSFiles;
    std::unordered_map<std::string, QList<QSharedPointer<HCGeometry>>> m_geometries;
    std::vector<ObjectiveOutput*> m_objectiveOutputs;
    std::vector<ObjectiveInput*> m_objectiveInputs;

    QFileInfo m_outputCSVFile;
    QTextStream m_outputCSVStream;

    double m_startDate, m_endDate;
    static const QRegExp m_dateTimeDelim;
};

Q_DECLARE_METATYPE(TSObjectiveFunctionComponent*)

#endif // TSOBJECTIVEFUNCTIONCOMPONENT_H
