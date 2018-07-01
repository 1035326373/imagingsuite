//<LICENSE>

#include <set>
#include <sstream>
#include <string>
#include <iomanip>

#include <QFileDialog>
#include <QMessageBox>
#include <QDir>
#include <QDate>
#include <QFile>
#include <QDesktopServices>
#include <QUrl>
#include <QSignalBlocker>
#include <QDebug>
#include <QVersionNumber>

#include <base/timage.h>
#include <base/KiplException.h>
#include <math/mathfunctions.h>
#include <base/thistogram.h>
#include <strings/string2array.h>
#include <strings/filenames.h>
#include <io/DirAnalyzer.h>
#include <generators/Sine2D.h>

#include <BackProjectorModuleBase.h>
#include <ReconHelpers.h>
#include <ReconException.h>
#include <ProjectionReader.h>

#include <ModuleException.h>
#include <ParameterHandling.h>

#include "muhrecmainwindow.h"
#include "ui_muhrecmainwindow.h"
#include "configuregeometrydialog.h"
#include "findskiplistdialog.h"
#include "recondialog.h"
#include "viewgeometrylistdialog.h"
#include "preferencesdialog.h"
#include "dialogtoobig.h"
#include "piercingpointdialog.h"
#include "referencefiledlg.h"


MuhRecMainWindow::MuhRecMainWindow(QApplication *app, QWidget *parent) :
    QMainWindow(parent),
    logger("MuhRec3MainWindow"),
    ui(new Ui::MuhRecMainWindow),
    logdlg(new QtAddons::LoggingDialog(this)),
    m_QtApp(app),
    m_ModuleConfigurator(&m_Config),
    m_pEngine(nullptr),
    m_nCurrentPage(0),
    m_nRequiredMemory(0),
    m_sApplicationPath(app->applicationDirPath().toStdString()),
    m_sHomePath(QDir::homePath().toStdString()),
    m_sConfigFilename("noname.xml"),
    m_bCurrentReconStored(true),
    m_oldRotateDial(0),
    m_oldRotateSpin(0.0)
{
    std::ostringstream msg;
    ui->setupUi(this);


    // Setup logging dialog
    logdlg->setModal(false);
    kipl::logging::Logger::AddLogTarget(*logdlg);

    logger(kipl::logging::Logger::LogMessage,"Enter c'tor");

    // Prepare paths
    kipl::strings::filenames::CheckPathSlashes(m_sApplicationPath,true);
    kipl::strings::filenames::CheckPathSlashes(m_sHomePath,true);

    m_sConfigPath = m_sHomePath + ".imagingtools";
    kipl::strings::filenames::CheckPathSlashes(m_sConfigPath,true);

    QDir dir;

    if (!dir.exists(QString::fromStdString(m_sConfigPath))) {
        dir.mkdir(QString::fromStdString(m_sConfigPath));
    }

    msg.str("");
    msg<<"ApplicationPath = "<<m_sApplicationPath<<std::endl
      <<"HomePath         = "<<m_sHomePath<<std::endl
      <<"ConfigPath       = "<<m_sConfigPath<<std::endl;
    logger(kipl::logging::Logger::LogMessage,msg.str());


    ui->projectionViewer->hold_annotations(true);

    // Setup default module libs in the config
    std::string defaultmodules;
#ifdef Q_OS_WIN
        defaultmodules=m_sApplicationPath+"\\StdBackProjectors.dll";
#else
    #ifdef Q_OS_MAC
        defaultmodules = m_sApplicationPath+"../Frameworks/libStdBackProjectors.dylib";
    #else
        defaultmodules = m_sApplicationPath+"../Frameworks/libStdBackProjectors.so";
    #endif
#endif
    ui->ConfiguratorBackProj->Configure("muhrecbp",defaultmodules,m_sApplicationPath);

#ifdef Q_OS_WIN
        defaultmodules=m_sApplicationPath+"\\StdPreprocModules.dll";
#else
    #ifdef Q_OS_MAC
        defaultmodules = m_sApplicationPath+"../Frameworks/libStdPreprocModules.dylib";
    #else
        defaultmodules = m_sApplicationPath+"../Frameworks/libStdPreprocModules.so";
    #endif
#endif

    ui->moduleconfigurator->configure("muhrec",m_sApplicationPath,&m_ModuleConfigurator);
    ui->moduleconfigurator->SetDefaultModuleSource(defaultmodules);
    ui->moduleconfigurator->SetApplicationObject(this);



    m_oldROI[0]=0; m_oldROI[1]=0; m_oldROI[2]=1; m_oldROI[3]=1;

    LoadDefaults(true);
    UpdateDialog();


    ProjectionIndexChanged(0);
    SlicesChanged(0);
    SetupCallBacks();
    ui->widgetProjectionROI->updateViewer();

//    if (ui->checkCBCT->isChecked()) SlicesCBCTChanged(0);

}

MuhRecMainWindow::~MuhRecMainWindow()
{
    delete ui;
}


void MuhRecMainWindow::SetupCallBacks()
{
    // Menus
    connect(ui->actionNew,SIGNAL(triggered()),this,SLOT(MenuFileNew()));
    connect(ui->actionOpen,SIGNAL(triggered()),this,SLOT(MenuFileOpen()));
    connect(ui->actionSave,SIGNAL(triggered()),this,SLOT(MenuFileSave()));
    connect(ui->actionSave_As,SIGNAL(triggered()),this,SLOT(MenuFileSaveAs()));
    connect(ui->actionQuit,SIGNAL(triggered()),this,SLOT(MenuFileQuit()));
    connect(ui->actionStartReconstruction,SIGNAL(triggered()),this,SLOT(MenuReconstructStart()));
    connect(ui->actionAbout,SIGNAL(triggered()),this,SLOT(MenuHelpAbout()));

    connect(ui->actionStore_geometry,SIGNAL(triggered()),this,SLOT(StoreGeometrySetting()));
    connect(ui->actionView_geometry_list,SIGNAL(triggered()),this,SLOT(ViewGeometryList()));
    connect(ui->actionClear_list,SIGNAL(triggered()),this,SLOT(ClearGeometrySettings()));

    connect(ui->sliceViewer,&QtAddons::ImageViewerWidget::levelsChanged,this,&MuhRecMainWindow::on_sliceViewer_levelsChanged);

    ui->widgetDoseROI->registerViewer(ui->projectionViewer);
    ui->widgetDoseROI->setROIColor("green");
    ui->widgetDoseROI->setTitle("Dose ROI");
    ui->widgetDoseROI->updateViewer();

    ui->widgetProjectionROI->registerViewer(ui->projectionViewer);
    ui->widgetProjectionROI->setROIColor("orange");
    ui->widgetProjectionROI->setTitle("Projection ROI");
    ui->widgetProjectionROI->updateViewer();

    ui->widgetMatrixROI->registerViewer(ui->sliceViewer);
    ui->widgetMatrixROI->setROIColor("yellow");
    ui->widgetMatrixROI->setTitle("Matrix ROI");
    ui->widgetMatrixROI->setAutoHideROI(true);
    ui->widgetMatrixROI->setAllowUpdateImageDims(false);
    ui->widgetMatrixROI->updateViewer();

    CenterOfRotationChanged();
}


void MuhRecMainWindow::lookForReferences(string &path)
{
    ReferenceFileDlg dlg;

    dlg.setPath(QString::fromStdString(path));

    int res=dlg.exec();

    if (res == QDialog::Accepted) {
        int first;
        int last;
        QString mask;

        if (dlg.getOpenBeamMask(mask,first,last)) {
            ui->editOpenBeamMask->setText(mask);
            ui->spinFirstOpenBeam->setValue(first);
            ui->spinOpenBeamCount->setValue(last-first+1);
        }

        if (dlg.getDarkCurrentMask(mask,first,last)) {
            ui->editDarkMask->setText(mask);
            ui->spinFirstDark->setValue(first);
            ui->spinDarkCount->setValue(last-first+1);
        }
    }
}

void MuhRecMainWindow::on_buttonBrowseReference_clicked()
{
    std::ostringstream msg;
    std::string sPath, sFname;
    std::vector<std::string> exts;
    if (ui->editOpenBeamMask->text().isEmpty())
        kipl::strings::filenames::StripFileName(ui->editProjectionMask->text().toStdString(),sPath,sFname,exts);
    else
        kipl::strings::filenames::StripFileName(ui->editOpenBeamMask->text().toStdString(),sPath,sFname,exts);

    QString projdir=QFileDialog::getOpenFileName(this,
                                      "Select location of the open-beam projections",
                                      ui->editOpenBeamMask->text());

    if (!projdir.isEmpty()) {
        kipl::io::DirAnalyzer da;
        kipl::io::FileItem fi=da.GetFileMask(projdir.toStdString());


        if (fi.m_sExt=="hdf"){
            ui->editOpenBeamMask->setText(projdir);
            ProjectionReader reader;
            size_t Nofimgs[2];
            reader.GetNexusInfo(projdir.toStdString(),Nofimgs, NULL);
            ui->spinFirstOpenBeam->setValue(static_cast<int>(Nofimgs[0]));
            ui->spinOpenBeamCount->setValue(static_cast<int>(Nofimgs[1]+1));
        }
        else {
            ui->editOpenBeamMask->setText(QString::fromStdString(fi.m_sMask));

            int c=0;
            int f=0;
            int l=0;

            da.AnalyzeMatchingNames(fi.m_sMask,c,f,l);

            msg<<"Found "<<c<<" files for mask "<<fi.m_sMask<<" in the interval "<<f<<" to "<<l;
            logger(logger.LogMessage,msg.str());

            ui->spinFirstOpenBeam->setValue(f);
            ui->spinOpenBeamCount->setValue(c);
        }
    }
}

void MuhRecMainWindow::ProjectionIndexChanged(int x)
{
    (void)x;

    std::ostringstream msg;
    int first=ui->spinFirstProjection->value();
    int last=ui->spinLastProjection->value();

    msg<<"New projection indices first="<<first<<", last="<<last;
    logger(logger.LogMessage,msg.str());
    if (last<first) {
        logger(logger.LogWarning,"Last<First index.");

        return ;
    }
    ui->sliderProjections->setMaximum(last);
    ui->sliderProjections->setMinimum(first);

    PreviewProjection();
}

