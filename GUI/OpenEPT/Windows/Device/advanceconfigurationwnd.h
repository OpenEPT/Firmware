#ifndef ADVANCECONFIGURATIONWND_H
#define ADVANCECONFIGURATIONWND_H

#include <QWidget>
#include <QLineEdit>
#include <QComboBox>
#include <QVBoxLayout>
#include <QHBoxLayout>
#include "Windows/Device/advcofigurationdata.h"

namespace Ui {
class AdvanceConfigurationWnd;
enum    loadProfileType_t;
}

typedef enum loadProfileType_t
{
    LOAD_PROFILE_TYPE_UNKNOW,
    LOAD_PROFILE_TYPE_STATIC,
    LOAD_PROFILE_TYPE_RAMP
}loadProfileType_t;

typedef enum{
    ADVCONFIG_ADC_MODE_INTERNAL = 0,
    ADVCONFIG_ADC_MODE_EXTERNAL = 1
}advconfig_adc_mode_t;

class AdvanceConfigurationWnd : public QWidget
{
    Q_OBJECT

public:
    explicit AdvanceConfigurationWnd(QWidget *parent = nullptr);

    void     assignAdcOptionsList(QStringList *aList);
    void     assignResolutionOptionsList(QStringList *aList);
    void     assignClockDivOptionsList(QStringList *aList);
    void     assignSampleTimeOptionsList(QStringList *aList);
    void     assignAvgRatioOptionsList(QStringList *aList);

    bool     setDeviceMode(advconfig_adc_mode_t aMode);
    bool     setClockDiv(QString aClkDiv);
    bool     setADCInClk(QString aInClk);
    bool     setResolution(QString aResolution);
    bool     setChSampleTime(QString aSTime);
    bool     setAvgRatio(QString aAvgRatio);
    bool     setVOffset(QString aAVoffset);
    bool     setCOffset(QString aCOffset);
    bool     setSamplingTime(QString sTime);


    void     SetProfileType(loadProfileType_t aProfileType);
    void     RemoveProfileType(loadProfileType_t aProfileType);


    advConfigurationData cdata;
    ~AdvanceConfigurationWnd();

signals:
    void    sigAdvConfigurationChanged(QVariant data);
    void    sigAdvConfigurationRequested();
public slots:
    void    onLoadProfileChanged(int loadIndex);
    void    onAdvResolutionChanged(int index);
    void    onAdvClkDivChanged(int index);
    void    onAdvAvrRatioChanged(int index);
    void    onAdvSampleTimeChanged(int index);
    void    onAdvSamplingTimeChanged(QString time);
    void    onAdvVOffsetChanged(QString off);
    void    onAdvCOffsetChanged(QString off);
    void    onAdvConfigurationPressed(void);
    void    onAdvConfigurationRequsted(void);
private:
    Ui::AdvanceConfigurationWnd *ui;

    void    SetProfileTypeUknown();
    void    RemoveProfileTypeUknown();
    void    SetProfileTypeStatic();
    void    RemoveProfileTypeStatic();
    void    SetProfileTypeRamp();
    void    RemoveProfileTypeRamp();
    void    LabelChangedNotUpdated();
    void    LabelChangedUpdated();
    void    SetTextRed(QComboBox* cb, QLineEdit* le1, QLineEdit* le2, QLineEdit* le3);


    QStringList*         adcOptions;
    QStringList*         resolutionOptions;
    QStringList*         sampleTimeOptions;
    QStringList*         clockDivOptions;
    QStringList*         avgRatioOptions;


    QHBoxLayout*        uknownProfileLayout;
    QWidget*            uknownProfileWidg;

    QLineEdit*          staticProfileAverageCurrentLine;
    QLineEdit*          staticProfileAverageCurrentDeviationLine;
    QLineEdit*          staticProfileAverageCurrentChangePeriodLine;
    QLineEdit*          staticProfileAverageCurrentChangePeriodDevLine;
    QLineEdit*          staticProfileDurationLine;
    QLineEdit*          staticProfileDurationDevLine;
    QVBoxLayout*        staticProfileLayout;
    QWidget*            staticProfileWidg;

    loadProfileType_t   activeLoadProfileType;


    QString             oldVOffset = "105";
    QString             oldCOffset = "105";
    QString             oldAvrratio = "2";
};

#endif // ADVANCECONFIGURATIONWND_H
