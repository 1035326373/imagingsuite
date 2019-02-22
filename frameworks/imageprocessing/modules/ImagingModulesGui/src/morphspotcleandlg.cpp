//<LICENSE>

#include "morphspotcleandlg.h"
#include "ui_morphspotcleandlg.h"

#undef None
#define None 0L


#include <QDebug>
#include <QSignalBlocker>
#include <QFileDialog>

#include <strings/miscstring.h>
#include <base/thistogram.h>
#include <math/sums.h>
#include <math/mathfunctions.h>
#include <base/KiplException.h>

#include <ParameterHandling.h>
#include <ModuleException.h>

MorphSpotCleanDlg::MorphSpotCleanDlg(QWidget *parent) :
    ConfiguratorDialogBase("MorphSpotCleanDlg",true,true,true,parent),
    ui(new Ui::MorphSpotCleanDlg),
    m_eConnectivity(kipl::morphology::conn4),
    m_eDetectionMethod(ImagingAlgorithms::MorphDetectBoth),
    m_eCleanMethod(ImagingAlgorithms::MorphCleanReplace),
    m_fThreshold(0.05f),
    m_fSigma(m_fThreshold/10.0f),
    m_nEdgeSmoothLength(7),
    m_nMaxArea(0),
    m_bUseClamping(false),
    m_fMinLevel(-0.1f),
    m_fMaxLevel(12.0f),
    m_bThreading(false)
{
    ui->setupUi(this);

    // here I hide stuff that is not used
    ui->comboCleanMethod->hide();
    ui->comboConnectivity->hide();
    ui->label_13->hide();
    ui->label_6->hide();
    ui->label_7->hide();
    ui->spinArea->hide();

}

MorphSpotCleanDlg::~MorphSpotCleanDlg()
{
    delete ui;
}

void MorphSpotCleanDlg::ApplyParameters()
{
    //kipl::base::TImage<float,2> img(m_Projections.Dims());
//    m_OriginalImage=GetProjection(m_Projections,m_Projections.Size(2)/2);

    // inizio a prendere la prima slide
//    m_OriginalImage=kipl::base::ExtractSlice(m_Projections,0,kipl::base::ImagePlaneXY,nullptr);
    // I take the first image, for my first check

    m_OriginalImage = m_Projections;

    const size_t N=512;
    size_t hist[N];
    float axis[N];
    size_t nLo, nHi;

//    kipl::base::Histogram(m_OriginalImage.GetDataPtr(), m_OriginalImage.Size(), hist, N, 0.0f, 0.0f, axis);
//    kipl::base::FindLimits(hist, N, 97.5f, &nLo, &nHi);
//    ui->viewerOrignal->set_image(m_OriginalImage.GetDataPtr(),m_OriginalImage.Dims(),axis[nLo],axis[nHi]);

    std::map<std::string, std::string> parameters;
    UpdateParameters();
    UpdateParameterList(parameters);

    m_Cleaner.Configure(*m_Config, parameters);


//    m_DetectionImage=m_Cleaner.DetectionImage(m_OriginalImage);

    m_DetectionImage = kipl::base::ExtractSlice(m_OriginalImage,0,kipl::base::ImagePlaneXY,nullptr); // I start by taking the first image
    m_DetectionImage=m_Cleaner.DetectionImage(m_DetectionImage);

    for (size_t i=0; i<m_DetectionImage.Size(); i++)
        if (m_DetectionImage[i]!=m_DetectionImage[i]) m_DetectionImage[i]=0;

    size_t cumhist[N];
    memset(hist,0,N*sizeof(size_t));
    memset(axis,0,N*sizeof(float));

    kipl::base::Histogram(m_DetectionImage.GetDataPtr(), m_DetectionImage.Size(), hist, N, 0.0f, 0.0f, axis);

    kipl::math::cumsum(hist,cumhist,N);

    float fcumhist[N];
    size_t ii=0;
    do
    {
        for (ii=0;ii<N;++ii) {
            fcumhist[ii]=static_cast<float>(cumhist[ii])/static_cast<float>(cumhist[N-1]);
            if (0.99f<fcumhist[ii])
                break;
        }
        kipl::base::Histogram(m_DetectionImage.GetDataPtr(), m_DetectionImage.Size(), hist, N, axis[0], axis[ii], axis);
            kipl::math::cumsum(hist,cumhist,N);
    }
    while (ii<50);

    size_t N99=ii;
    ui->plotDetection->setCurveData(0,axis,fcumhist,N99, QString("Cumulative histogram"));
    float threshold[N];

    float thaxis[N];
    if (m_fSigma!=0.0f)
    { // In case of sigmoid mixing

        for (size_t i=0; i<N; i++) {
            thaxis[i]=axis[0]+i*(axis[N99]-axis[0])/N;
            threshold[i]=kipl::math::Sigmoid(thaxis[i], m_fThreshold, m_fSigma);
        }
        ui->plotDetection->setCurveData(1,thaxis,threshold,N,"Threshold");
    }
    else
    {
//            ui->plotDetection->setPlotCursor(0,QtAddons::PlotCursor(m_fThreshold,Qt::red,QtAddons::PlotCursor::Vertical));
    }

    std::map<std::string,std::string> pars;
    size_t dims[3]={m_OriginalImage.Size(0),m_OriginalImage.Size(1),1};
    m_ProcessedImage.Resize(dims);
    std::copy_n(m_OriginalImage.GetDataPtr(),m_ProcessedImage.Size(),m_ProcessedImage.GetDataPtr());

    m_Cleaner.Process(m_ProcessedImage, pars);

    std::ostringstream msg;
    msg.str(""); msg<<"Processed image"<<m_ProcessedImage;
    logger(kipl::logging::Logger::LogMessage,msg.str());

    memset(hist,0,N*sizeof(size_t));
    memset(axis,0,N*sizeof(float));
    kipl::base::Histogram(m_ProcessedImage.GetDataPtr(), m_ProcessedImage.Size(), hist, N, 0.0f, 0.0f, axis);
    kipl::base::FindLimits(hist, N, 97.5, &nLo, &nHi);
    ui->viewer_original->set_image(m_OriginalImage.GetDataPtr(),m_OriginalImage.Dims());
    ui->viewer_processed->set_image(m_ProcessedImage.GetDataPtr(),m_ProcessedImage.Dims());

    kipl::base::TImage<float,2> diffimg(m_ProcessedImage.Dims());
    for (size_t i=0; i<m_ProcessedImage.Size(); ++i)
        diffimg[i]=m_OriginalImage[i]-m_ProcessedImage[i];

    kipl::base::Histogram(diffimg.GetDataPtr(), diffimg.Size(), hist, N, 0.0f, 0.0f, axis);
    kipl::base::FindLimits(hist, N, 99.0f, &nLo, &nHi);

    ui->viewer_difference->set_image(diffimg.GetDataPtr(),diffimg.Dims(),axis[nLo],axis[nHi]);
}