void MuhRecMainWindow::PreviewProjection(int x)
{
    QSignalBlocker sliderSignal(ui->sliderProjections);

    std::ostringstream msg;
    ProjectionReader reader;

    if (ui->spinLastProjection->value()<ui->spinFirstProjection->value())
        return;

    if (x<=0) {
       int slice = ui->spinFirstProjection->value();
       ui->sliderProjections->setValue(slice);
    }

    msg.str("");
    msg<<"Preview slider info:min="<<ui->sliderProjections->minimum()
      <<", max="<<ui->sliderProjections->maximum()<<", current="
     << ui->sliderProjections->value();

    logger(logger.LogVerbose,msg.str());

    try {
        UpdateConfig();
        std::string fmask=ui->editProjectionMask->text().toStdString();

        std::string name, ext;
        size_t found;
        int position=ui->sliderProjections->value();

        std::map<float,ProjectionInfo> fileList;
        BuildFileList(&m_Config,&fileList);

        if (fileList.size()<position) // Workaround for bad BuildFileList implementation
        {
            logger(logger.LogWarning, "Projection slider out of list range.");
            return;
        }

        auto it=fileList.begin();
        if (position<=ui->sliderProjections->maximum())
            std::advance(it,position-(position==0 ? 0 :1));
        else {
            logger(logger.LogError,"Slider out of range");
            return;
        }

        name=it->second.name;

        if (QFile::exists(QString::fromStdString(name))) {
            int sliderval=ui->sliderProjections->value();
//            m_PreviewImage=reader.Read(name,
//                            static_cast<kipl::base::eImageFlip>(ui->comboFlipProjection->currentIndex()),
//                            static_cast<kipl::base::eImageRotate>(ui->comboRotateProjection->currentIndex()),
//                            static_cast<float>(ui->spinProjectionBinning->value()),nullptr);

            if (QFile::exists(QString::fromStdString(fmask)) || QFile::exists(QString::fromStdString(name))) {

                int sliderval=ui->sliderProjections->value();

                found = fmask.find("hdf");
                //           size_t zdims[2]={1,1};
                //            m_PreviewImage.Resize(zdims);

                try {
                    if (found!=std::string::npos )
                    {
                        m_PreviewImage=reader.ReadNexus(fmask,static_cast<size_t>(ui->sliderProjections->value()),
                                        static_cast<kipl::base::eImageFlip>(ui->comboFlipProjection->currentIndex()),
                                        static_cast<kipl::base::eImageRotate>(ui->comboRotateProjection->currentIndex()),
                                        static_cast<float>(ui->spinProjectionBinning->value()),nullptr);


                    }
                    else
                    {
//                        m_PreviewImage=reader.Read("",fmask,static_cast<size_t>(ui->sliderProjections->value()),
//                                        static_cast<kipl::base::eImageFlip>(ui->comboFlipProjection->currentIndex()),
//                                        static_cast<kipl::base::eImageRotate>(ui->comboRotateProjection->currentIndex()),
//                                        static_cast<float>(ui->spinProjectionBinning->value()),nullptr);
                        m_PreviewImage=reader.Read(name,
                                        static_cast<kipl::base::eImageFlip>(ui->comboFlipProjection->currentIndex()),
                                        static_cast<kipl::base::eImageRotate>(ui->comboRotateProjection->currentIndex()),
                                        static_cast<float>(ui->spinProjectionBinning->value()),nullptr);
                    }

                    if ( m_PreviewImage.Size()==0)
                    { // this happens in case an empty image is returned by ReadNexus
                        QMessageBox mbox(this);
                        msg.str("");
                        msg<<"KiplException: Nexus format not supported\n";
                        logger(kipl::logging::Logger::LogError,msg.str());
                        mbox.setText(QString::fromStdString(msg.str()));
                        mbox.exec();
                    }
                }
                catch(ReconException &e){
                    msg<<"ReconException: Reading file failed\n"<<e.what();

                    logger(kipl::logging::Logger::LogError,msg.str());
                    throw ReconException(msg.str(),__FILE__,__LINE__);

                }
                catch(kipl::base::KiplException &e){
                    msg<<"KiplException: Reading file failed\n"<<e.what();

                    logger(kipl::logging::Logger::LogError,msg.str());
                    throw ReconException(msg.str(),__FILE__,__LINE__);
                }

                float lo,hi;

                if (m_PreviewImage.Size()==1) {
                    logger(logger.LogWarning,"Could not read preview");
                    return ;
                }
                if (x < 0) {

                    const size_t NHist=512;
                    size_t hist[NHist];
                    float axis[NHist];
                    size_t nLo=0;
                    size_t nHi=0;

                    kipl::base::Histogram(m_PreviewImage.GetDataPtr(),m_PreviewImage.Size(),hist,NHist,0.0f,0.0f,axis);
                    kipl::base::FindLimits(hist, NHist, 99.0f, &nLo, &nHi);
                    lo=axis[nLo];
                    hi=axis[nHi];
                    ui->projectionViewer->set_image(m_PreviewImage.GetDataPtr(),m_PreviewImage.Dims(),lo,hi);
                }
                else {
                    ui->projectionViewer->get_levels(&lo,&hi);
                    ui->projectionViewer->set_image(m_PreviewImage.GetDataPtr(),m_PreviewImage.Dims(),lo,hi);
                }
                msg.str("");
                msg<<sliderval<<" ("<<std::fixed<<std::setprecision(2)<<sliderval * (ui->dspinAngleStop->value()-ui->dspinAngleStart->value())/
                     (ui->spinLastProjection->value()-ui->spinFirstProjection->value())<<" deg)";

                ui->label_projindex->setText(QString::fromStdString(msg.str()));

                SetImageDimensionLimits(m_PreviewImage.Dims());
                UpdateMemoryUsage(m_Config.ProjectionInfo.roi);
            }
            else {
                msg.str("");
                msg<<"Could not load the image "<<name<<std::endl<<"the file does not exist.";
                logger(kipl::logging::Logger::LogError,msg.str());
                throw ReconException(msg.str(),__FILE__,__LINE__);
            }
        }

    }
    catch (ReconException &e) {
        QMessageBox mbox(this);
        msg.str("");
        msg<<"Could not load the projection for preview: \n"<<e.what();
        logger(kipl::logging::Logger::LogError,msg.str());
        mbox.setText(QString::fromStdString(msg.str()));
        mbox.exec();

    }
    catch (kipl::base::KiplException &e) {
        QMessageBox mbox(this);
        msg.str("");
        msg<<"Could not load the projection for preview: \n"<<e.what();
        logger(kipl::logging::Logger::LogError,msg.str());
        mbox.setText(QString::fromStdString(msg.str()));
        mbox.exec();
    }

}

void MuhRecMainWindow::PreviewProjection()
{
    PreviewProjection(-1);
}

void MuhRecMainWindow::DisplaySlice()
{
    on_sliderSlices_sliderMoved(-1);
}

void MuhRecMainWindow::SetImageDimensionLimits(const size_t *const dims)
{
    ui->spinSlicesFirst->setMaximum(static_cast<int>(dims[1])-1);
    ui->spinSlicesLast->setMaximum(static_cast<int>(dims[1])-1);
}


void MuhRecMainWindow::CenterOfRotationChanged()
{
    int roi[4];
    ui->widgetProjectionROI->getROI(roi);
    double pos=ui->dspinRotationCenter->value()+roi[0];
    QVector<QPointF> coords;
    coords.push_back(QPointF(pos,0));
    coords.push_back(QPointF(pos,m_PreviewImage.Size(1)));


    if (ui->checkCorrectTilt->checkState()==Qt::Checked) {
        double pivot=ui->dspinTiltPivot->value();
        double tantilt=tan(ui->dspinTiltAngle->value()*3.1415/180.0);
        coords[0].setX(coords[0].x()-tantilt*pivot);
        coords[1].setX(coords[1].x()+tantilt*(coords[1].y()-pivot));
    }

    ui->projectionViewer->set_plot(coords,QColor("red").light(),0);
}

void MuhRecMainWindow::StoreGeometrySetting()
{
    if (!m_bCurrentReconStored)
    {
        UpdateConfig();
        std::ostringstream msg;
        if (m_LastMidSlice.Size()!=0) {
            m_StoredReconList.push_back(std::make_pair(m_LastReconConfig,m_LastMidSlice));
            msg<<"Stored last recon config (list size "<<m_StoredReconList.size()<<")";
        }
        else
            msg<<"Data was not reconstructed, no geometry was stored";

        logger(kipl::logging::Logger::LogMessage,msg.str());
        m_bCurrentReconStored = true;
    }
}

void MuhRecMainWindow::ClearGeometrySettings()
{
    if (!m_StoredReconList.empty())
    {
        QMessageBox confirm_dlg;

        confirm_dlg.setStandardButtons(QMessageBox::Ok | QMessageBox::Abort);
        confirm_dlg.setDefaultButton(QMessageBox::Abort);
        confirm_dlg.setText("Do you want to clear the list with stored reconstruction settings?");
        confirm_dlg.setWindowTitle("Clear configuration list");

        int res=confirm_dlg.exec();

        if (res==QMessageBox::Ok)
        {
            logger(kipl::logging::Logger::LogMessage, "The list with stored configurations was cleared.");
            m_StoredReconList.clear();
            m_bCurrentReconStored = false;
        }
    }
}

void MuhRecMainWindow::ViewGeometryList()
{
    if (!m_StoredReconList.empty()) {
        ViewGeometryListDialog dlg;
        dlg.setList(m_StoredReconList);
        int res=dlg.exec();

        if (res==QDialog::Accepted) {
            if (dlg.changedConfigFields() & ConfigField_Tilt) {
                float center,tilt, pivot;
                dlg.getTilt(center, tilt, pivot);
                ui->dspinTiltAngle->setValue(tilt);
                ui->dspinTiltPivot->setValue(pivot);
                ui->dspinRotationCenter->setValue(center);
            }

//            if (dlg.changedConfigFields() & ConfigField_ROI) {

//                dlg.getROI(m_Config.MatrixInfo.roi);

//                ui->spinMatrixROI0->setValue(m_Config.MatrixInfo.roi[0]);
//                ui->spinMatrixROI1->setValue(m_Config.MatrixInfo.roi[1]);
//                ui->spinMatrixROI2->setValue(m_Config.MatrixInfo.roi[2]);
//                ui->spinMatrixROI3->setValue(m_Config.MatrixInfo.roi[3]);
//            }
        }
    }
}

void MuhRecMainWindow::MatrixROIChanged(int x)
{
    // todo Update the matrix roi callback
//    (void) x;
//    logger(kipl::logging::Logger::LogMessage,"MatrixROI changed");
//    UpdateMatrixROI();

//    if (m_Config.ProjectionInfo.beamgeometry == m_Config.ProjectionInfo.BeamGeometry_Cone){
//        ComputeVolumeSize(); // updated volume size
//        std::ostringstream msg;
//        msg.str("");
//        msg<<"Volume size update: "<<m_Config.MatrixInfo.nDims[0]<<" "<<m_Config.MatrixInfo.nDims[1]<<" "<<m_Config.MatrixInfo.nDims[2];
//        logger(logger.LogMessage,msg.str());
//    }
}

void MuhRecMainWindow::MenuFileNew()
{
    if (m_pEngine!=nullptr) {
        delete m_pEngine;
        m_pEngine=nullptr;
    }
    LoadDefaults(false);
}

void MuhRecMainWindow::LoadDefaults(bool checkCurrent)
{
    std::ostringstream msg;

    QDir dir;

    std::string defaultsname=m_sHomePath+".imagingtools/CurrentRecon.xml";
    kipl::strings::filenames::CheckPathSlashes(defaultsname,false);


    std::string sModulePath=m_sApplicationPath;
    kipl::strings::filenames::CheckPathSlashes(sModulePath,true);

    msg.str("");
    msg<<"default name is "<<defaultsname<<" it "<<(dir.exists(QString::fromStdString(defaultsname))==true ? "exists" : "doesn't exist")<<" and should "<< (checkCurrent ? " " : "not ")<<"be used";
    logger(logger.LogMessage,msg.str());
    bool bUseDefaults=true;
    if ((checkCurrent==true) && // do we check for previous recons?
            (dir.exists(QString::fromStdString(defaultsname))==true)) { // is there a previous recon?
        bUseDefaults=false;
    }
    else {
#ifdef Q_OS_DARWIN
        defaultsname=m_sApplicationPath+"../Resources/defaults_mac.xml";
        sModulePath+="..";
#endif

#ifdef Q_OS_WIN
         defaultsname=m_sApplicationPath+"resources/defaults_windows.xml";
#endif

#ifdef Q_OS_LINUX
        defaultsname=m_sApplicationPath+"../resources/defaults_linux.xml";
        sModulePath+="..";
#endif
        bUseDefaults=true;
    }

    kipl::strings::filenames::CheckPathSlashes(defaultsname,false);
    msg.str("");
    msg<<"Looking for defaults at "<<defaultsname;
    logger(kipl::logging::Logger::LogMessage,msg.str());

    msg.str("");
    try {
        m_Config.LoadConfigFile(defaultsname.c_str(),"reconstructor");
        msg.str("");
        msg<<m_Config.WriteXML();
        logger(logger.LogMessage,msg.str());
    }
    catch (ReconException &e) {
        msg.str("");
        msg<<"Loading defaults failed :\n"<<e.what();
        logger(kipl::logging::Logger::LogError,msg.str());
    }
    catch (ModuleException &e) {
        msg.str("");
        msg<<"Loading defaults failed :\n"<<e.what();
        logger(kipl::logging::Logger::LogError,msg.str());
    }
    catch (kipl::base::KiplException &e) {
        msg.str("");
        msg<<"Loading defaults failed :\n"<<e.what();
        logger(kipl::logging::Logger::LogError,msg.str());
    }

    if (bUseDefaults) {
        m_Config.ProjectionInfo.sPath              = m_sHomePath;
        m_Config.ProjectionInfo.sReferencePath     = m_sHomePath;
        m_Config.MatrixInfo.sDestinationPath       = m_sHomePath;

        std::list<ModuleConfig>::iterator it;

        std::string sSearchStr = "@executable_path";

        // Replace template path by module path for pre processing
        size_t pos=0;

        logger(logger.LogMessage,"Updating path of preprocessing modules");
        for (it=m_Config.modules.begin(); it!=m_Config.modules.end(); it++) {
            pos=it->m_sSharedObject.find(sSearchStr);

            if (pos!=std::string::npos)
                it->m_sSharedObject.replace(pos,sSearchStr.size(),sModulePath);

            logger(kipl::logging::Logger::LogMessage,it->m_sSharedObject);
        }

        logger(logger.LogMessage,"Updating path of preprocessing modules");
        pos=m_Config.backprojector.m_sSharedObject.find(sSearchStr);
        if (pos!=std::string::npos)
            m_Config.backprojector.m_sSharedObject.replace(pos,sSearchStr.size(),sModulePath);

        logger(kipl::logging::Logger::LogMessage,m_Config.backprojector.m_sSharedObject);

        size_t dims[2]={100,100};
        kipl::base::TImage<float,2> img=kipl::generators::Sine2D::SineRings(dims,2.0f);
        ui->projectionViewer->set_image(img.GetDataPtr(),img.Dims());
        ui->sliceViewer->set_image(img.GetDataPtr(),img.Dims());
    }
    std::copy(m_Config.ProjectionInfo.projection_roi,
              m_Config.ProjectionInfo.projection_roi,
              m_oldROI);

    UpdateDialog();
    UpdateMemoryUsage(m_Config.ProjectionInfo.roi);
    m_sConfigFilename=m_sHomePath+"noname.xml";
}

