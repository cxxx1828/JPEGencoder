#include "ImageProcessing.h"
#include "ColorSpaces.h"
#include "JPEG.h"

#include <QImage>
#include <QString>
#include <QVector>
#include <QDebug>

void imageProcessingFun(const QString& progName, QImage& outImgs, const QImage& inImgs, const QVector<double>& params)
{
    qDebug() << "pocetak obrade";

    int origWidth  = inImgs.width();
    int origHeight = inImgs.height();
    qDebug() << "originalne dimenzije:" << origWidth << "x" << origHeight;

    //priprema sliku i YUV bafer pre poziva enkodera
    int paddedWidth  = (origWidth  + 15) / 16 * 16;
    int paddedHeight = (origHeight + 15) / 16 * 16;
    qDebug() << "prosirene dimenzije:" << paddedWidth << "x" << paddedHeight;

    int ySize = paddedWidth * paddedHeight;
    int uvSize = ySize / 4;
    uchar* Y = new uchar[ySize];
    char* U = new char[uvSize];
    char* V = new char[uvSize];
    qDebug() << "yuv baferi alocirani:" << "y:" << ySize << "u/v:" << uvSize;

    QImage padded = inImgs.copy(0, 0, paddedWidth, paddedHeight);
    qDebug() << "slika prosirena na:" << padded.width() << "x" << padded.height();

    qDebug() << "konvertujem rgb -> yuv420...";
    RGBtoYUV420(padded.bits(), paddedWidth, paddedHeight, Y, U, V);
    qDebug() << "konverzija zavrsena, primer vrednosti: y[0]=" << (int)Y[0]
             << "u[0]=" << (int)U[0] << "v[0]=" << (int)V[0];

    if(progName == "JPEG Encoder")
    {

        //performJPEGEncoding takodje radi proveru/padding, ali sa detaljnijom obradom ivica i DCT blokovima
        qDebug() << "pokrecem jpeg enkoder sa kvalitetom:" << params[0];
        performJPEGEncoding(Y, U, V, paddedWidth, paddedHeight, params[0]);
        qDebug() << "jpeg enkodovanje gotovo, napravljeno example.jpg";
    }

    qDebug() << "ucitavam example.jpg...";
    outImgs = QImage("example.jpg");
    if(outImgs.isNull())
        qDebug() << "greska: example.jpg nije ucitan!";
    else
        qDebug() << "example.jpg ucitan, dimenzije:" << outImgs.width() << "x" << outImgs.height();

    delete[] Y;
    delete[] U;
    delete[] V;
    qDebug() << "memorija oslobodjena";
    qDebug() << "kraj obrade";
}