int MorphSpotCleanDlg::exec(ConfigBase *config, std::map<std::string, std::string> &parameters, kipl::base::TImage<float,3> &img)
{
    std::ostringstream msg;

    m_Projections=img;

    ui->viewer_original->set_image(m_Projections.GetDataPtr(),m_Projections.Dims());

    m_Config=dynamic_cast<KiplProcessConfig *>(config);

    msg.str("");
    try {
        string2enum(GetStringParameter(parameters,"cleanmethod"), m_eCleanMethod);
        string2enum(GetStringParameter(parameters,"detectionmethod"), m_eDetectionMethod);
        string2enum(GetStringParameter(parameters,"connectivity"), m_eConnectivity);

        m_fThreshold        = GetFloatParameter(parameters,"threshold");
        m_fSigma            = GetFloatParameter(parameters,"sigma");
        m_nEdgeSmoothLength = GetIntParameter(parameters,"edgesmooth");
        m_nMaxArea          = GetIntParameter(parameters,"maxarea");
        m_fMinLevel         = GetFloatParameter(parameters,"minlevel");
        m_fMaxLevel         = GetFloatParameter(parameters,"maxlevel");
        m_bThreading        = kipl::strings::string2bool(GetStringParameter(parameters,"threading"));
    }
    catch (ModuleException &e) {
        msg<<"Module exception: Failed to get parameters: "<<e.what();
        logger(kipl::logging::Logger::LogError,msg.str());
        return -1;
    }
    catch (kipl::base::KiplException &e) {
        msg<<"Kipl Exception: Failed to get parameters: "<<e.what();
        logger(kipl::logging::Logger::LogError,msg.str());
        return -1;
    }
    catch (std::exception &e) {
        msg<<"STL exception: Failed to get parameters: "<<e.what();
        logger(kipl::logging::Logger::LogError,msg.str());
        return -1;
    }


    UpdateDialog();
//    ApplyParameters(); // I don't want to apply the parameters when I run the config

    int res=QDialog::exec();

    if (res==QDialog::Accepted) {
        logger(kipl::logging::Logger::LogMessage,"Use settings");
        UpdateParameters();
        UpdateParameterList(parameters);
        return true;
    }
    else {
        logger(kipl::logging::Logger::LogMessage,"Discard settings");
    }

    return false;
}

void MorphSpotCleanDlg::UpdateDialog()
{
    ui->spinThreshold->setValue(m_fThreshold);
    ui->spinSigma->setValue(m_fSigma);
    ui->comboCleanMethod->setCurrentIndex(m_eCleanMethod);
    ui->comboDetectionMethod->setCurrentIndex(m_eDetectionMethod);
    ui->comboConnectivity->setCurrentIndex(m_eConnectivity);
    ui->spinArea->setValue(m_nMaxArea);
    ui->groupClampData->setChecked(m_bUseClamping);
    ui->spinMinValue->setValue(m_fMinLevel);
    ui->spinMaxValue->setValue(m_fMaxLevel);
    ui->spinEdgeLenght->setValue(m_nEdgeSmoothLength);
}