void MuhRecMainWindow::MenuFileOpen()
{
    std::ostringstream msg;
    QString fileName = QFileDialog::getOpenFileName(this,tr("Open reconstruction configuration"),tr("*.xml"));

    QMessageBox msgbox;

    msgbox.setWindowTitle(tr("Config file error"));
    msgbox.setText(tr("Failed to load the configuration file"));

    try {
        m_Config.LoadConfigFile(fileName.toStdString(),"reconstructor");
    }
    catch (ReconException & e) {
        msg<<"Failed to load the configuration file :\n"<<
             e.what();
        msgbox.setDetailedText(QString::fromStdString(msg.str()));
        msgbox.exec();
    }
    catch (ModuleException & e) {
        msg<<"Failed to load the configuration file :\n"<<
             e.what();
        msgbox.setDetailedText(QString::fromStdString(msg.str()));
        msgbox.exec();
    }
    catch (kipl::base::KiplException & e) {
        msg<<"Failed to load the configuration file :\n"<<
             e.what();
        msgbox.setDetailedText(QString::fromStdString(msg.str()));
        msgbox.exec();
    }
    catch (std::exception & e) {
        msg<<"Failed to load the configuration file :\n"<<
             e.what();
        msgbox.setDetailedText(QString::fromStdString(msg.str()));
        msgbox.exec();
    }
    UpdateDialog();

}

void MuhRecMainWindow::MenuFileSave()
{
    if (m_sConfigFilename=="noname.xml")
        MenuFileSaveAs();
    else {
        UpdateConfig();
        std::ofstream conffile(m_sConfigFilename.c_str());

        conffile<<m_Config.WriteXML();
    }
}

void MuhRecMainWindow::MenuFileSaveAs()
{
    QString fname=QFileDialog::getSaveFileName(this,"Save configuration file",QDir::homePath());

    UpdateConfig();
    m_sConfigFilename=fname.toStdString();
    std::ofstream conffile(m_sConfigFilename.c_str());

    conffile<<m_Config.WriteXML();
}

void MuhRecMainWindow::MenuFileQuit()
{
    m_QtApp->quit();
}

void MuhRecMainWindow::MenuHelpAbout()
{
    QMessageBox dlg;
    std::ostringstream msg;
    QVersionNumber ver;

    msg<<"MuhRec "<<m_QtApp->applicationVersion().toStdString()<<"\nCompile date: "<<__DATE__<<" at "<<__TIME__<<std::endl;

    msg<<"Using \nQt version: "<<qVersion()<<"\n"
      <<"LibTIFF, zLib, fftw3, libcfitsio";

    dlg.setText(QString::fromStdString(msg.str()));

    dlg.exec();
}

void MuhRecMainWindow::saveCurrentRecon()
{
    ostringstream confpath;
    ostringstream msg;
    // Save current recon settings
    QDir dir;

    QString path=dir.homePath()+"/.imagingtools";

    logger(kipl::logging::Logger::LogMessage,path.toStdString());
    if (!dir.exists(path)) {
        dir.mkdir(path);
    }
    std::string sPath=path.toStdString();
    kipl::strings::filenames::CheckPathSlashes(sPath,true);
    confpath<<sPath<<"CurrentRecon.xml";

    try {
        UpdateConfig();
        ofstream of(confpath.str().c_str());
        if (!of.is_open()) {
            msg.str("");
            msg<<"Failed to open config file: "<<confpath.str()<<" for writing.";
            logger(kipl::logging::Logger::LogError,msg.str());
            return ;
        }

        of<<m_Config.WriteXML();
        of.flush();
        logger(kipl::logging::Logger::LogMessage,"Saved current recon config");
    }
    catch (kipl::base::KiplException &e) {
        msg.str("");
        msg<<"Saving current config failed with exception: "<<e.what();
        logger(kipl::logging::Logger::LogError,msg.str());
        return;
    }
    catch (std::exception &e) {
        msg.str("");
        msg<<"Saving current config failed with exception: "<<e.what();
        logger(kipl::logging::Logger::LogError,msg.str());
        return;
    }


}

void MuhRecMainWindow::MenuReconstructStart()
{
    ostringstream msg;

    ui->tabMainControl->setCurrentIndex(4);

    try {
        UpdateConfig();
    }
    catch (ModuleException &e) {
        QMessageBox::warning(this,"Reconstruction failed","Reconstruction failed due to bad configuration (Module Exception).");
        return ;
    }
    catch (ReconException &e) {
        QMessageBox::warning(this,"Reconstruction failed","Reconstruction failed due to bad configuration (Reconstruction Exception).");
        return ;
    }
    catch (kipl::base::KiplException &e) {
        QMessageBox::warning(this,"Reconstruction failed","Reconstruction failed due to bad configuration (kipl Exception).");
        return ;
    }

    try {
        m_Config.MatrixInfo.bAutomaticSerialize=false;
        if (m_Config.System.nMemory<m_nRequiredMemory) {

            DialogTooBig largesize_dlg;
            int roi[4];
            ui->widgetProjectionROI->getROI(roi);
            largesize_dlg.SetFields(ui->editDestPath->text(),
                                    ui->editSliceMask->text(),
                                    false,
                                    ui->spinSlicesFirst->value(),
                                    ui->spinSlicesLast->value(),
                                    roi[1],roi[3]);
            int res=largesize_dlg.exec();

            if (res!=QDialog::Accepted) {
                logger(logger.LogMessage,"Reconstruction was aborted");
                return;
            }
            m_Config.MatrixInfo.sDestinationPath = largesize_dlg.GetPath().toStdString();
            kipl::strings::filenames::CheckPathSlashes(m_Config.MatrixInfo.sDestinationPath,true);
            m_Config.MatrixInfo.sFileMask        = largesize_dlg.GetMask().toStdString();
            ui->editDestPath->setText(QString::fromStdString(m_Config.MatrixInfo.sDestinationPath));
            ui->editSliceMask->setText(QString::fromStdString(m_Config.MatrixInfo.sFileMask));

            msg.str("");
            msg<<"Reconstructing direct to folder "<<m_Config.MatrixInfo.sDestinationPath
              <<" using the mask "<< m_Config.MatrixInfo.sFileMask;
            logger(logger.LogMessage,msg.str());

            m_Config.MatrixInfo.bAutomaticSerialize=true;
        }
        else
            m_Config.MatrixInfo.bAutomaticSerialize=false;

        saveCurrentRecon();
    }
    catch (ModuleException &e) {
        QMessageBox::warning(this,"Reconstruction failed","Reconstruction failed due to bad configuration (Module Exception).");
        return ;
    }
    catch (ReconException &e) {
        QMessageBox::warning(this,"Reconstruction failed","Reconstruction failed due to bad configuration (Reconstruction Exception).");
        return ;
    }
    catch (kipl::base::KiplException &e) {
        QMessageBox::warning(this,"Reconstruction failed","Reconstruction failed due to bad configuration (kipl Exception).");
        return ;
    }

    saveCurrentRecon();

    try {
        ExecuteReconstruction();
    }
    catch (ModuleException &e) {
        QMessageBox::warning(this,"Reconstruction failed","Reconstruction failed due to bad configuration (Module Exception).");
        return ;
    }
    catch (ReconException &e) {
        QMessageBox::warning(this,"Reconstruction failed","Reconstruction failed due to bad configuration (Reconstruction Exception).");
        return ;
    }
    catch (kipl::base::KiplException &e) {
        QMessageBox::warning(this,"Reconstruction failed","Reconstruction failed due to bad configuration (kipl Exception).");
        return ;
    }
}

