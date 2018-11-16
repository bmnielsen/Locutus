#ifndef OPPONENT_PREDICTION
#define OPPONENT_PREDICTION
#include "opencv.hpp"
#include <vector>

class OpponentPredictionNN
{
private:
    CvANN_MLP m_Mlp;
public:
    int m_NnLoaded;
    OpponentPredictionNN();
    int getOpeningPlan(std::vector<int> features);
};

#endif // !
