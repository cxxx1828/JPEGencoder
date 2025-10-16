#include "JPEG.h"
#include "NxNDCT.h"
#include <math.h>
#include "JPEGBitStreamWriter.h"

#define DEBUG(x) do{ qDebug() << #x << " = " << x;}while(0)

uint8_t QuantLuminance[8*8] =
    { 16, 11, 10, 16, 24, 40, 51, 61,
     12, 12, 14, 19, 26, 58, 60, 55,
     14, 13, 16, 24, 40, 57, 69, 56,
     14, 17, 22, 29, 51, 87, 80, 62,
     18, 22, 37, 56, 68,109,103, 77,
     24, 35, 55, 64, 81,104,113, 92,
     49, 64, 78, 87,103,121,120,101,
     72, 92, 95, 98,112,100,103, 99 };

uint8_t QuantChrominance[8*8] =
    { 17, 18, 24, 47, 99, 99, 99, 99,
     18, 21, 26, 66, 99, 99, 99, 99,
     24, 26, 56, 99, 99, 99, 99, 99,
     47, 66, 99, 99, 99, 99, 99, 99,
     99, 99, 99, 99, 99, 99, 99, 99,
     99, 99, 99, 99, 99, 99, 99, 99,
     99, 99, 99, 99, 99, 99, 99, 99,
     99, 99, 99, 99, 99, 99, 99, 99 };



void DCTUandV(const char input[], int16_t output[], int N, double* DCTKernel)
{
    double* temp = new double[N*N];
    double* result = new double[N*N];

    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            double suma = 0;
            for (int k = 0; k < N; k++) {
                suma = suma + DCTKernel[i*N+k] * input[k*N+j];
            }
            temp[i*N + j] = suma;
        }
    }

    for (int i = 0; i < N; i++) {
        for (int j = 0; j < N; j++) {
            double suma = 0;
            for (int k = 0; k < N; k++) {
                suma = suma + temp[i*N+k] * DCTKernel[j*N+k];
            }
            result[i*N+j] = suma;
        }
    }

    for(int i = 0; i < N*N; i++) {
        output[i] = floor(result[i] + 0.5);
    }

    delete[] temp;
    delete[] result;

    return;
}

uint8_t quantQuality(uint8_t quant, uint8_t quality)
{

    // osiguram validan opseg
    if (quality < 1) quality = 1;
    if (quality > 100) quality = 100;

    //male vrednosti quality → snazna kvantizacija
    //velike vrednosti → blaga kvantizacija
    int q;
    if(quality < 50)
        q = 5000 / quality;
    else
        q = 200 - quality * 2;

    int result = (quant * q + 50) / 100;// +50 integer trik za zaokruzivanje na najblizi ceo broj

    //clampujemo u opseg [1,255]
    if(result < 1) result = 1;
    if(result > 255) result = 255;

    return result;
}


void ZigZag(int16_t block[], uint8_t quantBlock[], int N, int flag)
{
    int zigzag[64] = {
        0,1,8,16,9,2,3,10,
        17,24,32,25,18,11,4,5,
        12,19,26,33,40,48,41,34,
        27,20,13,6,7,14,21,28,
        35,42,49,56,57,50,43,36,
        29,22,15,23,30,37,44,51,
        58,59,52,45,38,31,39,46,
        53,60,61,54,47,55,62,63
    };

    if (block != nullptr) {
        int16_t temp[64];
        for(int i=0; i < 64; i++) {
            temp[i] = block[zigzag[i]];
        }
        for(int i=0; i < 64; i++) {
            block[i] = temp[i];
        }
    }


    //zigzag za kvantizacione tabele
    if (flag == 1) {
        uint8_t temp[64];
        for (int i = 0; i < 64; i++) {
            temp[i] = quantBlock[zigzag[i]];
        }
        for (int i = 0; i < 64; i++) {
            quantBlock[i] = temp[i];
        }
    }
}

// /* perform DCT */
// imageProperties performDCT(char input[], int xSize, int ySize, int N, uint8_t quality, bool quantType)
// {
//     // TO DO
// }