void MuhRecMainWindow::ExecuteReconstruction()
{
    std::ostringstream msg;
    std::list<string> freelist;
    bool bBuildFailure=false;

    freelist.push_back("grayinterval");
    freelist.push_back("center");
    freelist.push_back("operator");
    freelist.push_back("instrument");
    freelist.push_back("projectnumber");
    freelist.push_back("sample");
    freelist.push_back("comment");
    freelist.push_back("rotation");
    freelist.push_back("tiltangle");
    freelist.push_back("tiltpivot");
    freelist.push_back("correcttilt");

    freelist.push_back("usematrixroi");
    freelist.push_back("matrixroi");

    bool bRerunBackproj=!m_Config.ConfigChanged(m_LastReconConfig,freelist);
    msg.str(""); msg<<"Config has "<<(bRerunBackproj ? "not" : "")<<" changed";
    logger(kipl::logging::Logger::LogMessage,msg.str());

    if ( bRerunBackproj==false || m_pEngine==nullptr || m_Config.MatrixInfo.bAutomaticSerialize==true) {
        bRerunBackproj=false; // Just in case if other cases outruled re-run

        logger(kipl::logging::Logger::LogMessage,"Preparing for full recon");
        msg.str("");
        try {
            if (m_pEngine!=nullptr)
            {
                delete m_pEngine;
            }

            m_pEngine=nullptr;

            m_pEngine=m_Factory.BuildEngine(m_Config,&m_Interactor);
        }
        catch (std::exception &e) {
            msg<<"Reconstructor initialization failed with an STL exception: "<<std::endl
                <<e.what();
            bBuildFailure=true;
        }
        catch (ModuleException &e) {
            msg<<"Reconstructor initialization failed with a ModuleException: \n"
                <<e.what();
            bBuildFailure=true;
        }
        catch (ReconException &e) {
            msg<<"Reconstructor initialization failed with a recon exception: "<<std::endl
                <<e.what();
            bBuildFailure=true;
        }
        catch (kipl::base::KiplException &e) {
            msg<<"Reconstructor initialization failed a Kipl exception: "<<std::endl
                <<e.what();
            bBuildFailure=true;
        }
        catch (...) {
            msg<<"Reconstructor initialization failed with an unknown exception";
            bBuildFailure=true;
        }

        if (bBuildFailure) {
            logger(kipl::logging::Logger::LogError,msg.str());
            QMessageBox error_dlg;
            error_dlg.setText("Failed to build reconstructor due to plugin exception. See log message for more information.");
            error_dlg.setDetailedText(QString::fromStdString(msg.str()));

            error_dlg.exec();

            if (m_pEngine!=nullptr)
                delete m_pEngine;
            m_pEngine=nullptr;

            return ;
        }
    }
    else {
            logger(kipl::logging::Logger::LogMessage,"Preparing for back proj only");
            if (m_pEngine!=nullptr)
                m_pEngine->SetConfig(m_Config); // Set new recon parameters for the backprojector
            else {
                logger(logger.LogError,"No engine allocated");
                return;
            }
            bRerunBackproj=true;
    }


    msg.str("");

    ReconDialog dlg(&m_Interactor);
    bool bRunFailure=false;
    try {
        if (m_pEngine!=nullptr) {
            int res=0;
            ui->tabMainControl->setCurrentIndex(3);
            res=dlg.exec(m_pEngine,bRerunBackproj);

            if (res==QDialog::Accepted) {
                logger(kipl::logging::Logger::LogVerbose,"Finished with OK");

                // Store info about last recon
                m_LastReconConfig     = m_Config;
                m_bCurrentReconStored = false;
                size_t dims[3];
                m_pEngine->GetMatrixDims(dims);
                m_LastMidSlice = m_pEngine->GetSlice(dims[2]/2);

                // Prepare visualization
                if (m_Config.MatrixInfo.bAutomaticSerialize==false) {
                    PreviewProjection(); // Display the projection if it was not done already
                    ui->tabMainControl->setCurrentIndex(3);

                    const int nBins=256;
                    float x[nBins];
                    size_t y[nBins];
                    m_pEngine->GetHistogram(x,y,nBins);
                    ui->plotHistogram->setCurveData(0,x,y,nBins);

                    m_pEngine->GetMatrixDims(m_Config.MatrixInfo.nDims);
                    msg.str("");
                    msg<<"Reconstructed "<<m_Config.MatrixInfo.nDims[2]<<" slices";
                    logger(kipl::logging::Logger::LogMessage,msg.str());

                    ui->sliderSlices->setRange(0,static_cast<int>(m_Config.MatrixInfo.nDims[2])-1);
                    ui->sliderSlices->setValue(static_cast<int>(m_Config.MatrixInfo.nDims[2]/2));
                    m_nSliceSizeX=m_Config.MatrixInfo.nDims[0];
                    m_nSliceSizeY=m_Config.MatrixInfo.nDims[1];
                    m_eSlicePlane=kipl::base::ImagePlaneXY;

                    msg.str("");
                    msg<<"Matrix display interval ["<<m_Config.MatrixInfo.fGrayInterval[0]<<", "<<m_Config.MatrixInfo.fGrayInterval[1]<<"]";
                    logger(kipl::logging::Logger::LogMessage,msg.str());

                    DisplaySlice();
                }
                else {
                    std::string fname=m_Config.MatrixInfo.sDestinationPath+"ReconConfig.xml";
                    kipl::strings::filenames::CheckPathSlashes(fname,false);
                    std::ofstream configfile(fname.c_str());
                    configfile<<m_Config.WriteXML();
                    configfile.close();

                    delete m_pEngine;
                    m_pEngine=nullptr;
                }

            }
            if (res==QDialog::Rejected)
            {
                logger(kipl::logging::Logger::LogVerbose,"Finished with Cancel");
                if (m_pEngine!=nullptr)
                {
                    delete m_pEngine;
                    m_pEngine = nullptr;
                }
            }
        }
    }
    catch (std::exception &e) {
        msg<<"Reconstruction failed: "<<std::endl
            <<e.what();
        bRunFailure=true;
    }
    catch (ModuleException &e) {
        msg<<"Reconstruction failed with a module exception: \n"
            <<e.what();
        bRunFailure=true;
    }
    catch (ReconException &e) {
        msg<<"Reconstruction failed: "<<std::endl
            <<e.what();
        bRunFailure=true;
    }
    catch (kipl::base::KiplException &e) {
        msg<<"Reconstruction failed: "<<std::endl
            <<e.what();
        bRunFailure=true;
    }

    catch (...) {
        msg<<"Reconstruction failed";
        bRunFailure=true;
    }

    if (bRunFailure) {
        logger(kipl::logging::Logger::LogError,msg.str());

        QMessageBox error_dlg;
        error_dlg.setText("Failed to run the reconstructor.");
        error_dlg.setDetailedText(QString::fromStdString(msg.str()));

        error_dlg.exec();
        if (m_pEngine!=nullptr)
            delete m_pEngine;
        m_pEngine=nullptr;

        return ;
    }
}


void MuhRecMainWindow::UpdateMemoryUsage(size_t * roi)
{
    ostringstream msg;
    try  {
        ostringstream text;
        m_nRequiredMemory=0;
        double nMatrixMemory=0;
        double nBufferMemory=0;
        // Matrix size
        double length = abs(static_cast<ptrdiff_t>(roi[2])-static_cast<ptrdiff_t>(roi[0]));
        double height = 0;

        height = abs(static_cast<ptrdiff_t>(roi[3])-static_cast<ptrdiff_t>(roi[1]));
        text.str("");

        nMatrixMemory = length*length*height*sizeof(float);
        // Memory for temp matrix
        double blocksize=GetIntParameter(m_Config.backprojector.parameters,"SliceBlock");
        blocksize=min(blocksize,height);
        nBufferMemory = length*length*blocksize*sizeof(float);
        // Projection buffer
        double projbuffersize=GetIntParameter(m_Config.backprojector.parameters,"ProjectionBufferSize");
        nBufferMemory += length*height*projbuffersize*sizeof(float);

        // Projection Data
        double nprojections=((double)m_Config.ProjectionInfo.nLastIndex-(double)m_Config.ProjectionInfo.nFirstIndex+1)/(double)m_Config.ProjectionInfo.nProjectionStep;
        nBufferMemory += length*height*nprojections*sizeof(float);

        nMatrixMemory/=1024*1024;
        nBufferMemory/=(1024*1024);
        text.str("");
        m_nRequiredMemory=static_cast<size_t>(nBufferMemory+nMatrixMemory);
//        m_nRequiredMemory=static_cast<size_t>(nMatrixMemory);
        text<<"Memory usage: "<<m_nRequiredMemory
           <<"Mb (matrix: "<<ceil(nMatrixMemory)<<", buffers: "
           <<ceil(nBufferMemory)<<") system max "
           <<m_Config.System.nMemory<<"Mb";

        ui->statusBar->showMessage(QString::fromStdString(text.str()));
    }
    catch (ModuleException &e) {
        msg<<"Failed to compute the memory usage\n"<<e.what();
    }
    catch (ReconException &e) {
        msg<<"Failed to compute the memory usage\n"<<e.what();
    }
    catch (kipl::base::KiplException &e) {
        msg<<"Failed to compute the memory usage\n"<<e.what();
    }
    catch (std::exception &e) {
        msg<<"Failed to compute the memory usage\n"<<e.what();
    }
    catch (...) {
        msg<<"Failed to compute the memory usage\n";
    }

    if (!msg.str().empty()) {
        QMessageBox error_dlg;
        error_dlg.setWindowTitle("Error");
        error_dlg.setText("Failed to build reconstructor due to plugin exception");
        error_dlg.setDetailedText(QString::fromStdString(msg.str()));
    }
}

void MuhRecMainWindow::UpdateDialog()
{

    std::ostringstream str;
    QSignalBlocker blockFirstProjection(ui->spinFirstProjection);
    QSignalBlocker blockLastProjection(ui->spinLastProjection);
    QSignalBlocker blockFlipProjection(ui->comboFlipProjection);
    QSignalBlocker blockRotateProjection(ui->comboRotateProjection);

    ui->editProjectionMask->setText(QString::fromStdString(m_Config.ProjectionInfo.sFileMask));
    ui->editOpenBeamMask->setText(QString::fromStdString(m_Config.ProjectionInfo.sOBFileMask));
    ui->editDarkMask->setText(QString::fromStdString(m_Config.ProjectionInfo.sDCFileMask));

    ui->spinFirstProjection->setValue(static_cast<int>(m_Config.ProjectionInfo.nFirstIndex));
    ui->spinLastProjection->setValue(static_cast<int>(m_Config.ProjectionInfo.nLastIndex));
    ui->spinProjectionStep->setValue(static_cast<int>(m_Config.ProjectionInfo.nProjectionStep));
    ui->comboProjectionStyle->setCurrentIndex(m_Config.ProjectionInfo.imagetype);
    ui->spinProjectionBinning->setValue(m_Config.ProjectionInfo.fBinning);
    ui->comboFlipProjection->setCurrentIndex(m_Config.ProjectionInfo.eFlip);
    ui->comboRotateProjection->setCurrentIndex(m_Config.ProjectionInfo.eRotate);
  //  ProjectionIndexChanged(-1);

    ui->spinFirstOpenBeam->setValue(static_cast<int>(m_Config.ProjectionInfo.nOBFirstIndex));
    ui->spinOpenBeamCount->setValue(static_cast<int>(m_Config.ProjectionInfo.nOBCount));


    ui->spinFirstDark->setValue(static_cast<int>(m_Config.ProjectionInfo.nDCFirstIndex));
    ui->spinDarkCount->setValue(static_cast<int>(m_Config.ProjectionInfo.nDCCount));

    ui->widgetDoseROI->setROI(m_Config.ProjectionInfo.dose_roi,true);

    QSignalBlocker blockSlicesFirst(ui->spinSlicesFirst);
    QSignalBlocker blockSlicesLast(ui->spinSlicesLast);

    std::copy(m_Config.ProjectionInfo.projection_roi,m_Config.ProjectionInfo.projection_roi+4,m_oldROI);
 //   qDebug("UpdateDialog");
    ui->widgetProjectionROI->setROI(m_Config.ProjectionInfo.projection_roi,true);

    on_widgetProjectionROI_valueChanged(m_Config.ProjectionInfo.projection_roi[0],
                                        m_Config.ProjectionInfo.projection_roi[1],
                                        m_Config.ProjectionInfo.projection_roi[2],
                                        m_Config.ProjectionInfo.projection_roi[3]);

    ui->spinSlicesFirst->setValue(static_cast<int>(m_Config.ProjectionInfo.roi[1]));
    ui->spinSlicesLast->setValue(static_cast<int>(m_Config.ProjectionInfo.roi[3]));

    // Scan arc settings
    ui->dspinAngleStart->setValue(m_Config.ProjectionInfo.fScanArc[0]);
    ui->dspinAngleStop->setValue(m_Config.ProjectionInfo.fScanArc[1]);
    if ( m_Config.ProjectionInfo.fScanArc[0] == 0)
    {
        if (m_Config.ProjectionInfo.fScanArc[1] == 360)
        {
            ui->radioButton_fullTurn->setChecked(true);
            on_radioButton_fullTurn_clicked();
        }
        else if (m_Config.ProjectionInfo.fScanArc[1] == 180)
        {
            ui->radioButton_halfTurn1->setChecked(true);
            on_radioButton_halfTurn1_clicked();
        }
        else
        {
            ui->radioButton_customTurn->setChecked(true);
            on_radioButton_customTurn_clicked();
        }
    }
    else {
        if (m_Config.ProjectionInfo.fScanArc[0] == 180) {
            if (m_Config.ProjectionInfo.fScanArc[1] == 360) {
                ui->radioButton_halfTurn2->setChecked(true);
                on_radioButton_halfTurn2_clicked();
            }
            else {
                ui->radioButton_customTurn->setChecked(true);
                on_radioButton_customTurn_clicked();
            }
        }
    }

    ui->comboDataSequence->setCurrentIndex(m_Config.ProjectionInfo.scantype);
    ui->dspinResolution->setValue(m_Config.ProjectionInfo.fResolution[0]);

    // Set center of rotation
    ui->dspinRotationCenter->setValue(m_Config.ProjectionInfo.fCenter);
    ui->dspinTiltAngle->setValue(m_Config.ProjectionInfo.fTiltAngle);
    ui->dspinTiltPivot->setValue(m_Config.ProjectionInfo.fTiltPivotPosition);
    ui->checkCorrectTilt->setChecked(m_Config.ProjectionInfo.bCorrectTilt);
    on_checkCorrectTilt_clicked(m_Config.ProjectionInfo.bCorrectTilt);
    on_dspinRotationCenter_valueChanged(m_Config.ProjectionInfo.fCenter);


    ui->check_stitchprojections->setChecked(m_Config.ProjectionInfo.bTranslate);
    ui->moduleconfigurator->SetModules(m_Config.modules);
    ui->dspinRotateRecon->setValue(m_Config.MatrixInfo.fRotation);

    ui->dspinGrayLow->setValue(m_Config.MatrixInfo.fGrayInterval[0]);
    ui->dspinGrayHigh->setValue(m_Config.MatrixInfo.fGrayInterval[1]);

    ui->widgetMatrixROI->setROI(m_Config.MatrixInfo.roi,true);
    ui->checkUseMatrixROI->setChecked(m_Config.MatrixInfo.bUseROI);
    on_checkUseMatrixROI_toggled(m_Config.MatrixInfo.bUseROI);

    ui->editDestPath->setText(QString::fromStdString(m_Config.MatrixInfo.sDestinationPath));
    ui->editSliceMask->setText(QString::fromStdString(m_Config.MatrixInfo.sFileMask));
    ui->comboDestFileType->setCurrentIndex(m_Config.MatrixInfo.FileType-3);
    // -2 to skip matlab types

    ui->editProjectName->setText(QString::fromStdString(m_Config.UserInformation.sProjectNumber));
    ui->editOperatorName->setText(QString::fromStdString(m_Config.UserInformation.sOperator));
    ui->editInstrumentName->setText(QString::fromStdString(m_Config.UserInformation.sInstrument));
    ui->editSampleDescription->setText(QString::fromStdString(m_Config.UserInformation.sSample));
    ui->editExperimentDescription->setText(QString::fromStdString(m_Config.UserInformation.sComment));


    str.str("");
    std::set<size_t>::iterator it;
    if (!m_Config.ProjectionInfo.nlSkipList.empty()) {
        for (it=m_Config.ProjectionInfo.nlSkipList.begin(); it!=m_Config.ProjectionInfo.nlSkipList.end(); it++)
            str<<*it<<" ";
        ui->editProjectionSkipList->setText(QString::fromStdString(str.str()));
        ui->checkBoxUseSkipList->setChecked(true);
        on_checkBoxUseSkipList_toggled(true);
    }
    else {
        ui->checkBoxUseSkipList->setChecked(false);
        ui->editProjectionSkipList->clear();
        on_checkBoxUseSkipList_toggled(false);
    }
    ui->ConfiguratorBackProj->SetModule(m_Config.backprojector);

    ui->dspinSDD->setValue(m_Config.ProjectionInfo.fSDD);
    ui->dspinSOD->setValue(m_Config.ProjectionInfo.fSOD);

    ui->dspinPiercPointX->setValue(m_Config.ProjectionInfo.fpPoint[0]);
    ui->dspinPiercPointY->setValue(m_Config.ProjectionInfo.fpPoint[1]);

    ui->comboDirRotation->setCurrentIndex(m_Config.ProjectionInfo.eDirection);

    if(m_Config.ProjectionInfo.beamgeometry == m_Config.ProjectionInfo.BeamGeometry_Cone) {
        ui->checkCBCT->setChecked(true);
        ui->groupBox_ConeBeamGeometry->setVisible(true);
//        ComputeVolumeSizeSpacing();
    }
    else{
        ui->checkCBCT->setChecked(false);
        ui->groupBox_ConeBeamGeometry->setVisible(false);
    }

    CenterOfRotationChanged();

    SlicesChanged(0);
}

