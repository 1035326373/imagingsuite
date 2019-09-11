//<LICENCE>

#include <iostream>
#include <sstream>
#include <cstring>
#include <algorithm>
#include <numeric>

#include "../../include/morphology/pixeliterator.h"
#include "../../include/base/KiplException.h"
#include <cstring>


namespace kipl {
namespace base {

PixelIterator::PixelIterator(const size_t *dims, kipl::base::eConnectivity conn) :
    logger("PixelIterator"),
    m_imageSize(std::accumulate(dims, dims+kipl::base::getConnectivityDims(conn), 1, std::multiplies<int>())),
    m_ndims(kipl::base::getConnectivityDims(conn)),
    m_sx(static_cast<ptrdiff_t>(dims[0])),
    m_sxy(static_cast<ptrdiff_t>(dims[0]*dims[1])),
    m_connectivity(conn),
    m_currentPosition(0L),
    m_currentX(0),
    m_currentY(0),
    m_currentZ(0),
    m_rowStart(0),
    m_rowEnd(m_sx-1)
{
    m_dims.resize(kipl::base::getConnectivityDims(conn));

    std::copy_n(dims,kipl::base::getConnectivityDims(conn),m_dims.begin());

    setupNeighborhoods();
    setPosition(0L);
}

PixelIterator::PixelIterator(const PixelIterator &pi) :
    logger("PixelIterator"),
    m_dims(pi.m_dims),
    m_imageSize(pi.m_imageSize),
    m_ndims(pi.m_ndims),
    m_sx(pi.m_sx),
    m_sxy(pi.m_sxy),
    m_connectivity(pi.m_connectivity),
    m_neighborhoodIndex(pi.m_neighborhoodIndex),
    m_edgeX0(pi.m_edgeX0),
    m_edgeX1(pi.m_edgeX1),
    m_edgeY0(pi.m_edgeY0),
    m_edgeY1(pi.m_edgeY1),
    m_edgeZ0(pi.m_edgeZ0),
    m_edgeZ1(pi.m_edgeZ1),
    m_cornerX0Y0Z0(pi.m_cornerX0Y0Z0),
    m_cornerX1Y0Z0(pi.m_cornerX1Y0Z0),
    m_cornerX0Y1Z0(pi.m_cornerX0Y1Z0),
    m_cornerX1Y1Z0(pi.m_cornerX1Y1Z0),
    m_cornerX0Y0Z1(pi.m_cornerX0Y0Z1),
    m_cornerX0Y1Z1(pi.m_cornerX0Y1Z1),
    m_cornerX1Y0Z1(pi.m_cornerX1Y0Z1),
    m_cornerX1Y1Z1(pi.m_cornerX1Y1Z1),
    m_currentPosition(pi.m_currentPosition),
    m_edgeStatus(pi.m_edgeStatus),
    m_currentX(0),
    m_currentY(0),
    m_currentZ(0),
    m_rowStart(0),
    m_rowEnd(m_sx-1)
{
    setupNeighborhoods();
}

PixelIterator & PixelIterator::operator=(const PixelIterator &pi)
{
    m_currentPosition  = pi.m_currentPosition;
    m_edgeStatus       = pi.m_edgeStatus;
    m_imageSize        = pi.m_imageSize;
    m_connectivity     = pi.m_connectivity;
    m_currentX         = pi.m_currentX;
    m_currentY         = pi.m_currentY;

    m_sx                = pi.m_sx;
    m_sxy               = pi.m_sxy;
    m_dims              = pi.m_dims;
    m_ndims             = pi.m_ndims;
    m_neighborhoodIndex = pi.m_neighborhoodIndex;
    m_edgeX0            = pi.m_edgeX0;
    m_edgeX1            = pi.m_edgeX1;
    m_edgeY0            = pi.m_edgeY0;
    m_edgeY1            = pi.m_edgeY1;
    m_edgeZ0            = pi.m_edgeZ0;
    m_edgeZ1            = pi.m_edgeZ1;

    m_cornerX0Y0Z0      = pi.m_cornerX0Y0Z0;
    m_cornerX1Y0Z0      = pi.m_cornerX1Y0Z0;
    m_cornerX0Y1Z0      = pi.m_cornerX0Y1Z0;
    m_cornerX1Y1Z0      = pi.m_cornerX1Y1Z0;
    m_cornerX0Y0Z1      = pi.m_cornerX0Y0Z1;
    m_cornerX1Y0Z1      = pi.m_cornerX1Y0Z1;
    m_cornerX0Y1Z1      = pi.m_cornerX0Y1Z1;
    m_cornerX1Y1Z1      = pi.m_cornerX1Y1Z1;

    setupNeighborhoods();

    return *this;
}

void PixelIterator::setupNeighborhoods()
{

    switch (m_connectivity) {
    case kipl::base::conn4  : setupConn4();  break;
    case kipl::base::conn8  : setupConn8();  break;
    case kipl::base::conn6  : setupConn6();  break;
    case kipl::base::conn18 : setupConn18(); break;
    case kipl::base::conn26 : setupConn26(); break;

    default :
        throw kipl::base::KiplException("Unknown connectivity provided to PixelIterator");
    }
}

void PixelIterator::setupConn4()
{
    m_neighborhoodIndex = {-m_sx, -1,1,m_sx};

    m_edgeX0 = { -m_sx,  1, m_sx};
    m_edgeX1 = { -m_sx, -1, m_sx};

    m_edgeY0 = {   -1,  1, m_sx};
    m_edgeY1 = {-m_sx, -1, 1};

    m_cornerX0Y0 = { 1, m_sx};
    m_cornerX1Y0 = {-1, m_sx};
    m_cornerX0Y1 = { 1,-m_sx};
    m_cornerX1Y1 = {-1, -m_sx};
}

void PixelIterator::setupConn8()
{

    m_neighborhoodIndex = {-m_sx-1, -m_sx, -m_sx+1, -1, 1, m_sx-1, m_sx,m_sx+1};

    m_edgeX0 = {-m_sx, -m_sx+1, 1, m_sx, m_sx+1};
    m_edgeX1 = {-m_sx-1, -m_sx, -1, m_sx-1, m_sx};

    m_edgeY0 = {-1, 1, m_sx-1, m_sx,m_sx+1};
    m_edgeY1 = {-m_sx-1, -m_sx,- m_sx+1,-1, 1};

    m_cornerX0Y0 = {1,m_sx, m_sx+1};
    m_cornerX1Y0 = {-1,m_sx-1,m_sx};
    m_cornerX0Y1 = {1,-m_sx,-m_sx+1};
    m_cornerX1Y1 = {-m_sx-1, -m_sx,-1};
}

void PixelIterator::setupConn6()
{
    m_neighborhoodIndex = {-m_sxy, -m_sx, -1, 1, m_sx, m_sxy};

    m_edgeX0 = { -m_sxy, -m_sx,  1, m_sx, m_sxy};
    m_edgeX1 = { -m_sxy, -m_sx, -1, m_sx, m_sxy};

    m_edgeY0 = {-m_sxy, -1, 1, m_sx, m_sxy};
    m_edgeY1 = {-m_sxy, -m_sx, -1, 1, m_sxy};

    m_edgeZ0 = {-m_sx, -1, 1, m_sx, m_sxy};
    m_edgeZ1 = {-m_sxy, -m_sx, -1, 1,m_sx};

    m_cornerX0Y0 = {-m_sxy, 1,  m_sx, m_sxy};
    m_cornerX1Y0 = {-m_sxy,-1,  m_sx, m_sxy};
    m_cornerX0Y1 = {-m_sxy,  -m_sx,1, m_sxy};
    m_cornerX1Y1 = {-m_sxy, -m_sx ,-1, m_sxy};
    m_cornerX0Z0 = {-m_sx,1,  m_sx,m_sxy};
    m_cornerX1Z0 = {-m_sx,-1, m_sx,m_sxy};
    m_cornerX0Z1 = {-m_sx,1,  m_sx,m_sxy};
    m_cornerX1Z1 = {-m_sxy,-m_sx,-1, m_sx};
    m_cornerY0Z0 = {-1,1,m_sx,m_sxy};
    m_cornerY1Z0 = {-m_sx,-1,1,m_sxy};
    m_cornerY0Z1 = {-m_sxy,-1,1, m_sx};
    m_cornerY1Z1 = {-m_sxy,-m_sx,-1,1};


    m_cornerX0Y0Z0 = { 1,  m_sx,  m_sxy};
    m_cornerX1Y0Z0 = {-1,  m_sx,  m_sxy};
    m_cornerX0Y1Z0 = { 1, -m_sx,  m_sxy};
    m_cornerX1Y1Z0 = {-1, -m_sx,  m_sxy};
    m_cornerX0Y0Z1 = { 1,  m_sx, -m_sxy};
    m_cornerX1Y0Z1 = {-1,  m_sx, -m_sxy};
    m_cornerX0Y1Z1 = { 1, -m_sx, -m_sxy};
    m_cornerX1Y1Z1 = {-1, -m_sx, -m_sxy};
}

void PixelIterator::setupConn18()
{
    throw kipl::base::KiplException("The pixel-iterator doesn't support 18-connectivity");
}

void PixelIterator::setupConn26()
{
     throw kipl::base::KiplException("The pixel-iterator doesn't support 26-connectivity");
}

PixelIterator & PixelIterator::operator++()
{
    setPosition(++m_currentPosition);
    return *this;
}

PixelIterator & PixelIterator::operator--()
{
    setPosition(--m_currentPosition);
    return *this;
}

ptrdiff_t PixelIterator::begin()
{
    return 0;
}

ptrdiff_t PixelIterator::end()
{
    return m_imageSize;
}

ptrdiff_t PixelIterator::operator=(ptrdiff_t pos)
{
    return setPosition(pos);
}

bool PixelIterator::operator==(ptrdiff_t pos)
{
    return m_currentPosition==pos;
}

bool PixelIterator::operator!=(ptrdiff_t pos)
{
    return m_currentPosition!=pos;
}

ptrdiff_t PixelIterator::setPosition(int x, int y)
{
    if ((x<0) || (y<0) || (m_dims[0]<=x) || (m_dims[1]<=y)) {
        std::ostringstream msg;
        msg<<"PixelIterator: Coordinates ("<<x<<", "<<y
           <<") are out of bounds ("<<m_dims[0]<<", "<<m_dims[1]<<")";
    //    logger.warning(msg.str());
        throw KiplException(msg.str(),__FILE__,__LINE__);
    }
    m_currentZ=0;
    m_currentX=x;
    m_currentY=y;

    m_rowStart        = y*m_sx;
    m_currentPosition = m_rowStart+x;
    m_rowEnd          = m_rowStart+m_sx-1;

    updateEdgeStatus();

    return m_currentPosition;
}

ptrdiff_t PixelIterator::setPosition(int x, int y, int z)
{
    if ((x<0) || (y<0) || (z<0) || (m_dims[0]<=x) || (m_dims[1]<=y) || (m_dims[2]<=z)) {
        std::ostringstream msg;
        msg<<"PixelIterator: Coordinates ("<<x<<", "<<y<<", "<<z<<") are out of bounds ("<<m_dims[0]<<", "<<m_dims[1]<<", "<<m_dims[2]<<")";
        throw KiplException(msg.str(),__FILE__,__LINE__);
    }

    m_currentX=x;
    m_currentY=y;
    m_currentZ=z;

    m_rowStart        = y*m_sx+z*m_sxy;
    m_currentPosition = m_rowStart+x;
    m_rowEnd          = m_rowStart+m_sx-1;

    updateEdgeStatus();

    return m_currentPosition;
}

ptrdiff_t PixelIterator::setPosition(ptrdiff_t pos)
{
//    std::cout<<m_ndims<<", "<<m_connectivity<<"\n";
    switch (m_ndims) {
    case 2:
        if ((pos<m_rowStart) || (m_rowEnd<pos)) {
            m_currentZ = 0;
            m_currentY=pos/m_sx;
        }
        m_currentX=pos-m_currentY*m_sx;
//        std::cout<<"pos="<<pos<<" x="<<m_currentX<<" y="<<m_currentY<<"\n";
        break;
    case 3:
        if ((pos<m_rowStart) || (m_rowEnd<pos)) {
            m_currentZ = pos/m_sxy;
            m_currentY = (pos % m_sxy) / m_sx;
        }
        m_currentX = pos-m_currentY*m_sx-m_currentZ*m_sxy;

//        std::cout<<"pos="<<pos<<" x="<<m_currentX<<" y="<<m_currentY<<" z="<<m_currentZ<<"\n";
        break;
    default :
        kipl::base::KiplException("PixelIterator: Unsupported dimensionality",__FILE__,__LINE__);
    }

    m_currentPosition=pos;

    updateEdgeStatus();

    return m_currentPosition;
}

const std::vector<ptrdiff_t> &PixelIterator::neighborhood()
{

    switch (edgeStatus()) {
    case kipl::base::noEdge       : return m_neighborhoodIndex;
    case kipl::base::edgeX0       : return m_edgeX0;
    case kipl::base::edgeX1       : return m_edgeX1;
    case kipl::base::edgeY0       : return m_edgeY0;
    case kipl::base::edgeY1       : return m_edgeY1;
    case kipl::base::cornerX0Y0   : return m_cornerX0Y0;
    case kipl::base::cornerX1Y1   : return m_cornerX1Y1;
    case kipl::base::cornerX1Y0   : return m_cornerX1Y0;
    case kipl::base::cornerX0Y1   : return m_cornerX0Y1;
    case kipl::base::cornerX0Z0   : return m_cornerX0Z0;
    case kipl::base::cornerX0Z1   : return m_cornerX0Z0;
    case kipl::base::cornerX1Z0   : return m_cornerX0Z0;
    case kipl::base::cornerX1Z1   : return m_cornerX0Z0;
    case kipl::base::cornerY0Z0   : return m_cornerY1Z0;
    case kipl::base::cornerY0Z1   : return m_cornerY1Z0;
    case kipl::base::cornerY1Z0   : return m_cornerY1Z0;
    case kipl::base::cornerY1Z1   : return m_cornerY1Z0;
    case kipl::base::cornerX0Y0Z0 : return m_cornerX0Y0Z0;
    case kipl::base::cornerX0Y0Z1 : return m_cornerX0Y0Z1;
    case kipl::base::cornerX1Y0Z0 : return m_cornerX1Y0Z0;
    case kipl::base::cornerX1Y0Z1 : return m_cornerX1Y0Z1;
    case kipl::base::cornerX0Y1Z0 : return m_cornerX0Y1Z0;
    case kipl::base::cornerX0Y1Z1 : return m_cornerX0Y1Z1;
    case kipl::base::cornerX1Y1Z0 : return m_cornerX1Y1Z0;
    case kipl::base::cornerX1Y1Z1 : return m_cornerX1Y1Z1;
    default : throw kipl::base::KiplException("Unknown edge status in PixelIterator::neighborhood.",__FILE__,__LINE__);
    }

    return m_neighborhoodIndex;
}

ptrdiff_t PixelIterator::neighborhood(size_t idx)
{
    ptrdiff_t pos=0L;
    switch (edgeStatus()) {
    case kipl::base::noEdge       : pos = m_neighborhoodIndex[idx]; break;
    case kipl::base::edgeX0       : pos = m_edgeX0[idx]; break;
    case kipl::base::edgeX1       : pos = m_edgeX1[idx]; break;
    case kipl::base::edgeY0       : pos = m_edgeY0[idx]; break;
    case kipl::base::edgeY1       : pos = m_edgeY1[idx]; break;
    case kipl::base::cornerX0Y0   : pos = m_cornerX0Y0[idx]; break;
    case kipl::base::cornerX1Y1   : pos = m_cornerX1Y1[idx]; break;
    case kipl::base::cornerX1Y0   : pos = m_cornerX1Y0[idx]; break;
    case kipl::base::cornerX0Y1   : pos = m_cornerX0Y1[idx]; break;
    case kipl::base::cornerX0Z0   : pos = m_cornerX0Z0[idx]; break;
    case kipl::base::cornerX0Z1   : pos = m_cornerX0Z0[idx]; break;
    case kipl::base::cornerX1Z0   : pos = m_cornerX0Z0[idx]; break;
    case kipl::base::cornerX1Z1   : pos = m_cornerX0Z0[idx]; break;
    case kipl::base::cornerY0Z0   : pos = m_cornerY1Z0[idx]; break;
    case kipl::base::cornerY0Z1   : pos = m_cornerY1Z0[idx]; break;
    case kipl::base::cornerY1Z0   : pos = m_cornerY1Z0[idx]; break;
    case kipl::base::cornerY1Z1   : pos = m_cornerY1Z0[idx]; break;
    case kipl::base::cornerX0Y0Z0 : pos = m_cornerX0Y0Z0[idx]; break;
    case kipl::base::cornerX0Y0Z1 : pos = m_cornerX0Y0Z1[idx]; break;
    case kipl::base::cornerX1Y0Z0 : pos = m_cornerX1Y0Z0[idx]; break;
    case kipl::base::cornerX1Y0Z1 : pos = m_cornerX1Y0Z1[idx]; break;
    case kipl::base::cornerX0Y1Z0 : pos = m_cornerX0Y1Z0[idx]; break;
    case kipl::base::cornerX0Y1Z1 : pos = m_cornerX0Y1Z1[idx]; break;
    case kipl::base::cornerX1Y1Z0 : pos = m_cornerX1Y1Z0[idx]; break;
    case kipl::base::cornerX1Y1Z1 : pos = m_cornerX1Y1Z1[idx]; break;
    default : throw kipl::base::KiplException("Unknown edge status in PixelIterator::neighborhood.",__FILE__,__LINE__);
    }

    pos+=m_currentPosition;

    return pos;
}

size_t PixelIterator::neighborhoodSize()
{
    switch (m_edgeStatus) {
        case kipl::base::noEdge       : return m_neighborhoodIndex.size();
        case kipl::base::edgeX0       : return m_edgeX0.size();
        case kipl::base::edgeX1       : return m_edgeX1.size();
        case kipl::base::edgeY0       : return m_edgeY0.size();
        case kipl::base::edgeY1       : return m_edgeY1.size();
        case kipl::base::edgeZ0       : return m_edgeZ0.size();
        case kipl::base::edgeZ1       : return m_edgeZ1.size();
        case kipl::base::cornerX0Y0   : return m_cornerX0Y0.size();
        case kipl::base::cornerX1Y0   : return m_cornerX1Y0.size();
        case kipl::base::cornerX0Y1   : return m_cornerX0Y1.size();
        case kipl::base::cornerX1Y1   : return m_cornerX1Y1.size();
        case kipl::base::cornerX0Z0   : return m_cornerX0Z0.size();
        case kipl::base::cornerX0Z1   : return m_cornerX0Z1.size();
        case kipl::base::cornerX1Z0   : return m_cornerX1Z0.size();
        case kipl::base::cornerX1Z1   : return m_cornerX1Z1.size();
        case kipl::base::cornerY0Z0   : return m_cornerY0Z0.size();
        case kipl::base::cornerY0Z1   : return m_cornerY0Z1.size();
        case kipl::base::cornerY1Z0   : return m_cornerY1Z0.size();
        case kipl::base::cornerY1Z1   : return m_cornerY1Z1.size();
        case kipl::base::cornerX0Y0Z0 : return m_cornerX0Y0Z0.size();
        case kipl::base::cornerX0Y0Z1 : return m_cornerX0Y0Z1.size();
        case kipl::base::cornerX1Y0Z0 : return m_cornerX1Y0Z0.size();
        case kipl::base::cornerX1Y0Z1 : return m_cornerX1Y0Z1.size();
        case kipl::base::cornerX0Y1Z0 : return m_cornerX0Y1Z0.size();
        case kipl::base::cornerX0Y1Z1 : return m_cornerX0Y1Z1.size();
        case kipl::base::cornerX1Y1Z0 : return m_cornerX1Y1Z0.size();
        case kipl::base::cornerX1Y1Z1 : return m_cornerX1Y1Z1.size();
    }

    return 0;
}

/// \brief Returns the size of the forward and backward neighborhoods with the current connectivity
ptrdiff_t PixelIterator::forwardSize()
{
    throw kipl::base::KiplException("PixelIterator::forwardSize is not supported",__FILE__,__LINE__);
//    switch (m_edgeStatus) {
//        case kipl::base::noEdge :     return m_fwbwSize.full;
//        case kipl::base::edgeX0 :     return m_fwbwSize.edgeX0;
//        case kipl::base::edgeX1 :     return m_fwbwSize.edgeX1;
//        case kipl::base::edgeY0 :     return m_fwbwSize.edgeY0;
//        case kipl::base::edgeY1 :     return m_fwbwSize.edgeY1;
//        case kipl::base::cornerX0Y0 : return m_fwbwSize.cornerX0Y0 ;
//        case kipl::base::cornerX1Y0 : return m_fwbwSize.cornerX1Y0 ;
//        case kipl::base::cornerX0Y1 : return m_fwbwSize.cornerX0Y1 ;
//        case kipl::base::cornerX1Y1 : return m_fwbwSize.cornerX1Y1 ;
//    }

    return 0;
}

ptrdiff_t PixelIterator::backwardSize()
{
    throw kipl::base::KiplException("PixelIterator::forwardSize is not supported",__FILE__,__LINE__);

//    switch (m_edgeStatus) {
//        case kipl::base::noEdge :     return m_fwbwSize.full;
//        case kipl::base::edgeX0 :     return m_fwbwSize.edgeX1;
//        case kipl::base::edgeX1 :     return m_fwbwSize.edgeX0;
//        case kipl::base::edgeY0 :     return m_fwbwSize.edgeY1;
//        case kipl::base::edgeY1 :     return m_fwbwSize.edgeY0;
//        case kipl::base::cornerX0Y0 : return m_fwbwSize.cornerX1Y1 ;
//        case kipl::base::cornerX1Y0 : return m_fwbwSize.cornerX0Y1 ;
//        case kipl::base::cornerX0Y1 : return m_fwbwSize.cornerX1Y0 ;
//        case kipl::base::cornerX1Y1 : return m_fwbwSize.cornerX0Y0 ;
//    }

    return 0;
}

ptrdiff_t PixelIterator::forwardNeighborhood(int idx)
{
    throw kipl::base::KiplException("PixelIterator: forwardNeighborhood is not implemented",__FILE__,__LINE__);
    ptrdiff_t pos=m_currentPosition;

//    switch (edgeStatus()) {
//        case kipl::base::noEdge :     pos +=  m_neighborhoodIndex[idx+m_halfSizePos.full]; break;
//        case kipl::base::edgeX0 :     pos +=  m_edgeX[idx+m_halfSizePos.edgeX0]; break;
//        case kipl::base::edgeX1 :     pos += -m_edgeX[idx]; break;
//        case kipl::base::edgeY0 :     pos +=  m_edgeY[idx+m_halfSizePos.edgeY0]; break;
//        case kipl::base::edgeY1 :     pos +=  1;    break; //-m_edgeY[idx+m_halfSizePos.edgeY1];   break;
//        case kipl::base::cornerX0Y0 : pos +=  m_corner0[idx+m_halfSizePos.cornerX0Y0]; break;
//        case kipl::base::cornerX1Y1 : pos += -m_corner0[idx+m_halfSizePos.cornerX1Y1]; break;
//        case kipl::base::cornerX1Y0 : pos +=  m_corner1[idx+m_halfSizePos.cornerX1Y0]; break;
//        case kipl::base::cornerX0Y1 : pos += -m_corner1[idx+m_halfSizePos.cornerX0Y1]; break;
//        default : throw kipl::base::KiplException("Unknown edge status in PixelIterator::neighborhood.",__FILE__,__LINE__);
//    }

    return pos;
}

ptrdiff_t PixelIterator::backwardNeighborhood(ptrdiff_t idx)
{
    throw kipl::base::KiplException("PixelIterator: backwardNeighborhood is not implemented",__FILE__,__LINE__);

    ptrdiff_t pos=m_currentPosition;

//    switch (edgeStatus()) {
//        case kipl::base::noEdge :     pos +=  m_neighborhoodIndex[idx]; break;
//        case kipl::base::edgeX0 :     pos +=  m_edgeX[idx]; break;
//        case kipl::base::edgeX1 :     pos += -m_edgeX[idx+m_halfSizePos.edgeX1]; break;
//        case kipl::base::edgeY0 :     pos +=  m_edgeY[idx]; break;
//        case kipl::base::edgeY1 :     pos += -m_edgeY[idx+1];   break;
//        case kipl::base::cornerX0Y0 : pos +=  m_corner0[idx]; break;
//        case kipl::base::cornerX1Y1 : pos += -m_corner0[idx]; break;
//        case kipl::base::cornerX1Y0 : pos +=  m_corner1[idx]; break;
//        case kipl::base::cornerX0Y1 : pos += -m_corner1[idx+1]; break;
//        default : throw kipl::base::KiplException("Unknown edge status in PixelIterator::neighborhood.",__FILE__,__LINE__);
//    }

    return pos;
}

ptrdiff_t PixelIterator::currentPosition()
{
    return m_currentPosition;
}

int PixelIterator::currentX()
{
    return m_currentX;
}

int PixelIterator::currentY()
{
    return m_currentY;
}

int PixelIterator::currentZ()
{
    return m_currentZ;
}

kipl::base::eConnectivity PixelIterator::connectivity()
{
    return m_connectivity;
}

kipl::base::eEdgeStatus PixelIterator::edgeStatus()
{
    return m_edgeStatus;
}

int PixelIterator::supportedDims()
{
    return m_ndims;
}

kipl::base::eEdgeStatus PixelIterator::updateEdgeStatus() {


    int edgeStatus =
              (m_currentX == 0)
            + ((m_currentX == (m_dims[0]-1)) << 1)
            + ((m_currentY == 0)             << 2)
            + ((m_currentY == (m_dims[1]-1)) << 3) ;
    if (m_ndims==3) {
        edgeStatus +=
              ((m_currentZ == 0) << 4)
            + ((m_currentZ == m_dims[2]-1) <<5);
    }

    m_edgeStatus= static_cast<kipl::base::eEdgeStatus>(edgeStatus);

 //   std::cout<<"Edge status: pos:"<<m_currentPosition<<", x:"<<m_currentX<<", y:"<<m_currentY<<", z:"<<m_currentZ<<", edgeStatus:"<<m_edgeStatus<<"\n";

    return m_edgeStatus;
}
}}
