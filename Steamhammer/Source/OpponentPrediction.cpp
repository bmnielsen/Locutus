#include <iostream>
#include <fstream>
#include "opencv.hpp"
#include "OpponentPrediction.h"

using namespace cv;

OpponentPredictionNN::OpponentPredictionNN()
{
    std::string filePath = "bwapi-data/AI/ISAMind.xml";
    std::fstream _file;
    _file.open(filePath, std::ios::in);
    if (!_file)
    {
        m_NnLoaded = 0;
    } 
    else
    {
        m_Mlp.load(filePath.c_str());
        m_NnLoaded = 1;
    }
}

int OpponentPredictionNN::getOpeningPlan(std::vector<int> features)
{
    Mat responseMat;
    Mat featureMat = Mat::zeros(1, features.size(), CV_32F);
    Point maxLoc = 0;

    for (unsigned int i = 0; i < features.size(); i++)
    {
        featureMat.at<float>(0, i) = features[i];
    }
    m_Mlp.predict(featureMat, responseMat);
    minMaxLoc(responseMat, NULL, NULL, NULL, &maxLoc);

    return maxLoc.x;
}