void MuhRecMainWindow::UpdateConfig()
{
    std::ostringstream msg;
    m_Config.ProjectionInfo.sPath="";
    m_Config.ProjectionInfo.sReferencePath="";

    m_Config.ProjectionInfo.sFileMask = ui->editProjectionMask->text().toStdString();
    kipl::strings::filenames::CheckPathSlashes(m_Config.ProjectionInfo.sFileMask,false);
    m_Config.ProjectionInfo.nFirstIndex = ui->spinFirstProjection->value();
    m_Config.ProjectionInfo.nLastIndex = ui->spinLastProjection->value();
    if (m_Config.ProjectionInfo.nLastIndex<m_Config.ProjectionInfo.nFirstIndex) {
        qDebug()<<"Update config: Last<First projection";
        std::swap(m_Config.ProjectionInfo.nLastIndex,m_Config.ProjectionInfo.nFirstIndex);
        ui->spinFirstProjection->setValue(m_Config.ProjectionInfo.nFirstIndex);
        ui->spinLastProjection->setValue(m_Config.ProjectionInfo.nLastIndex);
        QMessageBox::information(this,"Last<First projection","Last<First projection, swapped values");
    }

    m_Config.ProjectionInfo.nProjectionStep = ui->spinProjectionStep->value();
    m_Config.ProjectionInfo.imagetype = static_cast<ReconConfig::cProjections::eImageType>(ui->comboProjectionStyle->currentIndex());
    m_Config.ProjectionInfo.fBinning = ui->spinProjectionBinning->value();
    m_Config.ProjectionInfo.eFlip = static_cast<kipl::base::eImageFlip>(ui->comboFlipProjection->currentIndex());
    m_Config.ProjectionInfo.eRotate = static_cast<kipl::base::eImageRotate>(ui->comboRotateProjection->currentIndex());

//    m_Config.ProjectionInfo.sReferencePath = ui->editReferencePath->text().toStdString();
//    kipl::strings::filenames::CheckPathSlashes(m_Config.ProjectionInfo.sReferencePath,true);

    m_Config.ProjectionInfo.sOBFileMask = ui->editOpenBeamMask->text().toStdString();
    kipl::strings::filenames::CheckPathSlashes(m_Config.ProjectionInfo.sOBFileMask,false);

    m_Config.ProjectionInfo.nOBFirstIndex = ui->spinFirstOpenBeam->value();
    m_Config.ProjectionInfo.nOBCount = ui->spinOpenBeamCount->value();
    m_Config.ProjectionInfo.sDCFileMask = ui->editDarkMask->text().toStdString();
    kipl::strings::filenames::CheckPathSlashes(m_Config.ProjectionInfo.sDCFileMask,false);

    m_Config.ProjectionInfo.nDCFirstIndex = ui->spinFirstDark->value();
    m_Config.ProjectionInfo.nDCCount = ui->spinDarkCount->value();
    std::string str=ui->editProjectionSkipList->text().toStdString();
    if (!str.empty() && ui->checkBoxUseSkipList->isChecked())
        kipl::strings::String2Set(str,m_Config.ProjectionInfo.nlSkipList);
    else
        m_Config.ProjectionInfo.nlSkipList.clear();

    ui->widgetDoseROI->getROI(m_Config.ProjectionInfo.dose_roi);
    ui->widgetProjectionROI->getROI(m_Config.ProjectionInfo.projection_roi);

    m_Config.ProjectionInfo.roi[0] = m_Config.ProjectionInfo.projection_roi[0];
    m_Config.ProjectionInfo.roi[2] = m_Config.ProjectionInfo.projection_roi[2];
    m_Config.ProjectionInfo.roi[1] = ui->spinSlicesFirst->value();
    m_Config.ProjectionInfo.roi[3] = ui->spinSlicesLast->value();

    m_Config.ProjectionInfo.fCenter = ui->dspinRotationCenter->value();
    m_Config.ProjectionInfo.fScanArc[0] = ui->dspinAngleStart->value();
    m_Config.ProjectionInfo.fScanArc[1] = ui->dspinAngleStop->value();
    m_Config.ProjectionInfo.scantype = static_cast<ReconConfig::cProjections::eScanType>(ui->comboDataSequence->currentIndex());
    m_Config.ProjectionInfo.fResolution[0] = m_Config.ProjectionInfo.fResolution[1] = ui->dspinResolution->value();
    m_Config.ProjectionInfo.fTiltAngle = ui->dspinTiltAngle->value();
    m_Config.ProjectionInfo.fTiltPivotPosition = ui->dspinTiltPivot->value();
    m_Config.ProjectionInfo.bCorrectTilt = ui->checkCorrectTilt->checkState();
    m_Config.ProjectionInfo.bTranslate = ui->check_stitchprojections->checkState();

    m_Config.ProjectionInfo.fSDD = ui->dspinSDD->value();
    m_Config.ProjectionInfo.fSOD = ui->dspinSOD->value();

    m_Config.ProjectionInfo.fpPoint[0] = ui->dspinPiercPointX->value();
    m_Config.ProjectionInfo.fpPoint[1] = ui->dspinPiercPointY->value();

    CenterOfRotationChanged();

    if (ui->checkCBCT->isChecked()) {
        m_Config.ProjectionInfo.beamgeometry = m_Config.ProjectionInfo.BeamGeometry_Cone;
        ComputeVolumeSizeSpacing();

        ui->groupBox_ConeBeamGeometry->show();
        SlicesChanged(0);
    }
    else {
        m_Config.ProjectionInfo.beamgeometry= m_Config.ProjectionInfo.BeamGeometry_Parallel;
        ui->groupBox_ConeBeamGeometry->hide();
        SlicesChanged(0);
    }


    m_Config.MatrixInfo.voi[0] = 0;
    m_Config.MatrixInfo.voi[1] = (m_Config.ProjectionInfo.roi[2]-m_Config.ProjectionInfo.roi[0]);
    m_Config.MatrixInfo.voi[2] = 0;
    m_Config.MatrixInfo.voi[3] = (m_Config.ProjectionInfo.roi[2]-m_Config.ProjectionInfo.roi[0]);
//        m_Config.MatrixInfo.voi[4] = m_Config.ProjectionInfo.roi[3]-ui->spinSubVolumeSizeZ1->value();
//        m_Config.MatrixInfo.voi[5] = m_Config.ProjectionInfo.roi[3]-ui->spinSubVolumeSizeZ0->value();
    m_Config.MatrixInfo.voi[4] = 0;
    m_Config.MatrixInfo.voi[5] = m_Config.ProjectionInfo.roi[3]-m_Config.ProjectionInfo.roi[1];



    m_Config.modules = ui->moduleconfigurator->GetModules();
    m_Config.MatrixInfo.fRotation= ui->dspinRotateRecon->value();
    m_Config.MatrixInfo.fGrayInterval[0] = ui->dspinGrayLow->value();
    m_Config.MatrixInfo.fGrayInterval[1] = ui->dspinGrayHigh->value();
    m_Config.MatrixInfo.bUseROI = ui->checkUseMatrixROI->checkState();
    ui->widgetMatrixROI->getROI(m_Config.MatrixInfo.roi);

    m_Config.MatrixInfo.sDestinationPath = ui->editDestPath->text().toStdString();
    kipl::strings::filenames::CheckPathSlashes(m_Config.MatrixInfo.sDestinationPath,true);

    m_Config.MatrixInfo.FileType = static_cast<kipl::io::eFileType>(ui->comboDestFileType->currentIndex()+3);
    m_Config.MatrixInfo.sFileMask = ui->editSliceMask->text().toStdString();

    ptrdiff_t pos;

    switch (ui->comboDestFileType->currentIndex())
    {

    case 0: case 1:
        // Validity test of the slice file mask
        if (m_Config.MatrixInfo.sFileMask.find_last_of('.')==std::string::npos) {
            logger(logger.LogWarning,"Destination file mask is missing a file extension. Adding .tif");
            m_Config.MatrixInfo.sFileMask.append(".tif");
        }

        pos=m_Config.MatrixInfo.sFileMask.find_last_of('.');

        if (m_Config.MatrixInfo.sFileMask.substr(m_Config.MatrixInfo.sFileMask.find_last_of(".") + 1) == "hdf") {
            logger(logger.LogWarning,"Changing file extension to .tif");
            m_Config.MatrixInfo.sFileMask.replace(pos+1,3,"tif");
        }


        if (m_Config.MatrixInfo.sFileMask.find('#')==std::string::npos) {
            logger(logger.LogWarning,"Destination file mask is missing an index mask. Adding '_####'' before file extension");
            m_Config.MatrixInfo.sFileMask.insert(pos,"_####");
        }
        break;

    case 2: case 3:

        if (m_Config.MatrixInfo.sFileMask.find_last_of('.')==std::string::npos) {
            logger(logger.LogWarning,"Destination file mask is missing a file extension. Adding .hdf");
            m_Config.MatrixInfo.sFileMask.append(".hdf");
        }

        pos=m_Config.MatrixInfo.sFileMask.find_last_of('.');

        if (m_Config.MatrixInfo.sFileMask.substr(m_Config.MatrixInfo.sFileMask.find_last_of(".") + 1) == "tif") {
            logger(logger.LogWarning,"Changing file extension to .hdf");
             m_Config.MatrixInfo.sFileMask.replace(pos+1,3,"hdf");
        }
        break;

    }


    ui->editSliceMask->setText(QString::fromStdString(m_Config.MatrixInfo.sFileMask));
    // +2 to skip the matlab file types

    m_Config.UserInformation.sProjectNumber = ui->editProjectName->text().toStdString();
    m_Config.UserInformation.sOperator = ui->editOperatorName->text().toStdString();
    m_Config.UserInformation.sInstrument = ui->editInstrumentName->text().toStdString();
    m_Config.UserInformation.sSample = ui->editSampleDescription->text().toStdString();
    m_Config.UserInformation.sComment = ui->editExperimentDescription->toPlainText().toStdString();
    m_Config.backprojector = ui->ConfiguratorBackProj->GetModule();

    m_Config.ProjectionInfo.fSDD = ui->dspinSDD->value();
    m_Config.ProjectionInfo.fSOD = ui->dspinSOD->value();
    m_Config.ProjectionInfo.fpPoint[0] = ui->dspinPiercPointX->value();
    m_Config.ProjectionInfo.fpPoint[1] = ui->dspinPiercPointY->value();
    m_Config.ProjectionInfo.eDirection = static_cast<kipl::base::eRotationDirection>(ui->comboDirRotation->currentIndex());

    try {
        m_Config.SanityCheck();
    }
    catch (ReconException & e)
    {
        msg<<"Config sanity check failed in update config"<<std::endl<<e.what();
        logger(logger.LogError,msg.str());
        throw ReconException(msg.str(),__FILE__,__LINE__);
    }
    catch (kipl::base::KiplException & e)
    {
        msg<<"Config sanity check failed in update config"<<std::endl<<e.what();
        logger(logger.LogError,msg.str());
        throw kipl::base::KiplException(msg.str(),__FILE__,__LINE__);
    }

    try{
        m_Config.SanityAnglesCheck();
    }
    catch (ReconException & e)
    {
        msg<<"Config angle sanity check failed in update config"<<std::endl<<e.what();
        logger(logger.LogError,msg.str());
        throw ReconException(msg.str(),__FILE__,__LINE__);
    }
    catch (kipl::base::KiplException & e)
    {
        msg<<"Config angle sanity check failed in update config"<<std::endl<<e.what();
        logger(logger.LogError,msg.str());
        throw kipl::base::KiplException(msg.str(),__FILE__,__LINE__);
    }


}


