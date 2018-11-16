#pragma once
#include <math.h>
#include <iostream>
#include <string>
#include <vector>
#include <algorithm>
#include <stdlib.h>
#include <regex>
#include <fstream>
#include <sstream>
#include <iterator>
#include <map>
#include <BWAPI.h>

#define ZERG_FEATURE_NUM 95
#define TERRAN_FEATURE_NUM 96
#define PROTOSS_FEATURE_NUM 94

using namespace std;


class PredictModel
{
	vector<pair<string, pair<vector<int>, vector<double>>>> pvz_params;
	vector<pair<string, pair<vector<int>, vector<double>>>> pvt_params;
	vector<pair<string, pair<vector<int>, vector<double>>>> pvp_params;
	BWAPI::Race enemy_race;

	PredictModel();
	void initParams();
	// ========================= Activation Function: ELUs ========================
	template<typename _Tp>
	int activation_function_ELUs(const _Tp* src, _Tp* dst, int length, _Tp a = 1.);
	// ========================= Activation Function: Leaky_ReLUs =================
	template<typename _Tp>
	int activation_function_Leaky_ReLUs(const _Tp* src, _Tp* dst, int length);
	// ========================= Activation Function: ReLU =======================
	template<typename _Tp>
	int activation_function_ReLU(const _Tp* src, _Tp* dst, int length);
	// ========================= Activation Function: softplus ===================
	template<typename _Tp>
	int activation_function_softplus(const _Tp* src, _Tp* dst, int length);
	// ========================= Activation Function: softmax ===================
	template<typename _Tp>
	int activation_function_softmax(const _Tp* src, _Tp* dst, int length);
	// ========================= Activation Function: tanh ===================
	template<typename _Tp>
	int activation_function_tanh(const _Tp* src, _Tp* dst, int length);
	// =============================== Activation Function: sigmoid ==========================
	template<typename _Tp>
	int activation_function_sigmoid(const _Tp* src, _Tp* dst, int length);
	template<typename _Tp>
	int activation_function_sigmoid_fast(const _Tp* src, _Tp* dst, int length);

	void print_matrix(std::vector<double> mat);

	void test_activation_function();

	string& trim(string &s, char delim);

	//If you want to avoid reading into character arrays, 
	//you can use the C++ string getline() function to read lines into strings
	void ReadDataFromFileLBLIntoString();

	void _split(const string &s, char delim, vector<string> &elems);
	vector<string> split(const string &s, char delim);

	string extract(string &values, int index, char delim = ' ');

	// transfer string vector to int vector
	vector<int> get_iv_from_sv(const vector<string> sv);

	pair<vector<int>, vector<double>> matrix_dot(pair<vector<int>, vector<double>> m1, pair<vector<int>, vector<double>> m2);

	//If we were interested in preserving whitespace, 
	//we could read the file in Line-By-Line using the I/O getline() function.
	vector<pair<string, pair<vector<int>, vector<double>>>> load_data(char* filename);

	double normalize_min_max(double origin_value, int max_threashold, int min_threashold = 0);

public:
	int count;

	static	PredictModel & Instance();

	int getUpgradeFeature(BWAPI::UpgradeType upgrade);

	int getResearchFeature(BWAPI::TechType upgrade);

	vector<double> getPredictUnits(vector<double> input_m);

	void setEnemyRace(BWAPI::Race enemy_race);

	vector<size_t> sort_indexes(const vector<double> &v);

	void normalize_feature_pvz(vector<double> &features);

	void normalize_feature_pvt(vector<double> &features);

	void normalize_feature_pvp(vector<double> &features);
};