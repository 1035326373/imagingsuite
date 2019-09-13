#include <QtTest>

// add necessary includes here
#include <sstream>

#include <base/timage.h>
#include <base/KiplException.h>
#include <base/kiplenums.h>
#include <morphology/morphology.h>
#include <morphology/morphgeo.h>
#include <io/io_tiff.h>

class morphgeo : public QObject
{
    Q_OBJECT

public:
    morphgeo();
    ~morphgeo();

private slots:
    void test_RecByDilation();

};

morphgeo::morphgeo()
{

}

morphgeo::~morphgeo()
{

}

void morphgeo::test_RecByDilation()
{
    kipl::base::TImage<float,2> img;

    kipl::io::ReadTIFF(img,"../../imagingsuite/core/kipl/UnitTests/data/scroll_256.tif");

    kipl::base::TImage<float,2> img2;
    img2.Clone(img);
    img2+=1.0f;

    kipl::base::TImage<float,2> ref,dev;
//    QBENCHMARK
    {
        ref=kipl::morphology::old::RecByDilation(img2,img,kipl::morphology::conn8);
    }

//    QBENCHMARK
    {
        dev=kipl::morphology::RecByDilation(img2,img,kipl::base::conn8);
    }

    QCOMPARE(ref.Size(),dev.Size());
    std::ostringstream msg;
    size_t cnt=0;
    for (size_t i=0; i<ref.Size(); ++i)
    {
        //msg.str(""); msg<<"pos="<<i<<", ref="<<ref[i]<<", dev="<<dev[i];

       // QVERIFY2(ref[i]==dev[i],msg.str().c_str());
        if (ref[i]!=dev[i])
            cnt++;
    }

    kipl::io::WriteTIFF32(ref,"recdil_ref.tif");
    kipl::io::WriteTIFF32(dev,"recdil_dev.tif");
  //  QCOMPARE(cnt,0UL);
}



QTEST_APPLESS_MAIN(morphgeo)

#include "tst_morphgeo.moc"