void MuhRecMainWindow::on_buttonBrowseDC_clicked()
{
    std::ostringstream msg;
    std::string sPath, sFname;
    std::vector<std::string> exts;
    kipl::strings::filenames::StripFileName(ui->editProjectionMask->text().toStdString(),sPath,sFname,exts);
    QString projdir=QFileDialog::getOpenFileName(this,
                                      "Select a dark current file",
                                      QString::fromStdString(sPath));
    if (!projdir.isEmpty()) {
        std::string pdir=projdir.toStdString();

        kipl::io::DirAnalyzer da;
        kipl::io::FileItem fi=da.GetFileMask(pdir);

        if (fi.m_sExt=="hdf"){
            ui->editDarkMask->setText(projdir);
            ProjectionReader reader;
            size_t Nofimgs[2];
            reader.GetNexusInfo(projdir.toStdString(),Nofimgs, NULL);
            ui->spinFirstDark->setValue(Nofimgs[0]);
            ui->spinDarkCount->setValue(Nofimgs[1]+1);
        }
        else {
            ui->editDarkMask->setText(QString::fromStdString(fi.m_sMask));

            int c=0;
            int f=0;
            int l=0;

            da.AnalyzeMatchingNames(fi.m_sMask,c,f,l);

            msg<<"Found "<<c<<" files for mask "<<fi.m_sMask<<" in the interval "<<f<<" to "<<l;
            logger(logger.LogMessage,msg.str());

            ui->spinFirstDark->setValue(f);
            ui->spinDarkCount->setValue(c);
        }
    }
}

void MuhRecMainWindow::on_buttonGetPathDC_clicked()
{
//    std::ostringstream msg;
//    kipl::io::DirAnalyzer da;
//    kipl::io::FileItem fi=da.GetFileMask(ui->editProjectionMask->text().toStdString());

//    if (fi.m_sExt=="hdf"){
//        ui->editDarkMask->setText(ui->editProjectionMask->text());
//    }
//    else {
//        std::string darkname=fi.m_sPath+"dc_*."+fi.m_sExt;
//        std::vector<std::string> flist=da.GetDirList(darkname);
//        if (flist.size()==0) {
//            darkname=fi.m_sPath+"dark_*."+fi.m_sExt;
//            flist=da.GetDirList(darkname);
//        }

//        if (flist.size()!=0) {
//            int c=0;
//            int f=0;
//            int l=0;
//            fi=da.GetFileMask(flist.front());
//            da.AnalyzeMatchingNames(fi.m_sMask,c,f,l);

//            msg<<"Found "<<c<<" files for mask "<<fi.m_sMask<<" in the interval "<<f<<" to "<<l;
//            logger(logger.LogMessage,msg.str());

//            ui->editDarkMask->setText(QString::fromStdString(fi.m_sMask));
//            ui->spinFirstDark->setValue(f);
//            ui->spinDarkCount->setValue(c);
//        }
//        else {
//            ui->editDarkMask->setText(ui->editProjectionMask->text());
//        }
//    }


    ui->editDarkMask->setText(ui->editProjectionMask->text());
}

void MuhRecMainWindow::on_comboSlicePlane_activated(int index)
{
    std::ostringstream msg;
    m_eSlicePlane = static_cast<kipl::base::eImagePlanes>(1<<index);
    size_t dims[3];
    if (m_pEngine!=nullptr)
    {
        m_pEngine->GetMatrixDims(dims);
        int maxslices=static_cast<int>(dims[2-index]);
        ui->sliderSlices->setMaximum(maxslices-1);
        ui->sliderSlices->setValue(maxslices/2);

        msg<<"Changed slice plane to "<<m_eSlicePlane<<" max slices="<<maxslices;
        logger(kipl::logging::Logger::LogMessage,msg.str());

        on_sliderSlices_sliderMoved(maxslices/2);

        switch (m_eSlicePlane) {
            case kipl::base::ImagePlaneXY :
                m_nSliceSizeX=dims[0]-1;
                m_nSliceSizeY=dims[1]-1;
            break;
            case kipl::base::ImagePlaneXZ :
                m_nSliceSizeX=dims[0]-1;
                m_nSliceSizeY=dims[2]-1;
            break;
            case kipl::base::ImagePlaneYZ :
                m_nSliceSizeX=dims[1]-1;
                m_nSliceSizeY=dims[2]-1;
            break;
        }
    }
    else {
        logger(logger.LogWarning,"No engine allocated");
    }
}

void MuhRecMainWindow::on_actionPreferences_triggered()
{
    PreferencesDialog dlg;
    dlg.m_MemoryLimit = static_cast<int>(m_Config.System.nMemory);
    dlg.m_LogLevel    = m_Config.System.eLogLevel;


    int res=dlg.exec();

    if (res==dlg.Accepted) {

        m_Config.System.nMemory   = dlg.m_MemoryLimit;
        m_Config.System.eLogLevel = dlg.m_LogLevel;
    }

}

void MuhRecMainWindow::on_actionReconstruct_to_disk_triggered()
{
    if (reconstructToDisk()) {
      saveCurrentRecon();
      ExecuteReconstruction();
    }
}

bool MuhRecMainWindow::reconstructToDisk()
{
    std::ostringstream msg;
    logger(logger.LogMessage,"Starting reconstruct to disk");

    DialogTooBig largesize_dlg;
    UpdateConfig();

    logger(logger.LogMessage,msg.str());

    largesize_dlg.SetFields(ui->editDestPath->text(),
                                ui->editSliceMask->text(),
                                false,
                                m_Config.ProjectionInfo.roi[1], m_Config.ProjectionInfo.roi[3],
                                m_Config.ProjectionInfo.projection_roi[1],m_Config.ProjectionInfo.projection_roi[3]);

    int res=largesize_dlg.exec();

    if (res!=QDialog::Accepted) {
        logger(logger.LogMessage,"Reconstruction was aborted");
        return false;
    }

    m_Config.MatrixInfo.sDestinationPath = largesize_dlg.GetPath().toStdString();
    kipl::strings::filenames::CheckPathSlashes(m_Config.MatrixInfo.sDestinationPath,true);
    m_Config.MatrixInfo.sFileMask        = largesize_dlg.GetMask().toStdString();
    ui->editDestPath->setText(QString::fromStdString(m_Config.MatrixInfo.sDestinationPath));
    ui->editSliceMask->setText(QString::fromStdString(m_Config.MatrixInfo.sFileMask));

    ui->spinSlicesFirst->setValue(largesize_dlg.GetFirst());
    ui->spinSlicesLast->setValue(largesize_dlg.GetLast());


    UpdateConfig();

    msg.str("");
    msg<<"Reconstructing slices "<<m_Config.ProjectionInfo.roi[1]<<" to "<<m_Config.ProjectionInfo.roi[3]<<"direct to folder "<<m_Config.MatrixInfo.sDestinationPath
      <<" using the mask "<< m_Config.MatrixInfo.sFileMask;
    logger(logger.LogMessage,msg.str());

    m_Config.MatrixInfo.bAutomaticSerialize=true;

    return true;
}

void MuhRecMainWindow::on_spinSlicesFirst_valueChanged(int arg1)
{
    ui->spinSlicesLast->setMinimum(arg1+1);
    SlicesChanged(arg1);
}

void MuhRecMainWindow::on_spinSlicesLast_valueChanged(int arg1)
{
    ui->spinSlicesFirst->setMaximum(arg1-1);
    SlicesChanged(arg1);
}

void MuhRecMainWindow::SlicesChanged(int arg1)
{
    (void) arg1;

    QRect rect;
    size_t * dims=m_Config.ProjectionInfo.roi;

    size_t roi[4];
    ui->widgetProjectionROI->getROI(roi);

    rect.setCoords(dims[0]=roi[0],
                   dims[1]=ui->spinSlicesFirst->value(),
                   dims[2]=roi[2],
                   dims[3]=ui->spinSlicesLast->value());

    ui->projectionViewer->set_rectangle(rect,QColor("deepskyblue"),2); // TODO select a robust index, will be fixed with ROI manager.
    UpdateMemoryUsage(dims);
}

void MuhRecMainWindow::on_actionRemove_CurrentRecon_xml_triggered()
{
    ostringstream confpath;
    ostringstream msg;
    // Save current recon settings

    QDir dir;
    QString filename=dir.homePath()+"/.imagingtools/CurrentRecon.xml";

    std::string name=filename.toStdString();
    kipl::strings::filenames::CheckPathSlashes(name,false);
    filename=QString::fromStdString(name);

    if (dir.exists(filename)) {
        QDate date=QDate::currentDate();
        QString destname=filename+"."+date.toString("yyyyMMdd");
        dir.rename(filename,destname);
        msg.str("");
        msg<<"Moved "<<filename.toStdString()<<" to "<<destname.toStdString();
        logger(logger.LogMessage,msg.str());
    }
    else {
        msg.str("");
        msg<<"Couldn't find "<<filename.toStdString();
        logger(logger.LogMessage,msg.str());
    }

}

void MuhRecMainWindow::on_actionReport_a_bug_triggered()
{
    QUrl url=QUrl("https://github.com/neutronimaging/imagingsuite/issues");
    if (!QDesktopServices::openUrl(url)) {
 //       QMessageBox dlg("Could not open repository","MuhRec could not open your web browser with the link https://github.com/neutronimaging/tools/issues");
    }
}

void MuhRecMainWindow::on_checkCBCT_clicked(bool checked)
{

    if (checked) {
        m_Config.ProjectionInfo.beamgeometry = m_Config.ProjectionInfo.BeamGeometry_Cone;
        ComputeVolumeSizeSpacing();

        std::ostringstream msg;
        msg.str("");
        msg<<"Volume for CBCT: "<<m_Config.MatrixInfo.nDims[0]<<" "<<m_Config.MatrixInfo.nDims[1]<<" "<<m_Config.MatrixInfo.nDims[2]
          <<" voxels with isotropic spacing: "<< m_Config.MatrixInfo.fVoxelSize[0];
        logger(logger.LogMessage,msg.str());

        ui->groupBox_ConeBeamGeometry->show();
        SlicesChanged(0);

        // add here the message
        QMessageBox msgBox;
        msgBox.setWindowTitle("Cone Beam CT");
        msgBox.setText("Cone Beam CT reconstruction: \n - Tune the CB geometry in the Advanced geometry tab \n - in the Preprocessing module remove ProjectionFilterSingle. \n - in Back-projector configuration, add  FDKBackprojectors and then choose FDKbp (for double precision) or FDKbp_single (faster, for single precision). \n \n Enjoy!");
//        msgBox.setDetailedText(QString::fromStdString(msg.str()));
        msgBox.exec();

    } else {
        m_Config.ProjectionInfo.beamgeometry = m_Config.ProjectionInfo.BeamGeometry_Parallel;
        ui->groupBox_ConeBeamGeometry->hide();

        SlicesChanged(0);
    }
}

