#ifndef AVERAGEPROJECTION_H
#define AVERAGEPROJECTION_H

#include <string>

#include <base/timage.h>

namespace ImagingAlgorithms {

class AverageImage
{
public:
    enum eAverageMethod {
        ImageSum,
        ImageAverage,
        ImageMedian,
        ImageWeightedAverage,
        ImageMin,
        ImageMax
    };
    AverageImage();

    ~AverageImage();
    int WindowSize;
    kipl::base::TImage<float,2> operator()(kipl::base::TImage<float,3> &img, eAverageMethod method, float *weights=nullptr);
protected:
    kipl::base::TImage<float,3> WeightImages(kipl::base::TImage<float,3> &img, float *weights);
    kipl::base::TImage<float,2> ComputeSum(kipl::base::TImage<float,3> &img);
    kipl::base::TImage<float,2> ComputeAverage(kipl::base::TImage<float,3> & img);
    kipl::base::TImage<float,2> ComputeMedian(kipl::base::TImage<float,3> & img);
    kipl::base::TImage<float,2> ComputeWeightedAverage(kipl::base::TImage<float,3> & img);
    kipl::base::TImage<float,2> ComputeMin(kipl::base::TImage<float,3> & img);
    kipl::base::TImage<float,2> ComputeMax(kipl::base::TImage<float,3> & img);

    void GetColumn(kipl::base::TImage<float,3> &img, size_t idx, float *data);
};

}

void string2enum(std::string str, ImagingAlgorithms::AverageImage::eAverageMethod &eam);
std::string enum2string(ImagingAlgorithms::AverageImage::eAverageMethod &eam);

std::ostream & operator<<(ostream & s, ImagingAlgorithms::AverageImage::eAverageMethod eam);
#endif // AVERAGEPROJECTION_H
