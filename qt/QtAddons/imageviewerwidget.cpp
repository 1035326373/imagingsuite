#include "imageviewerwidget.h"
#include <QStylePainter>
#include <sstream>
#include <cmath>
#include <QMenu>
#include <QToolTip>


namespace QtAddons {

ImageViewerWidget::ImageViewerWidget(QWidget *parent) :
    QWidget(parent),
    logger("ImageViewerWidget"),
    m_ImagePainter(this),
    m_RubberBandStatus(RubberBandHide),
    m_PressedButton(Qt::NoButton),
    m_MouseMode(ViewerROI)
{
    QPalette palette;
    palette.setColor(QPalette::Background,Qt::black);
    setAutoFillBackground(true);
    setPalette(palette);
    setFocusPolicy(Qt::ClickFocus);
    setCursor(Qt::CrossCursor);
    this->setMouseTracking(true);
//    setContextMenuPolicy(Qt::CustomContextMenu);
//    connect(this, SIGNAL(customContextMenuRequested(const QPoint&)),
//        this, SLOT(ShowContextMenu(const QPoint&)));
}

//ImageViewerWidget::~ImageViewerWidget()
//{
//}

QSize ImageViewerWidget::minimumSizeHint() const
{
    return QSize(6 * Margin, 4 * Margin);
}

QSize ImageViewerWidget::sizeHint() const
{
    return QSize(12 * Margin, 8 * Margin);
}

void ImageViewerWidget::ShowContextMenu(const QPoint& pos) // this is a slot
{
    logger(kipl::logging::Logger::LogMessage,"Context menu");
    // for most widgets
    QPoint globalPos = this->mapToGlobal(pos);
    // for QAbstractScrollArea and derived classes you would use:
    // QPoint globalPos = myWidget->viewport()->mapToGlobal(pos);

    QMenu viewerMenu;
    viewerMenu.addAction("Full Image");
    viewerMenu.addAction("Set Levels");
    // ...

    QAction* selectedItem = viewerMenu.exec(globalPos);
    if (selectedItem)
    {
        if (selectedItem->text() == "Full Image") {
            logger(kipl::logging::Logger::LogMessage,"Full Image");
        }
        if (selectedItem->text() == "Set Levels") {
            logger(kipl::logging::Logger::LogMessage,"Set Levels");
        }
    }
    else
    {
        // nothing was chosen
        logger(kipl::logging::Logger::LogMessage,"Menu was canceled");
    }
}

void ImageViewerWidget::paintEvent(QPaintEvent * /* event */)
{

    QPainter painter(this);
    QSize s=this->size();

    m_ImagePainter.Render(painter,0,0,s.width(),s.height());

    if (m_RubberBandStatus != RubberBandHide) {
        painter.setPen(palette().light().color());
        painter.setPen(Qt::PenStyle()
        painter.drawRect(rubberBandRect.normalized()
                                       .adjusted(0, 0, -1, -1));
    }
}

void ImageViewerWidget::resizeEvent(QResizeEvent *event )
{
    widgetSize = event->size();
    // Call base class impl
    QWidget::resizeEvent(event);
}

void ImageViewerWidget::keyPressEvent(QKeyEvent *event)
{
    if (!this->hasFocus())
        return;
    char keyvalue=event->key();

    switch (keyvalue) {
    case 'm':
    case 'M':
//        logger(kipl::logging::Logger::LogMessage,"got m");
//        //ShowContextMenu(this->mouseGrabber()->pos());
//        ShowContextMenu(QPoint(100,100));
        break;
//    case 'z':
//    case 'Z':
//        setCursor(Qt::SizeBDiagCursor);
//        m_MouseMode=ViewerZoom;
//        break;
    case 'p':
    case 'P':
        setCursor(Qt::OpenHandCursor);
        m_MouseMode=ViewerPan;
        break;
    case 'l':
    case 'L': {
            SetGrayLevelsDialog dlg(this);
            dlg.exec();
            break;
        }
    case 'r':
    case 'R':
        setCursor(Qt::CrossCursor);
        m_MouseMode=ViewerROI;
        break;
    case '+':
        //m_ImagePainter.ZoomIn();
        break;
    case '-':
        //m_ImagePainter.ZoomOut();
        break;
    }
}

void ImageViewerWidget::enterEvent(QEvent *)
{
  // logger(kipl::logging::Logger::LogMessage,"Entered");

}

void ImageViewerWidget::mousePressEvent(QMouseEvent *event)
{

    QRect rect(Margin, Margin,
               width() - 2 * Margin, height() - 2 * Margin);

    if (m_RubberBandStatus != RubberBandHide) {
        m_RubberBandStatus = RubberBandHide;
        updateRubberBandRegion();
    }

    if (event->button() == Qt::LeftButton) {
        if (m_MouseMode==ViewerROI)
            if (rect.contains(event->pos())) {
                m_RubberBandStatus = RubberBandDrag;
                rubberBandRect.setTopLeft(event->pos());
                rubberBandRect.setBottomRight(event->pos());
                updateRubberBandRegion();
                setCursor(Qt::CrossCursor);
            }
//        if (m_MouseMode==ViewerPan)
//            m_ImagePainter.pan(event->pos());
    }
    if (event->button() == Qt::RightButton) {
        m_PressedButton=event->button();
        m_LastMotionPosition=event->pos();
    }


   QWidget::mousePressEvent(event);
}

void ImageViewerWidget::mouseMoveEvent(QMouseEvent *event)
{

    std::ostringstream msg;

    if (m_RubberBandStatus == RubberBandDrag) {
        updateRubberBandRegion();
        rubberBandRect.setBottomRight(event->pos());
        updateRubberBandRegion();
    }
    else {

    }

    if (m_PressedButton == Qt::RightButton) {
        float minlevel, maxlevel;

        get_levels(&minlevel, &maxlevel);

        float fWindow=maxlevel-minlevel;
        float fLevel=minlevel+fWindow/2.0f;

        float fLevelStep  = fWindow/1000.0f;
        float fWindowStep = fWindow/1000.0f;
        msg.str("");

        int dx=static_cast<int>(event->pos().x() - m_LastMotionPosition.x());
        int dy=static_cast<int>(event->pos().y() - m_LastMotionPosition.y());
        m_LastMotionPosition=event->pos();
        if (abs(dx)<abs(dy))
            fLevel+=dy*fLevelStep;
        else
            fWindow+=dx*fWindowStep;

        msg<<"W="<<fWindow<<", L="<<fLevel;

        showToolTip(event->pos(),QString::fromStdString(msg.str()));
        set_levels(fLevel-fWindow/2.0f,fLevel+fWindow/2.0f);
    }
    else {
        int const * const dims=m_ImagePainter.get_image_dims();

        int xpos=static_cast<int>((event->pos().x()-m_ImagePainter.get_offsetX())/m_ImagePainter.get_scale());
        int ypos=static_cast<int>((event->pos().y()-m_ImagePainter.get_offsetY())/m_ImagePainter.get_scale());

        if ((0<=xpos) && (0<=ypos) && (xpos<dims[0]) && (ypos<dims[1])) {
            msg.str("");
            msg<<m_ImagePainter.getValue(xpos,ypos)<<" @ ("<<xpos<<", "<<ypos<<")";

            showToolTip(event->globalPos(),QString::fromStdString(msg.str()));
        }
    }

    QWidget::mouseMoveEvent(event);
}

void ImageViewerWidget::mouseReleaseEvent(QMouseEvent *event)
{
    std::ostringstream msg;
    if ((event->button() == Qt::LeftButton) && (m_RubberBandStatus == RubberBandDrag)) {
        updateRubberBandRegion();

        QRect r=rubberBandRect.normalized();
        roiRect.setRect(floor((r.x()-m_ImagePainter.get_offsetX())/m_ImagePainter.get_scale()),
                        floor((r.y()-m_ImagePainter.get_offsetY())/m_ImagePainter.get_scale()),
                        floor(r.width()/m_ImagePainter.get_scale()),
                        floor(r.height()/m_ImagePainter.get_scale())
                        );
        m_RubberBandStatus = RubberBandFreeze;

    }

    if (event->button() == Qt::RightButton )
        m_PressedButton = Qt::NoButton;


    QWidget::mouseReleaseEvent(event);
}

void ImageViewerWidget::updateRubberBandRegion()
{
    QRect rect = rubberBandRect.normalized();

    update(rect.left(), rect.top(), rect.width(), 1);
    update(rect.left(), rect.top(), 1, rect.height());
    update(rect.left(), rect.bottom(), rect.width(), 1);
    update(rect.right(), rect.top(), 1, rect.height());
}

void ImageViewerWidget::showToolTip(QPoint position, QString message)
{
    QFont ttfont=this->font();
    ttfont.setPointSize(static_cast<int>(ttfont.pointSize()*0.9));
    QPalette color;
    int setval = 0;

    color.setColor( QPalette::Active,QPalette::QPalette::ToolTipBase,Qt::yellow);
    qreal x = position.x()+this->pos().x()+5;
    qreal y = position.y()+this->pos().y()-15;
    QPoint ttpos (x, y);

    QToolTip::setPalette(color);
    QToolTip::setFont(ttfont);

    QToolTip::showText(ttpos, message,this);
}

void ImageViewerWidget::set_image(float const * const data, size_t const * const dims)
{
    std::ostringstream msg;
    m_ImagePainter.set_image(data,dims);
    float mi,ma;
    m_ImagePainter.get_image_minmax(&mi,&ma);
//    msg<<"Set image with min="<<mi<<", max="<<ma;
//    logger(kipl::logging::Logger::LogMessage,msg.str());
}

QRect ImageViewerWidget::get_marked_roi()
{
    m_RubberBandStatus = RubberBandHide;
    updateRubberBandRegion();
    return roiRect;
}

void ImageViewerWidget::set_image(float const * const data, size_t const * const dims, const float low, const float high)
{
    m_ImagePainter.set_image(data,dims,low,high);
}

void ImageViewerWidget::set_plot(QVector<QPointF> data, QColor color, int idx)
{
    m_ImagePainter.set_plot(data,color,idx);
}

void ImageViewerWidget::clear_plot(int idx)
{
    m_ImagePainter.clear_plot(idx);
}

void ImageViewerWidget::set_rectangle(QRect rect, QColor color, int idx)
{
    m_ImagePainter.set_rectangle(rect,color,idx);
}

void ImageViewerWidget::clear_rectangle(int idx)
{
    m_ImagePainter.clear_rectangle(idx);
}

void ImageViewerWidget::hold_annotations(bool hold)
{
    m_ImagePainter.hold_annotations(hold);
}

void ImageViewerWidget::clear_viewer()
{
    m_ImagePainter.clear();
}

void ImageViewerWidget::set_levels(const float level_low, const float level_high)
{
    m_ImagePainter.set_levels(level_low,level_high);
}

void ImageViewerWidget::get_levels(float *level_low, float *level_high)
{
    m_ImagePainter.get_levels(level_low,level_high);
}

void ImageViewerWidget::get_minmax(float *level_low, float *level_high)
{
    m_ImagePainter.get_image_minmax(level_low,level_high);
}

void ImageViewerWidget::show_clamped(bool show)
{
    m_ImagePainter.show_clamped(show);
}

const QVector<QPointF> & ImageViewerWidget::get_histogram()
{
    return m_ImagePainter.get_image_histogram();
}

SetGrayLevelsDialog::SetGrayLevelsDialog(QWidget *parent) :
    QDialog(parent),
    logger("SetGrayLevelsDialog"),
    m_label1("Low value"),
    m_label2("High value"),
    m_pParent(dynamic_cast<ImageViewerWidget*>(parent))

{

    m_HorizontalLayout.insertWidget(0,&m_label1);
    m_HorizontalLayout.insertWidget(1,&m_spinLow);
    m_HorizontalLayout.insertWidget(2,&m_label2);
    m_HorizontalLayout.insertWidget(3,&m_spinHigh);
    m_VerticalLayout.insertWidget(0,&m_Plotter);
    m_VerticalLayout.insertLayout(1,&m_HorizontalLayout);
    this->setLayout(&m_VerticalLayout);
    this->setWindowTitle(tr("Set viewer gray levels"));
    float fMin;
    float fMax;
    m_pParent->get_minmax(&fMin,&fMax);
    double step=pow(10.0,floor(log10(fabs(fMax-fMin))-2.0));
    m_spinLow.setMaximum(fMax);
    m_spinLow.setMinimum(fMin);
    m_spinLow.setSingleStep(step);
    m_spinHigh.setMaximum(fMax);
    m_spinHigh.setMinimum(fMin);
    m_spinHigh.setSingleStep(step);
    m_pParent->get_levels(&fMin,&fMax);
    m_spinLow.setValue(fMin);
    m_spinHigh.setValue(fMax);
    QVector<QPointF> hist=m_pParent->get_histogram();
    m_Plotter.setCurveData(0,hist);
    GrayLevelsChanged(0.0);

    connect(&m_spinLow,SIGNAL(valueChanged(double)),this,SLOT(GrayLevelsChanged(double)));
    connect(&m_spinHigh,SIGNAL(valueChanged(double)),this,SLOT(GrayLevelsChanged(double)));
}

void SetGrayLevelsDialog::GrayLevelsChanged(double x)
{
    double low=m_spinLow.value();
    double high=m_spinHigh.value();
    m_pParent->set_levels(low,high);

    m_Plotter.setPlotCursor(0,PlotCursor(low,QColor("green"),PlotCursor::Vertical));
    m_Plotter.setPlotCursor(1,PlotCursor(high,QColor("blue"),PlotCursor::Vertical));

}

}