void performJPEGEncoding(uchar* YPlane, char* UPlane, char* VPlane, int width, int height, int quality)
{
    DEBUG(width);
    DEBUG(height);
    DEBUG(quality);


    //kreiranje i priprema jpeg-a
    const int blockSize = 8;

    qDebug() << "kreiram JPEGBitStreamWriter...";
    JPEGBitStreamWriter* writer = new JPEGBitStreamWriter("example.jpg");
    writer->writeHeader();
    qDebug() << "header napisan";


    //kvantizacija, tj. formiranje kvantizacionih tabela na osnovu quality
    uint8_t lumTable[64], chromTable[64];
    for(int i = 0; i < 64; i++){
        lumTable[i] = quantQuality(QuantLuminance[i], quality);
        chromTable[i] = quantQuality(QuantChrominance[i], quality);
    }
    ZigZag(nullptr, lumTable, blockSize, 1);
    ZigZag(nullptr, chromTable, blockSize, 1);

    qDebug() << "kvantizacioni tabeli kreirani i zigzagovana:";
    qDebug() << "primer lumTable[0] =" << lumTable[0];
    qDebug() << "primer chromTable[0] =" << chromTable[0];

    //ovde pisem u fajl kvantizacione tabele, dimenzije slike i huffman tables
    //to je jpeg header strukture koji opisuje kako ce se enkodirati podaci
    writer->writeQuantizationTables(lumTable, chromTable);
    writer->writeImageInfo(width, height);
    writer->writeHuffmanTables();
    qDebug() << "kvantizacija, info i huffman tabele napisane";


    //level shift aka pomak vrednosti Y kanala za 128
    //jpeg standard zahteva da se pikseli pre DCT transformacije pomere tako da im opseg bude oko nule
    //0–255 → -128 do 127.
    char* Yshifted = new char[width * height];
    for(int i = 0; i < width * height; i++)
        Yshifted[i] = static_cast<char>(YPlane[i] - 128);

    qDebug() << "Y kanal level-shiftovan, primer Yshifted[0] =" << (int)Yshifted[0];

    // U i V kanali se obično ne pomeraju jer su već centrirani oko 0, tj -127 - 128 nakon YUV konverzije

    //ovde su jos originalne vrednosti
    //(4:2:0)
    int paddedYWidth = width;
    int paddedYHeight = height;
    int paddedUWidth = width / 2;
    int paddedUHeight = height / 2;
    int paddedVWidth = width / 2;
    int paddedVHeight = height / 2;

    char* YData = Yshifted;
    char* UData = UPlane;
    char* VData = VPlane;


    //namestam da slika bude deljiva sa 8 tj 16 jer jpeg deli sliku u blokove od 8x8 piksela
    if(width % 16 != 0 || height % 16 != 0){
        qDebug() << "prosirujem granice za 16-blokove...";
        extendBorders(Yshifted, width, height, blockSize, &YData, &paddedYWidth, &paddedYHeight);
        extendBorders(UPlane, width/2, height/2, blockSize, &UData, &paddedUWidth, &paddedUHeight);
        extendBorders(VPlane, width/2, height/2, blockSize, &VData, &paddedVWidth, &paddedVHeight);
        qDebug() << "prosirenje granica gotovo, nova sirina/visina Y:" << paddedYWidth << "/" << paddedYHeight;
    }


    //generisanje DCT matrice
    double* dctMatrix = new double[64];
    GenerateDCTmatrix(dctMatrix, blockSize);
    qDebug() << "DCT matrica generisana";

    char blockY[64], blockU[64], blockV[64];
    short dctY[64], dctU[64], dctV[64];

    //obrada svakog 16×16 makrobloka (4:2:0 subsampling)
    //jpeg standard koristi blokove od 16x16 piksela(4 8x8 Y bloka i po jedan 8x8 blok za U i V)
    for(int y = 0; y < paddedYHeight; y += 16){
        for(int x = 0; x < paddedYWidth; x += 16){


            //za Y blokove
            for(int by = 0; by < 2; by++){
                for(int bx = 0; bx < 2; bx++){
                    for(int i = 0; i < blockSize; i++){
                        for(int j = 0; j < blockSize; j++){
                            blockY[i * blockSize + j] = YData[(y + by*8 + i)*paddedYWidth + (x + bx*8 + j)];//kopira se iz slike u lokalni blok
                        }
                    }
                    DCTUandV(blockY, dctY, blockSize, dctMatrix);//DCT
                    for(int k = 0; k < 64; k++)
                        dctY[k] = round((double)dctY[k] / lumTable[k]);//podeli se sa kvantizacionom vrednoscu
                    ZigZag(dctY, nullptr, blockSize, 0); //zigzag
                    writer->writeBlockY(dctY);//upise se u fajl
                }
            }


            //za U i V blokove
            //postupak isti samo druga kvantizaciona tabela(Chrominance)
            for(int i = 0; i < blockSize; i++){
                for(int j = 0; j < blockSize; j++){
                    blockU[i * blockSize + j] = UData[(y/2 + i)*paddedUWidth + (x/2 + j)];
                    blockV[i * blockSize + j] = VData[(y/2 + i)*paddedVWidth + (x/2 + j)];
                }
            }
            DCTUandV(blockU, dctU, blockSize, dctMatrix);
            DCTUandV(blockV, dctV, blockSize, dctMatrix);

            for(int k = 0; k < 64; k++){
                dctU[k] = round((double)dctU[k] / chromTable[k]);
                dctV[k] = round((double)dctV[k] / chromTable[k]);
            }
            ZigZag(dctU, nullptr, blockSize, 0);
            ZigZag(dctV, nullptr, blockSize, 0);

            writer->writeBlockU(dctU);
            writer->writeBlockV(dctV);

            if(x==0 && y==0){
                qDebug() << "prvi 16x16 blok Y, U, V napisan";
                qDebug() << "primer dctY[0]=" << dctY[0] << "dctU[0]=" << dctU[0] << "dctV[0]=" << dctV[0];
            }
        }
    }


    //zatvaranje fajla, ciscenje memorije
    writer->finishStream();
    qDebug() << "zavrseno pisanje jpeg fajla";

    delete[] dctMatrix;
    delete[] Yshifted;
    if(YData != Yshifted) delete[] YData;
    if(UData != UPlane) delete[] UData;
    if(VData != VPlane) delete[] VData;
    delete writer;

    qDebug() << "memorija oslobodjena, funkcija zavrsena";
}

