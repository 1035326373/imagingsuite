#ifndef NONLOCALMEANS_H
#define NONLOCALMEANS_H

#include "../base/timage.h"
#include "../logging/logger.h"

namespace akipl {

class NonLocalMeans {
protected:
    kipl::logging::Logger logger;
    public:
        /// \brief C'tor
        /// \param k Size of the box filter
        /// \param h width of the weighting function
        /// \param nBin number of histogram bins
        NonLocalMeans(int k, double h, size_t nBins=2048);
        ~NonLocalMeans();

        void operator()(kipl::base::TImage<float,2> &f, kipl::base::TImage<float,2> &g);
        void operator()(kipl::base::TImage<float,3> &f, kipl::base::TImage<float,3> &g);

    protected:
        /// \brief Computes distance weight between a and b
        /// \param first value
        /// \param second value
        float weight(float a,float b);

        /// \brief Computes a new histogram
        vector<pair<float, size_t> > ComputeHistogram(float *data, size_t N);

        /// \brief Naive implementation of the non-local means algorithm. Very slow due to N^2 complexity.
        /// \param f pointer to the original image
        /// \param ff pointer to the filtered image
        /// \param g pointer to the result image
        /// \param N number of pixels
        void nlm_core(float *f, float *ff, float *g, size_t N);

        /// \brief Implementation with histogram patching
        /// \param f pointer to the original image
        /// \param ff pointer to the filtered image
        /// \param g pointer to the result image
        /// \param N number of pixels
        void nlm_core_hist(float *f, float *ff, float *g, size_t N);

        float m_fWidth;
        float m_fWidthLimit;
        int m_nBoxSize;

        size_t m_nHistSize;
        size_t *m_nHistogram;
        float *m_fHistBins;
};

}

//#include "core/nonlocalmeans.hpp"
#endif // NONLOCALMEANS_H