void MuhRecMainWindow::ComputeVolumeSizeSpacing()
{
    ComputeVolumeSize();
    ComputeVoxelSpacing();
}

void MuhRecMainWindow::ComputeVolumeSize()
{
    float magn = m_Config.ProjectionInfo.fSDD/m_Config.ProjectionInfo.fSOD;
    // compute volume dimensions
    m_Config.MatrixInfo.nDims[0] = (m_Config.ProjectionInfo.roi[2]-m_Config.ProjectionInfo.projection_roi[0])*m_Config.ProjectionInfo.fResolution[0]/magn/m_Config.MatrixInfo.fVoxelSize[0];
    m_Config.MatrixInfo.nDims[1] = m_Config.MatrixInfo.nDims[0];
    m_Config.MatrixInfo.nDims[2] = (m_Config.ProjectionInfo.roi[3]-m_Config.ProjectionInfo.projection_roi[1])*m_Config.ProjectionInfo.fResolution[0]/magn/m_Config.MatrixInfo.fVoxelSize[0];
}

void MuhRecMainWindow::ComputeVoxelSpacing()
{
    float magn = m_Config.ProjectionInfo.fSDD/m_Config.ProjectionInfo.fSOD;
    m_Config.MatrixInfo.fVoxelSize[0] = m_Config.MatrixInfo.fVoxelSize[1] = m_Config.MatrixInfo.fVoxelSize[2] = m_Config.ProjectionInfo.fResolution[0]/magn;
}

void MuhRecMainWindow::on_checkCBCT_stateChanged(int arg1)
{
    (void) arg1;
    // probably this one is not needed

    if (ui->checkCBCT->isChecked()) {
        m_Config.ProjectionInfo.beamgeometry = m_Config.ProjectionInfo.BeamGeometry_Cone;
        ComputeVolumeSizeSpacing();
        ui->groupBox_ConeBeamGeometry->show();
    }
    else {
        m_Config.ProjectionInfo.beamgeometry = m_Config.ProjectionInfo.BeamGeometry_Parallel;
        ui->groupBox_ConeBeamGeometry->hide();
    }

}

void MuhRecMainWindow::on_buttonGetPP_clicked()
{
    PiercingPointDialog dlg;
    UpdateConfig();

    int res=dlg.exec(m_Config);

    if (res==QDialog::Accepted) {
        pair<float,float> position=dlg.getPosition();

        m_Config.ProjectionInfo.fpPoint[0]=position.first;
        m_Config.ProjectionInfo.fpPoint[1]=position.second;
        UpdateDialog();
    }

}

void MuhRecMainWindow::on_dspinResolution_valueChanged(double arg1)
{
    (void)arg1;
    if (m_Config.ProjectionInfo.beamgeometry == m_Config.ProjectionInfo.BeamGeometry_Cone){
        m_Config.ProjectionInfo.fResolution[0]=m_Config.ProjectionInfo.fResolution[1]= ui->dspinResolution->value();
        ComputeVoxelSpacing();
        std::ostringstream msg;
        msg.str("");
        msg<<"Voxel spacing for CBCT updated: "<< m_Config.MatrixInfo.fVoxelSize[0]<< " =" << m_Config.ProjectionInfo.fResolution[0] << "/magnification";
        logger(logger.LogMessage,msg.str());
    }
}




//void MuhRecMainWindow::on_spinSubVolumeSizeZ0_valueChanged(int arg1)
//{
//    SlicesCBCTChanged(arg1);
//}

//void MuhRecMainWindow::on_spinSubVolumeSizeZ1_valueChanged(int arg1)
//{
//    SlicesCBCTChanged(arg1);
//}

//void MuhRecMainWindow::SlicesCBCTChanged(int x)
//{
//    QRect rect;
//    rect.setCoords(ui->spinProjROIx0->value(),
//                   ui->spinSubVolumeSizeZ0->value(),
//                   ui->spinProjROIx1->value(),
//                   ui->spinSubVolumeSizeZ1->value());

//    ui->projectionViewer->set_rectangle(rect,QColor("lightblue"),2);
//}

void MuhRecMainWindow::on_actionRegister_for_news_letter_triggered()
{
    QUrl url=QUrl("http://www.imagingscience.ch/newsletter/");
    if (!QDesktopServices::openUrl(url)) {
        QMessageBox dlg;
        dlg.setText("MuhRec could not open your web browser with the link http://www.imagingscience.ch/newsletter/");
        dlg.exec();
    }

}

void MuhRecMainWindow::on_sliderProjections_sliderMoved(int position)
{
    bool fail=false;
    std::ostringstream msg;

    try {
        PreviewProjection(position);
    }
    catch (ReconException &e) {
        fail=true;
        msg<<e.what();
    }
    catch (kipl::base::KiplException &e) {
        fail=true;
        msg<<e.what();
    }

    if (fail) {
        QMessageBox dlg;
        dlg.setText("Failed to show projection");
        dlg.setDetailedText(QString::fromStdString(msg.str()));
        dlg.exec();
    }
}

void MuhRecMainWindow::on_spinFirstProjection_valueChanged(int arg1)
{
    std::ostringstream msg;
    if (ui->spinFirstProjection->text().isEmpty())
        return;

//    QFile::exists(QString::fromStdString(name));
    int last=ui->spinLastProjection->value();
    if (last<arg1) {
        msg<<"First projection greater than last ("<<last<<"<"<<arg1<<")";
        qDebug()<<QString::fromStdString(msg.str());
        logger(logger.LogWarning,msg.str());
    }
    else {
        ProjectionIndexChanged(0);
    }
}

void MuhRecMainWindow::on_spinLastProjection_valueChanged(int arg1)
{
    std::ostringstream msg;

    if (ui->spinLastProjection->text().isEmpty())
        return;

    int first=ui->spinFirstProjection->value();

    if (arg1<first) {
        msg<<"Last projection is less than first ("<<arg1<<"<"<<first<<")";
        qDebug()<<QString::fromStdString(msg.str());
        logger(logger.LogWarning,msg.str());
    }
    else {
        ProjectionIndexChanged(0);
    }
}

void MuhRecMainWindow::on_comboFlipProjection_currentIndexChanged(int index)
{
    (void) index;
    PreviewProjection();
}

void MuhRecMainWindow::on_comboRotateProjection_currentIndexChanged(int index)
{
    (void) index;
    PreviewProjection();
}

void MuhRecMainWindow::on_buttonPreview_clicked()
{
    PreviewProjection();
}

void MuhRecMainWindow::on_pushButton_logging_clicked()
{
    if (logdlg->isHidden()) {

        logdlg->show();
    }
    else {
        logdlg->hide();
    }
}

void MuhRecMainWindow::on_actionUser_manual_triggered()
{
    QUrl url=QUrl("https://github.com/neutronimaging/imagingsuite/wiki/User-manual-MuhRec");
    if (!QDesktopServices::openUrl(url)) {
        QMessageBox dlg;
        dlg.setText("MuhRec could not open your web browser with the link https://github.com/neutronimaging/imagingsuite/wiki/User-manual-MuhRec");
        dlg.exec();
    }

}

void MuhRecMainWindow::on_actionVideo_tutorials_triggered()
{
    QUrl url=QUrl("https://github.com/neutronimaging/imagingsuite/wiki/Tutorial-videos");
    if (!QDesktopServices::openUrl(url)) {
        QMessageBox dlg;
        dlg.setText("MuhRec could not open your web browser with the link https://github.com/neutronimaging/imagingsuite/wiki/User-manual-MuhRec");
        dlg.exec();
    }

}

void MuhRecMainWindow::on_actionShow_repository_triggered()
{
    QUrl url=QUrl("https://github.com/neutronimaging/imagingsuite"
                  "");
    if (!QDesktopServices::openUrl(url)) {
        QMessageBox dlg;
        dlg.setText("MuhRec could not open your web browser with the link https://github.com/neutronimaging/imagingsuite/wiki/User-manual-MuhRec");
        dlg.exec();
    }
}

void MuhRecMainWindow::on_radioButton_fullTurn_clicked()
{
    ui->dspinAngleStart->setValue(0.0);
    ui->dspinAngleStart->setEnabled(false);

    ui->dspinAngleStop->setValue(360.0);
    ui->dspinAngleStop->setEnabled(false);
    repaint();
}

void MuhRecMainWindow::on_radioButton_halfTurn1_clicked()
{
    ui->dspinAngleStart->setValue(0.0);
    ui->dspinAngleStart->setEnabled(false);

    ui->dspinAngleStop->setValue(180.0);
    ui->dspinAngleStop->setEnabled(false);
    repaint();
}

void MuhRecMainWindow::on_radioButton_halfTurn2_clicked()
{
    ui->dspinAngleStart->setValue(180.0);
    ui->dspinAngleStart->setEnabled(false);

    ui->dspinAngleStop->setValue(360.0);
    ui->dspinAngleStop->setEnabled(false);
    repaint();
}

void MuhRecMainWindow::on_radioButton_customTurn_clicked()
{
    ui->dspinAngleStart->setEnabled(true);
    ui->dspinAngleStop->setEnabled(true);

    repaint();

}

void MuhRecMainWindow::on_widgetProjectionROI_valueChanged(int x0, int y0, int x1, int y1)
{
  //  qDebug()<<x0<<", "<<y0<<", "<<x1<<", "<<y1;
    int oldFirstSlice=ui->spinSlicesFirst->value();
    int oldLastSlice=ui->spinSlicesLast->value();
    int sliceDiff=oldLastSlice-oldFirstSlice;

    ui->spinSlicesFirst->setMinimum(y0);
    ui->spinSlicesLast->setMaximum(y1);

    if (oldLastSlice<y0) {
        ui->spinSlicesLast->setValue(y0+sliceDiff);
    }

    on_spinSlicesFirst_valueChanged(y0);

    if (y1<oldFirstSlice) {
        ui->spinSlicesFirst->setValue(y1-sliceDiff);
    }
    on_spinSlicesLast_valueChanged(y1);

    ui->dspinRotationCenter->setValue(ui->dspinRotationCenter->value()-(x0-m_oldROI[0]));
    CenterOfRotationChanged();
    m_oldROI[0]=x0; m_oldROI[1]=y0; m_oldROI[2]=x1; m_oldROI[3]=y1;

    size_t roi[4];
    ui->widgetProjectionROI->getROI(roi);
    UpdateMemoryUsage(roi);

    if (ui->checkCBCT->isChecked()){
        ComputeVolumeSize(); // update the size of the output volume
    }
}

void MuhRecMainWindow::on_buttonProjectionPath_clicked()
{
    ostringstream msg;
    QString oldmask=ui->editProjectionMask->text();
    QString newmask;

    QString projdir=QFileDialog::getOpenFileName(this,
                                      tr("Select location of the projections"),
                                      ui->editProjectionMask->text());


    if (!projdir.isEmpty()) {

        kipl::io::DirAnalyzer da;
        kipl::io::FileItem fi = da.GetFileMask(projdir.toStdString());

        int c=0;
        int f=0;
        int l=0;

        if (fi.m_sExt!="hdf") {
            std::string pdir=projdir.toStdString();

            kipl::io::DirAnalyzer da;
            fi=da.GetFileMask(pdir);
            newmask=QString::fromStdString(fi.m_sMask);

            if ((fi.m_sExt=="txt") || (fi.m_sExt=="csv")) {
                da.AnalyzeFileList(fi.m_sMask,c);
                f=1;
                l=c;
            }
            else
                da.AnalyzeMatchingNames(fi.m_sMask,c,f,l);

            msg<<"Found "<<c<<" files for mask "<<fi.m_sMask<<" in the interval "<<f<<" to "<<l;
            logger(logger.LogMessage,msg.str());
        }
        else {
            newmask=projdir;
            double angles[2];
            size_t Nofimgs[2];
            ProjectionReader reader;
            reader.GetNexusInfo(projdir.toStdString(),Nofimgs,angles);
            f=static_cast<int>(Nofimgs[0]);
            l=static_cast<int>(Nofimgs[1]);
            ui->dspinAngleStart->setValue(angles[0]);
            ui->dspinAngleStop->setValue(angles[1]);
        }


        QSignalBlocker spinFirst(ui->spinFirstProjection);
//        ui->spinFirstProjection->setMaximum(l);
//        ui->spinFirstProjection->setMinimum(f);
        ui->spinFirstProjection->setValue(f);

        QSignalBlocker spinLast(ui->spinLastProjection);
//        ui->spinLastProjection->setMaximum(l);
//        ui->spinLastProjection->setMinimum(f);
        ui->spinLastProjection->setValue(l);

        ProjectionIndexChanged(0);
        if (oldmask!=newmask)
        {
            ui->editProjectionMask->setText(newmask);

            PreviewProjection(-1);
            ui->projectionViewer->clear_rectangle(-1);
            ui->projectionViewer->clear_plot(-1);
        }

        lookForReferences(fi.m_sPath);
    }
}