void MorphSpotCleanDlg::UpdateParameters()
{
    m_eCleanMethod      = static_cast<ImagingAlgorithms::eMorphCleanMethod>(ui->comboCleanMethod->currentIndex());
    m_eDetectionMethod  = static_cast<ImagingAlgorithms::eMorphDetectionMethod>(ui->comboDetectionMethod->currentIndex());
    m_eConnectivity     = static_cast<kipl::morphology::MorphConnect>(ui->comboConnectivity->currentIndex());
    m_fThreshold        = ui->spinThreshold->value();
    m_fSigma            = ui->spinSigma->value();
    m_fMinLevel         = ui->spinMinValue->value();
    m_bUseClamping      = ui->groupClampData->isChecked();
    m_fMaxLevel         = ui->spinMaxValue->value();
    m_nMaxArea          = ui->spinArea->value();
    m_nEdgeSmoothLength = ui->spinEdgeLenght->value();
}

void MorphSpotCleanDlg::UpdateParameterList(std::map<std::string, std::string> &parameters)
{    
    parameters["connectivity"]    = enum2string(m_eConnectivity);
    parameters["cleanmethod"]     = enum2string(m_eCleanMethod);
    parameters["detectionmethod"] = enum2string(m_eDetectionMethod);
    parameters["threshold"]       = kipl::strings::value2string(m_fThreshold);
    parameters["sigma"]           = kipl::strings::value2string(m_fSigma);
    parameters["edgesmooth"]      = kipl::strings::value2string(m_nEdgeSmoothLength);
    parameters["maxarea"]         = kipl::strings::value2string(m_nMaxArea);
    parameters["useclamping"]     = kipl::strings::bool2string(m_bUseClamping);
    parameters["minlevel"]        = kipl::strings::value2string(m_fMinLevel);
    parameters["maxlevel"]        = kipl::strings::value2string(m_fMaxLevel);
    parameters["threading"]       = kipl::strings::bool2string(m_bThreading);
}


void MorphSpotCleanDlg::on_buttonApply_clicked()
{
    ApplyParameters();
}


// I don't think I need this now
void MorphSpotCleanDlg::on_comboDetectionDisplay_currentIndexChanged(int index)
{
//    std::ostringstream msg;

//    msg<<"Change display index to "<<index;
//    logger(kipl::logging::Logger::LogMessage,msg.str());

//    if (m_OriginalImage.Size()==0) {
//        msg.str(""); msg<<"Original image void";
//        logger(kipl::logging::Logger::LogMessage,msg.str());

//        return;
//    }
//    if (m_OriginalImage.Size()!=m_ProcessedImage.Size()) {
//        msg.str(""); msg<<"Original image: "<<m_OriginalImage<<std::endl<<"Processed image: "<<m_ProcessedImage;
//        logger(kipl::logging::Logger::LogMessage,msg.str());

//        return;
//    }

//    kipl::base::TImage<float,3> dimg=m_ProcessedImage;

//    logger(kipl::logging::Logger::LogMessage,"Start switching");
//    switch (index) {
//    case 0: // Difference
//        dimg=m_OriginalImage-m_ProcessedImage;
//        break;
//    case 1:
//        for (size_t i=0; i<dimg.Size(); i++) {
//            if (m_OriginalImage[i]!=0.0f)
//                dimg[i] = 100*(m_OriginalImage[i]-m_ProcessedImage[i])/m_OriginalImage[i];
//            else
//                dimg[i]=0.0f;
//        }
//    case 2: // Detection image
//        dimg=m_DetectionImage;
//        break;
//    case 3: // Hit map
//        dimg=m_OriginalImage-m_ProcessedImage;
//        for (size_t i=0; i<dimg.Size(); i++)
//            dimg[i] = dimg[i]!=0 ? 1.0f : 0.0f;
//        break;
//    }
//    size_t nLo,nHi;

//    const size_t N=512;
//    float axis[N];
//    size_t hist[N];

//    memset(hist,0,N*sizeof(size_t));
//    memset(axis,0,N*sizeof(float));

//    kipl::base::Histogram(dimg.GetDataPtr(), dimg.Size(), hist, N, 0.0f, 0.0f, axis);
//    kipl::base::FindLimits(hist, N, 97.5f, &nLo, &nHi);
//    logger(kipl::logging::Logger::LogMessage,"Start display");
//    ui->viewerDifference->set_image(dimg.GetDataPtr(),dimg.Dims(),axis[nLo],axis[nHi]);
}