void MuhRecMainWindow::on_buttonBrowseDestinationPath_clicked()
{
    QString projdir=QFileDialog::getExistingDirectory(this,
                                      "Select location the reconstructed slices",
                                      ui->editDestPath->text());

    if (!projdir.isEmpty())
        ui->editDestPath->setText(projdir);
}

void MuhRecMainWindow::on_buttonTakePath_clicked()
{
      ui->editOpenBeamMask->setText(ui->editProjectionMask->text());
      repaint();
}

void MuhRecMainWindow::on_buttonGetSkipList_clicked()
{
    UpdateConfig();

    FindSkipListDialog dlg;

    int res=dlg.exec(m_Config);

    if (res==QDialog::Accepted)
        ui->editProjectionSkipList->setText(dlg.getSkipList());
}

void MuhRecMainWindow::on_dspinRotationCenter_valueChanged(double arg1)
{
    (void) arg1;
    CenterOfRotationChanged();
}

void MuhRecMainWindow::on_dspinTiltAngle_valueChanged(double arg1)
{
    (void) arg1;
    CenterOfRotationChanged();
}

void MuhRecMainWindow::on_dspinTiltPivot_valueChanged(double arg1)
{
    (void) arg1;
    CenterOfRotationChanged();
}

void MuhRecMainWindow::on_checkCorrectTilt_clicked(bool checked)
{
    if (checked) {
        ui->dspinTiltAngle->show();
        ui->dspinTiltPivot->show();
        ui->label_tiltAngle->show();
        ui->label_tiltPivot->show();
    }
    else {
        ui->dspinTiltAngle->hide();
        ui->dspinTiltPivot->hide();
        ui->label_tiltAngle->hide();
        ui->label_tiltPivot->hide();
    }

    CenterOfRotationChanged();
}

void MuhRecMainWindow::on_sliderSlices_sliderMoved(int position)
{
    QSignalBlocker blockSlider(ui->sliderSlices);
    if (m_pEngine==nullptr)
        return;

    std::ostringstream msg;
    int nSelectedSlice=position;

    if (position<0) {
        nSelectedSlice=m_Config.MatrixInfo.nDims[2]/2;
        ui->sliderSlices->setValue(nSelectedSlice);
    }

    try {
        kipl::base::TImage<float,2> slice=m_pEngine->GetSlice(nSelectedSlice,m_eSlicePlane);
        ui->sliceViewer->set_image(slice.GetDataPtr(),slice.Dims(),
                                   static_cast<float>(ui->dspinGrayLow->value()),
                                   static_cast<float>(ui->dspinGrayHigh->value()));
        msg.str("");
        msg<<position<<" ("<<position+m_Config.ProjectionInfo.roi[1]<<")";
        ui->label_sliceindex->setText(QString::fromStdString(msg.str()));
        // TODO Add line to indicate location of slice (XY-slices)
    }
    catch (kipl::base::KiplException &e) {
        msg.str("");
        msg<<"Failed to display slice \n"<<e.what();
        logger(kipl::logging::Logger::LogMessage,msg.str());
    }
}

void MuhRecMainWindow::on_button_FindCenter_clicked()
{
    std::ostringstream msg;
    int res;
    ConfigureGeometryDialog dlg;
    UpdateConfig();
    res=dlg.exec(m_Config);


    if (res==QDialog::Accepted)
    {
        dlg.GetConfig(m_Config);
        UpdateDialog();
        UpdateMemoryUsage(m_Config.ProjectionInfo.roi);
    }

}

void MuhRecMainWindow::on_checkUseMatrixROI_toggled(bool checked)
{
    ui->widgetMatrixROI->setVisible(checked);
}

void MuhRecMainWindow::on_dspinRotateRecon_valueChanged(double arg1)
{
    QSignalBlocker blockDial(ui->dialRotateRecon);

    ui->dialRotateRecon->setValue(int(arg1*10.0));

}

void MuhRecMainWindow::on_dialRotateRecon_sliderMoved(int position)
{
    QSignalBlocker blockSpin(ui->dspinRotateRecon);

    ui->dspinRotateRecon->setValue(double(position)/10.0);

}


void MuhRecMainWindow::on_buttonSaveMatrix_clicked()
{
    UpdateConfig();
    std::ostringstream msg;
    if (m_pEngine!=nullptr) {
        try {

            m_pEngine->Serialize(&m_Config.MatrixInfo);
            std::string fname=m_Config.MatrixInfo.sDestinationPath+"ReconConfig.xml";
            kipl::strings::filenames::CheckPathSlashes(fname,false);
            std::ofstream configfile(fname.c_str());
            configfile<<m_Config.WriteXML();
            configfile.close();

//			int repdims[2]={595,842};
//			ReconReport report(repdims);

//			const int nBins=256;
//			float axis[nBins];
//			size_t hist[nBins];
//			engine->GetHistogram(axis,hist,nBins);
//			kipl::base::TImage<float,2> xy;
//			size_t dims[3];
//			engine->GetMatrixDims(dims);
//			xy=engine->GetSlice(dims[2]/2);
//			ostringstream reportname;
//			reportname<<config.MatrixInfo.sDestinationPath;
//			if (!config_filename.empty()) {
//				std::string path;
//				std::string name;
//				std::vector<std::string> extensions;
//				kipl::strings::filenames::StripFileName(config_filename,
//					path,name,extensions);
//				reportname<<name<<".pdf";
//			}
//			else
//				reportname<<"reconstruction_report.pdf";

//			report.CreateReport(reportname.str(),&config,&xy,&xy,&xy,hist,axis,nBins);
        }
        catch (ReconException &e) {
            msg<<"A recon exception occurred "<<e.what();
        }
        catch (kipl::base::KiplException &e) {
            msg<<"A kipl exception occurred "<<e.what();
        }
        catch (std::exception &e) {
            msg<<"A STL exception occurred "<<e.what();
        }
        catch (...) {
            msg<<"An unknown exception occurred ";
        }
        if (!msg.str().empty()) {
            QMessageBox msgdlg;

            msgdlg.setWindowTitle("Error");
            msgdlg.setText("Failed to save the reconstructed slices");
            msgdlg.setDetailedText(QString::fromStdString(msg.str()));
            msgdlg.exec();

            logger(kipl::logging::Logger::LogError,msg.str());
        }
    }
    else {
        logger(kipl::logging::Logger::LogWarning,"There is no matrix to save yet.");
        QMessageBox msgdlg;

        msgdlg.setWindowTitle("Error");
        msgdlg.setText("There is no matrix to save yet.");

        msgdlg.exec();
    }
}

void MuhRecMainWindow::on_dspinGrayLow_valueChanged(double low)
{
    QSignalBlocker blockViewer(ui->sliceViewer);
    double high=ui->dspinGrayHigh->value();

    ui->sliceViewer->set_levels(low,high);
    ui->plotHistogram->setPlotCursor(0,QtAddons::PlotCursor(low,QColor("green"),QtAddons::PlotCursor::Vertical));
}

void MuhRecMainWindow::on_dspinGrayHigh_valueChanged(double high)
{
    QSignalBlocker blockViewer(ui->sliceViewer);
    double low=ui->dspinGrayLow->value();

    ui->sliceViewer->set_levels(low,high);
    ui->plotHistogram->setPlotCursor(1,QtAddons::PlotCursor(high,QColor("green"),QtAddons::PlotCursor::Vertical));

}

void MuhRecMainWindow::on_sliceViewer_levelsChanged(float low, float high)
{
    qDebug()<<"Main::levelsChanged";
    QSignalBlocker blockLow(ui->dspinGrayLow);
    QSignalBlocker blockHigh(ui->dspinGrayHigh);

    ui->dspinGrayLow->setValue(low);
    ui->dspinGrayHigh->setValue(high);

    ui->plotHistogram->setPlotCursor(0,QtAddons::PlotCursor(low,QColor("green"),QtAddons::PlotCursor::Vertical));
    ui->plotHistogram->setPlotCursor(1,QtAddons::PlotCursor(high,QColor("green"),QtAddons::PlotCursor::Vertical));
}

void MuhRecMainWindow::on_pushButton_levels95p_clicked()
{
    if (m_pEngine!=nullptr) {
        const int nBins=256;
        float x[nBins];
        size_t y[nBins];
        m_pEngine->GetHistogram(x,y,nBins);

        size_t lowlevel=0;
        size_t highlevel=0;
        kipl::base::FindLimits(y, nBins, 95.0, &lowlevel, &highlevel);
        ui->dspinGrayLow->setValue(x[lowlevel]);
        ui->dspinGrayHigh->setValue(x[highlevel]);
    }
    else
        logger(logger.LogMessage,"Level 95\%: Missing engine");
}

void MuhRecMainWindow::on_pushButton_levels99p_clicked()
{
    if (m_pEngine!=nullptr) {
        const int nBins=256;
        float x[nBins];
        size_t y[nBins];
        m_pEngine->GetHistogram(x,y,nBins);

        size_t lowlevel=0;
        size_t highlevel=0;
        kipl::base::FindLimits(y, nBins, 99.0, &lowlevel, &highlevel);
        ui->dspinGrayLow->setValue(x[lowlevel]);
        ui->dspinGrayHigh->setValue(x[highlevel]);
    }
    else
        logger(logger.LogMessage,"Level 99\%: Missing engine");
}


void MuhRecMainWindow::on_checkBoxUseSkipList_toggled(bool checked)
{
    ui->buttonGetSkipList->setVisible(checked);
    ui->editProjectionSkipList->setVisible(checked);
}

void MuhRecMainWindow::on_pushButtonGetSliceROI_clicked()
{
    QRect roi=ui->projectionViewer->get_marked_roi();
    int first=roi.top();
    int last=roi.bottom();

    size_t projROI[4];
    ui->widgetProjectionROI->getROI(projROI);

    if (first<projROI[1]) {
        first=projROI[1];
        if (last<projROI[1]) {
            QMessageBox::warning(this,"Invalid interval selected","Selected ROI is outside the marked projection.");
            return;
        }
        if (projROI[3]<last) {
            logger(logger.LogMessage,"Generous ROI selected, restricting to projection interval.");
            last=projROI[3];
        }
    }

    if (projROI[3]<last) {
        last=projROI[3];
        if (projROI[3]<first) {
            QMessageBox::warning(this,"Invalid interval selected","Selected ROI is outside the marked projection.");
            return;
        }
    }
    QSignalBlocker b1(ui->spinSlicesFirst);
    QSignalBlocker b2(ui->spinSlicesLast);

    ui->spinSlicesFirst->setValue(first);
    ui->spinSlicesLast->setValue(last);

    SlicesChanged(0);
}

void MuhRecMainWindow::on_comboDataSequence_currentIndexChanged(int index)
{
    if (index==m_Config.ProjectionInfo.GoldenSectionScan)
    {
        if  (ui->radioButton_customTurn->isChecked()) {
            ui->radioButton_customTurn->setCheckable(false);
            ui->radioButton_halfTurn1->setChecked(true); // default value
            on_radioButton_halfTurn1_clicked();
        }

        ui->radioButton_customTurn->setCheckable(false);
    }
    else
    {
        ui->radioButton_customTurn->setCheckable(true);
    }
}

