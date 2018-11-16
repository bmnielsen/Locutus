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
#include <numeric>
#include "PredictModel.h"
#include "ModelWeightInit.h"

using namespace std;

PredictModel::PredictModel()
{
	count = 0;
	initParams();
	//pvz_params = load_data("bwapi-data\\AI\\PvZ.model");
	//pvt_params = load_data("bwapi-data\\AI\\PvT.model");
	//pvp_params = load_data("bwapi-data\\AI\\PvP.model");
}

void PredictModel::initParams()
{
	string param_name("0.weight");
	vector<int> size = { 256, 151 };
	//pvz_params.push_back(make_pair(param_name, make_pair(size, pvz_weight_0)));
	//param_name = "0.bias";
	//size = { 256 };
	//pvz_params.push_back(make_pair(param_name, make_pair(size, pvz_bias_0)));
	//param_name = "2.weight";
	//size = { 256, 256 };
	//pvz_params.push_back(make_pair(param_name, make_pair(size, pvz_weight_2)));
	//param_name = "2.bias";
	//size = { 256 };
	//pvz_params.push_back(make_pair(param_name, make_pair(size, pvz_bias_2)));
	//param_name = "4.weight";
	//size = { 32, 256 };
	//pvz_params.push_back(make_pair(param_name, make_pair(size, pvz_weight_4)));
	//param_name = "4.bias";
	//size = { 32 };
	//pvz_params.push_back(make_pair(param_name, make_pair(size, pvz_bias_4)));

	param_name = "0.weight";
	size = { 64, 96 };
	pvt_params.push_back(make_pair(param_name, make_pair(size, PRED_PW0)));
	param_name = "0.bias";
	size = { 64 };
	pvt_params.push_back(make_pair(param_name, make_pair(size, PRED_PB0)));
	param_name = "2.weight";
	size = { 32, 64 };
	pvt_params.push_back(make_pair(param_name, make_pair(size, PRED_PW2)));
	param_name = "2.bias";
	size = { 32 };
	pvt_params.push_back(make_pair(param_name, make_pair(size, PRED_PB2)));
	//param_name = "4.weight";
	//size = { 32, 256 };
	//pvt_params.push_back(make_pair(param_name, make_pair(size, pvt_weight_4)));
	//param_name = "4.bias";
	//size = { 32 };
	//pvt_params.push_back(make_pair(param_name, make_pair(size, pvt_bias_4)));

	//param_name = "0.weight";
	//size = { 256, 150 };
	//pvp_params.push_back(make_pair(param_name, make_pair(size, pvp_weight_0)));
	//param_name = "0.bias";
	//size = { 256 };
	//pvp_params.push_back(make_pair(param_name, make_pair(size, pvp_bias_0)));
	//param_name = "2.weight";
	//size = { 256, 256 };
	//pvp_params.push_back(make_pair(param_name, make_pair(size, pvp_weight_2)));
	//param_name = "2.bias";
	//size = { 256 };
	//pvp_params.push_back(make_pair(param_name, make_pair(size, pvp_bias_2)));
	//param_name = "4.weight";
	//size = { 32, 256 };
	//pvp_params.push_back(make_pair(param_name, make_pair(size, pvp_weight_4)));
	//param_name = "4.bias";
	//size = { 32 };
	//pvp_params.push_back(make_pair(param_name, make_pair(size, pvp_bias_4)));
}

PredictModel & PredictModel::Instance()
{
	static PredictModel instance;
	return instance;
}
int PredictModel::getUpgradeFeature(BWAPI::UpgradeType upgrade)
{
	int upgrade_feature = BWAPI::Broodwar->self()->getUpgradeLevel(upgrade);
	if (BWAPI::Broodwar->self()->isUpgrading(upgrade))
		upgrade_feature++;
	return upgrade_feature;
}
int PredictModel::getResearchFeature(BWAPI::TechType tech)
{
	bool research_value = BWAPI::Broodwar->self()->hasResearched(tech);
	if (BWAPI::Broodwar->self()->isResearching(tech))
		research_value = true;
	return research_value ? 1 : 0;
}
// ========================= Activation Function: ELUs ========================
template<typename _Tp>
int PredictModel::activation_function_ELUs(const _Tp* src, _Tp* dst, int length, _Tp a)
{
	if (a < 0) {
		fprintf(stderr, "a is a hyper-parameter to be tuned and a>=0 is a constraint\n");
		return -1;
	}

	for (int i = 0; i < length; ++i) {
		dst[i] = src[i] >= (_Tp)0. ? src[i] : (a * (exp(src[i]) - (_Tp)1.));
	}

	return 0;
}

// ========================= Activation Function: Leaky_ReLUs =================
template<typename _Tp>
int PredictModel::activation_function_Leaky_ReLUs(const _Tp* src, _Tp* dst, int length)
{
	for (int i = 0; i < length; ++i) {
		dst[i] = src[i] >(_Tp)0. ? src[i] : (_Tp)0.01 * src[i];
	}

	return 0;
}

// ========================= Activation Function: ReLU =======================
template<typename _Tp>
int PredictModel::activation_function_ReLU(const _Tp* src, _Tp* dst, int length)
{
	for (int i = 0; i < length; ++i) {
		dst[i] = std::max((_Tp)0., src[i]);
	}

	return 0;
}

// ========================= Activation Function: softplus ===================
template<typename _Tp>
int PredictModel::activation_function_softplus(const _Tp* src, _Tp* dst, int length)
{
	for (int i = 0; i < length; ++i) {
		dst[i] = log((_Tp)1. + exp(src[i]));
	}

	return 0;
}

// ========================= Activation Function: softmax ===================
template<typename _Tp>
int PredictModel::activation_function_softmax(const _Tp* src, _Tp* dst, int length)
{
	_Tp denom = 0.;
	for (int i = 0; i < length; ++i) {
		denom += (_Tp)(exp(src[i]));
	}
	//printf("demon:%f", demon);
	for (int i = 0; i < length; ++i) {
		dst[i] = (_Tp)(exp(src[i]) / denom);
	}
	return 0;
}

// ========================= Activation Function: tanh ===================
template<typename _Tp>
int PredictModel::activation_function_tanh(const _Tp* src, _Tp* dst, int length)
{
	for (int i = 0; i < length; ++i) {
		dst[i] = (_Tp)((exp(src[i]) - exp(-src[i])) / (exp(src[i]) + exp(-src[i])));
	}

	return 0;
}


// =============================== Activation Function: sigmoid ==========================
template<typename _Tp>
int PredictModel::activation_function_sigmoid(const _Tp* src, _Tp* dst, int length)
{
	for (int i = 0; i < length; ++i) {
		dst[i] = (_Tp)(1. / (1. + exp(-src[i])));
	}

	return 0;
}

template<typename _Tp>
int PredictModel::activation_function_sigmoid_fast(const _Tp* src, _Tp* dst, int length)
{
	for (int i = 0; i < length; ++i) {
		dst[i] = (_Tp)(src[i] / (1. + fabs(src[i])));
	}

	return 0;
}


void PredictModel::print_matrix(std::vector<double> mat)
{
	fprintf(stderr, "[");
	for each (double var in mat)
	{
		fprintf(stderr, "%0.9f, ", var);
	}
	fprintf(stderr, "]\n");
}

void PredictModel::test_activation_function()
{
	std::vector<double> src{ 1.23f, 4.14f, -3.23f, -1.23f, 5.21f, 0.234f, -0.78f, 6.23f };
	int length = src.size();
	std::vector<double> dst(length);

	fprintf(stderr, "source vector: \n");
	print_matrix(src);
	fprintf(stderr, "calculate activation function:\n");
	fprintf(stderr, "type: sigmoid result: \n");
	activation_function_sigmoid(src.data(), dst.data(), length);
	print_matrix(dst);
	fprintf(stderr, "type: sigmoid fast result: \n");
	activation_function_sigmoid_fast(src.data(), dst.data(), length);
	print_matrix(dst);
	fprintf(stderr, "type: softplus result: \n");
	activation_function_softplus(src.data(), dst.data(), length);
	print_matrix(dst);
	fprintf(stderr, "type: ReLU result: \n");
	activation_function_ReLU(src.data(), dst.data(), length);
	print_matrix(dst);
	fprintf(stderr, "type: Leaky ReLUs result: \n");
	activation_function_Leaky_ReLUs(src.data(), dst.data(), length);
	print_matrix(dst);
	fprintf(stderr, "type: Leaky ELUs result: \n");
	activation_function_ELUs(src.data(), dst.data(), length);
	print_matrix(dst);
	fprintf(stderr, "type: Leaky tanh result: \n");
	activation_function_tanh(src.data(), dst.data(), length);
	print_matrix(dst);
	fprintf(stderr, "type: Leaky softmax result: \n");
	activation_function_softmax(src.data(), dst.data(), length);
	print_matrix(dst);
}

string& PredictModel::trim(string &s, char delim)
{
	if (s.empty())
	{
		return s;
	}

	s.erase(0, s.find_first_not_of(delim));
	s.erase(s.find_last_not_of(delim) + 1);
	return s;
}

//If you want to avoid reading into character arrays, 
//you can use the C++ string getline() function to read lines into strings
void PredictModel::ReadDataFromFileLBLIntoString()
{
	ifstream fin("data.txt");
	string s;
	while (getline(fin, s))
	{
		cout << "Read from file: " << s << endl;
	}
}

void PredictModel::_split(const string &s, char delim,
	vector<string> &elems) {
	stringstream ss(s);
	string item;

	while (getline(ss, item, delim)) {
		item = trim(item, ' ');
		elems.push_back(item);
	}
}

vector<string> PredictModel::split(const string &s, char delim) {
	vector<string> elems;
	_split(s, delim, elems);
	return elems;
}

string PredictModel::extract(string &values, int index, char delim) {
	if (values.length() == 0)
		return string("");

	vector<string> x = split(values, delim);
	try {
		return x.at(index);
	}
	catch (const out_of_range& e) {
		return string("");  // return empty str if out of range
	}
}

// transfer string vector to int vector
vector<int> PredictModel::get_iv_from_sv(const vector<string> sv)
{
	vector<int> result;
	for each (string s in sv)
	{
		result.push_back(stoi(s));
	}
	return result;
}

pair<vector<int>, vector<double>> PredictModel::matrix_dot(pair<vector<int>, vector<double>> m1, pair<vector<int>, vector<double>> m2)
{
	pair<vector<int>, vector<double>> null_result;
	if (m1.first.size() < 1 || m2.first.size() < 1)
	{
		cerr << "matrix dim must bigger than one!";
		return null_result;
	}
	int m1_row = m1.first[m1.first.size() - 1];
	int m2_col = m2.first[m2.first.size() - 2];
	cout << "m1_row:" << m1_row << endl;
	cout << "m2_col:" << m2_col << endl;
	int dot_num = m1_row;
	int jump_num = 1;
	int temp_num = 1; // loop times indicator
	vector<double> result_matrix_v;
	vector<int> result_size_v;

	// check whether the dot is legal
	if (m1_row != m2_col)
	{
		cerr << "the two matrix doesn't match the dot principle !";
		return null_result;
	}

	cout << "Calculate the Result..." << endl;
	// record the shape of matrix
	for (int i = 0; i < m1.first.size() - 1; i++)
	{
		result_size_v.push_back(m1.first[i]);
	}
	for (int i = 0; i < m2.first.size(); i++)
	{
		if (i != m2.first.size() - 2)
		{
			result_size_v.push_back(m2.first[i]);
		}
	}

	// jump num of m2 for each dot
	if (m2.first.size() > 2)
	{
		jump_num = m2.first[m2.first.size() - 1];
	}
	for (int i = 1; i < m2.first.size(); i++)
	{
		temp_num *= m2.first[i];
	}
	double result = 0.0;
	for (int i = 0; i < temp_num; i++)
	{
		for (int j = 0; j < m1.second.size(); j++)
		{
			result += m1.second[j] * m2.second[jump_num * j + dot_num * i];
			if (((j + 1) % dot_num) == 0)
			{
				result_matrix_v.push_back(result);
				result = 0;
			}
		}
	}
	return make_pair(result_size_v, result_matrix_v);
}

//If we were interested in preserving whitespace, 
//we could read the file in Line-By-Line using the I/O getline() function.
vector<pair<string, pair<vector<int>, vector<double>>>> PredictModel::load_data(char* filename)
{
	ifstream fin(filename);
	if (!fin)
	{
		cout << "Error opening " << filename << " for input" << endl;
		exit(-1);
	}
	cout << "Loading Model..." << endl;
	string s;
	vector<pair<string, pair<vector<int>, vector<double>>>> matrix_v;
	smatch m1;
	regex e1("dict\\[(.*)\\]");
	smatch m2;
	regex e2("TF.Size\\(\\[(.*)\\]\\)");
	regex elb("(.*)\\[(.*)");
	regex erb("(.*)\\](.*)");
	regex rb("\\]");
	string name;
	vector<string> size_v;
	vector<double> matrix_p;
	bool name_set = false;
	bool sizev_set = false;

	while (getline(fin, s))
	{
		if (s == "\n")
			continue;
		//cout << "Read from file: " << s << endl;
		if (regex_search(s, m1, e1))
		{
			name = m1.format("$1");
			name_set = true;
		}
		else if (regex_search(s, m2, e2))
		{
			string size = m2.format("$1");
			size_v = split(size, ',');
			sizev_set = true;
		}
		else
		{
			vector<string> data = split(s, ',');
			int data_size = data.size();
			int match_count = 0;
			for (int i = 0; i < data_size; i++)
			{
				while (regex_match(data[i], elb))
					data[i] = trim(data[i], '[');
				while (regex_match(data[i], erb))
				{
					match_count = std::distance(
						std::sregex_iterator(data[i].begin(), data[i].end(), rb),
						std::sregex_iterator());
					data[i] = trim(data[i], ']');
				}
				matrix_p.push_back(stod(data[i]));
				if (match_count == size_v.size())
				{
					vector<int> isv = get_iv_from_sv(size_v);
					reverse(isv.begin(), isv.end());
					matrix_v.push_back(make_pair(name, make_pair(isv, matrix_p)));
					matrix_p.clear();
					name_set = false;
					sizev_set = false;
				}
			}
			//cout << "param size: " << matrix_v.size() << endl;
		}
	}
	for each (pair<string, pair<vector<int>, vector<double>>> var in matrix_v)
	{

		cout << var.first << " length:" << var.second.second.size() << endl;
		cout << var.first << " size:(";
		for each (int size in var.second.first)
		{
			cout << size << ",";
		}
		cout << ")" << endl;
	}
	fin.close();
	return matrix_v;
}

double PredictModel::normalize_min_max(double origin_value, int max_threashold, int min_threashold)
{
	double norm_value = -1;
	if (origin_value != -1)
	{
		origin_value = origin_value > max_threashold ? max_threashold : origin_value;
		int denominator = max_threashold - min_threashold;
		if (denominator == 0)
		{
			norm_value = 0;
		}
		else
		{
			norm_value = (origin_value - min_threashold) / denominator;
		}
	}
	return norm_value;
}

//vector<int> getTopKIndexs(vector<double> preds, int k)
//{
//	vector<int> topki;
//	if (k > preds.size())
//	{
//		cout << "k must be smaller than preds length!!!" << endl;
//		return topki;
//	}
//	else
//	{
//		double currentValue = 0.0f;
//		for (int i = 0; i < preds.size(); i++)
//		{
//			currentValue = preds.at(i);
//			for (int j = 0; j < topki.size(); j++)
//			{
//				if (topki.at(j) < currentValue)
//				{
//					topki.insert(topki.begin() + (j-1), i);
//				}
//			}
//			if (topki.size() > k)
//			{
//				topki.pop_back();
//			}
//		}
//	}
//	_ASSERT(topki.size() == k, );
//}

vector<size_t> PredictModel::sort_indexes(const vector<double> &v) {

	// initialize original index locations
	vector<size_t> idx(v.size());
	iota(idx.begin(), idx.end(), 0);

	// sort indexes based on comparing values in v
	sort(idx.begin(), idx.end(),
		[&v](size_t i1, size_t i2) {return v[i1] < v[i2];});

	return idx;
}


vector<double> PredictModel::getPredictUnits(vector<double> input_m)
{
	vector<int> input_s;
	input_s.push_back(1);
	int feature_num = 0;
	vector<pair<string, pair<vector<int>, vector<double>>>> params;
	if (enemy_race == BWAPI::Races::Zerg)
	{
		feature_num = ZERG_FEATURE_NUM;
		params = pvz_params;
	}
	else if (enemy_race == BWAPI::Races::Terran)
	{
		feature_num = TERRAN_FEATURE_NUM;
		params = pvt_params;
	}
	else
	{
		feature_num = PROTOSS_FEATURE_NUM;
		params = pvp_params;
	}
	input_s.push_back(feature_num);
	pair<vector<int>, vector<double>> input = make_pair(input_s, input_m);
	pair<vector<int>, vector<double>> result = input;
	bool tanh_flag = false;
	bool ReLU_flag = false;
	bool softmax_flag = false;
	for each (pair<string, pair<vector<int>, vector<double>>> param in params)
	{
		regex weight_e("(.*)\\.weight");
		regex bias_e("(.*)\\.bias");
		smatch bias_match;

		// calculate a linear
		if (std::regex_match(param.first, weight_e))
		{
			pair<vector<int>, vector<double>> temp;
			temp = matrix_dot(result, param.second);
			if (temp.second.size() != 0)
			{
				result = temp;
				//cout << result.second << endl;
			}
		}
		// add bias
		else if (std::regex_search(param.first, bias_match, bias_e))
		{
			pair<vector<int>, vector<double>> temp;
			if (result.second.size() == param.second.second.size())
			{
				for (int i = 0; i < result.second.size(); i++)
				{
					result.second[i] += param.second.second[i];
				}
			}
			// check activate function
			switch (stoi(bias_match.format("$1")))
			{
			case 0:
				tanh_flag = true;
				break;
			case 2:
				ReLU_flag = true;
				break;
			case 4:
				softmax_flag = true;
				break;
			default:
				break;
			}
		}
		// activate function
		if (tanh_flag)
		{
			activation_function_tanh(result.second.data(), result.second.data(), result.second.size());
			tanh_flag = false;
		}
		if (ReLU_flag)
		{
			activation_function_ReLU(result.second.data(), result.second.data(), result.second.size());
			ReLU_flag = false;
		}
		if (softmax_flag)
		{
			activation_function_softmax(result.second.data(), result.second.data(), result.second.size());
			softmax_flag = false;
		}
	}
	// get predict unit
	/*vector<double>::iterator pred_unit;
	pred_unit = max_element(result.second.begin(), result.second.end());
	cout << "build_unit:" << distance(result.second.begin(), pred_unit) << endl;
	return distance(result.second.begin(), pred_unit);*/
	return result.second;
}

void PredictModel::setEnemyRace(BWAPI::Race race)
{
	this->enemy_race = race;
}

void PredictModel::normalize_feature_pvz(vector<double>& features)
{
	features[0] = normalize_min_max(features[0], 60000);
	features[1] = normalize_min_max(features[1], 30000);
	features[2] = normalize_min_max(features[2], 80000);
	features[3] = normalize_min_max(features[3], 10000);
	features[4] = normalize_min_max(features[4], 30000);
	features[5] = normalize_min_max(features[5], 400);
	features[6] = normalize_min_max(features[6], 400);
	features[7] = normalize_min_max(features[7], 50);
	features[8] = normalize_min_max(features[8], 50);
	features[9] = normalize_min_max(features[9], 120);
	features[10] = normalize_min_max(features[10], 25);
	features[11] = normalize_min_max(features[11], 15);
	features[12] = normalize_min_max(features[12], 20);
	features[13] = normalize_min_max(features[13], 5);
	features[14] = normalize_min_max(features[14], 40);
	features[15] = normalize_min_max(features[15], 10);
	features[16] = normalize_min_max(features[16], 20);
	features[17] = normalize_min_max(features[17], 150);
	features[18] = normalize_min_max(features[18], 50);
	features[19] = normalize_min_max(features[19], 30);
	features[20] = normalize_min_max(features[20], 30);
	features[21] = normalize_min_max(features[21], 10);
	features[22] = normalize_min_max(features[22], 8);
	features[23] = normalize_min_max(features[23], 40);
	features[24] = normalize_min_max(features[24], 8);
	features[25] = normalize_min_max(features[25], 20);
	features[26] = normalize_min_max(features[26], 4);
	features[27] = normalize_min_max(features[27], 3);
	features[28] = normalize_min_max(features[28], 70);
	features[29] = normalize_min_max(features[29], 6);
	features[30] = normalize_min_max(features[30], 15);
	features[31] = normalize_min_max(features[31], 3);
	features[32] = normalize_min_max(features[32], 3);
	features[33] = normalize_min_max(features[33], 2);
	features[34] = normalize_min_max(features[34], 3);
	features[35] = normalize_min_max(features[35], 2);
	features[36] = normalize_min_max(features[36], 3);
	features[37] = normalize_min_max(features[37], 3);
	features[38] = normalize_min_max(features[38], 3);
	features[39] = normalize_min_max(features[39], 3);
	features[40] = normalize_min_max(features[40], 3);
	features[41] = normalize_min_max(features[41], 1);
	features[42] = normalize_min_max(features[42], 1);
	features[43] = normalize_min_max(features[43], 3);
	features[44] = normalize_min_max(features[44], 1);
	features[45] = normalize_min_max(features[45], 1);
	features[46] = normalize_min_max(features[46], 1);
	features[47] = normalize_min_max(features[47], 1);
	features[48] = normalize_min_max(features[48], 1);
	features[49] = normalize_min_max(features[49], 1);
	features[50] = normalize_min_max(features[50], 1);
	features[51] = normalize_min_max(features[51], 1);
	features[52] = normalize_min_max(features[52], 1);
	features[53] = normalize_min_max(features[53], 1);
	features[54] = normalize_min_max(features[54], 1);
	features[55] = normalize_min_max(features[55], 1);
	features[56] = normalize_min_max(features[56], 1);
	features[57] = normalize_min_max(features[57], 1);
	features[58] = normalize_min_max(features[58], 1);
	features[59] = normalize_min_max(features[59], 1);
	features[60] = normalize_min_max(features[60], 1);
	features[61] = normalize_min_max(features[61], 1);
	features[62] = normalize_min_max(features[62], 2);
	features[63] = normalize_min_max(features[63], 10);
	features[64] = normalize_min_max(features[64], 100);
	features[65] = normalize_min_max(features[65], 0);
	features[66] = normalize_min_max(features[66], 100);
	features[67] = normalize_min_max(features[67], 0);
	features[68] = normalize_min_max(features[68], 30);
	features[69] = normalize_min_max(features[69], 25);
	features[70] = normalize_min_max(features[70], 30);
	features[71] = normalize_min_max(features[71], 150);
	features[72] = normalize_min_max(features[72], 12);
	features[73] = normalize_min_max(features[73], 20);
	features[74] = normalize_min_max(features[74], 15);
	features[75] = normalize_min_max(features[75], 50);
	features[76] = normalize_min_max(features[76], 35);
	features[77] = normalize_min_max(features[77], 20);
	features[78] = normalize_min_max(features[78], 30);
	features[79] = normalize_min_max(features[79], 20);
	features[80] = normalize_min_max(features[80], 2);
	features[81] = normalize_min_max(features[81], 4);
	features[82] = normalize_min_max(features[82], 8);
	features[83] = normalize_min_max(features[83], 1);
	features[84] = normalize_min_max(features[84], 20);
	features[85] = normalize_min_max(features[85], 2);
	features[86] = normalize_min_max(features[86], 3);
	features[87] = normalize_min_max(features[87], 3);
	features[88] = normalize_min_max(features[88], 30);
	features[89] = normalize_min_max(features[89], 2);
	features[90] = normalize_min_max(features[90], 2);
	features[91] = normalize_min_max(features[91], 3);
	features[92] = normalize_min_max(features[92], 120);
	features[93] = normalize_min_max(features[93], 25);
	features[94] = normalize_min_max(features[94], 2);
}

void PredictModel::normalize_feature_pvt(vector<double>& features)
{
	features[0] = normalize_min_max(features[0], 60000);
	features[1] = normalize_min_max(features[1], 30000);
	features[2] = normalize_min_max(features[2], 80000);
	features[3] = normalize_min_max(features[3], 10000);
	features[4] = normalize_min_max(features[4], 35000);
	features[5] = normalize_min_max(features[5], 400);
	features[6] = normalize_min_max(features[6], 400);
	features[7] = normalize_min_max(features[7], 60);
	features[8] = normalize_min_max(features[8], 50);
	features[9] = normalize_min_max(features[9], 120);
	features[10] = normalize_min_max(features[10], 20);
	features[11] = normalize_min_max(features[11], 15);
	features[12] = normalize_min_max(features[12], 35);
	features[13] = normalize_min_max(features[13], 20);
	features[14] = normalize_min_max(features[14], 20);
	features[15] = normalize_min_max(features[15], 20);
	features[16] = normalize_min_max(features[16], 40);
	features[17] = normalize_min_max(features[17], 150);
	features[18] = normalize_min_max(features[18], 30);
	features[19] = normalize_min_max(features[19], 30);
	features[20] = normalize_min_max(features[20], 40);
	features[21] = normalize_min_max(features[21], 10);
	features[22] = normalize_min_max(features[22], 10);
	features[23] = normalize_min_max(features[23], 40);
	features[24] = normalize_min_max(features[24], 10);
	features[25] = normalize_min_max(features[25], 40);
	features[26] = normalize_min_max(features[26], 6);
	features[27] = normalize_min_max(features[27], 3);
	features[28] = normalize_min_max(features[28], 50);
	features[29] = normalize_min_max(features[29], 8);
	features[30] = normalize_min_max(features[30], 15);
	features[31] = normalize_min_max(features[31], 3);
	features[32] = normalize_min_max(features[32], 2);
	features[33] = normalize_min_max(features[33], 2);
	features[34] = normalize_min_max(features[34], 3);
	features[35] = normalize_min_max(features[35], 2);
	features[36] = normalize_min_max(features[36], 20);
	features[37] = normalize_min_max(features[37], 3);
	features[38] = normalize_min_max(features[38], 3);
	features[39] = normalize_min_max(features[39], 3);
	features[40] = normalize_min_max(features[40], 3);
	features[41] = normalize_min_max(features[41], 1);
	features[42] = normalize_min_max(features[42], 1);
	features[43] = normalize_min_max(features[43], 3);
	features[44] = normalize_min_max(features[44], 1);
	features[45] = normalize_min_max(features[45], 1);
	features[46] = normalize_min_max(features[46], 1);
	features[47] = normalize_min_max(features[47], 1);
	features[48] = normalize_min_max(features[48], 1);
	features[49] = normalize_min_max(features[49], 1);
	features[50] = normalize_min_max(features[50], 1);
	features[51] = normalize_min_max(features[51], 1);
	features[52] = normalize_min_max(features[52], 1);
	features[53] = normalize_min_max(features[53], 1);
	features[54] = normalize_min_max(features[54], 1);
	features[55] = normalize_min_max(features[55], 1);
	features[56] = normalize_min_max(features[56], 1);
	features[57] = normalize_min_max(features[57], 1);
	features[58] = normalize_min_max(features[58], 1);
	features[59] = normalize_min_max(features[59], 1);
	features[60] = normalize_min_max(features[60], 1);
	features[61] = normalize_min_max(features[61], 1);
	features[62] = normalize_min_max(features[62], 8);
	features[63] = normalize_min_max(features[63], 12);
	features[64] = normalize_min_max(features[64], 60);
	features[65] = normalize_min_max(features[65], 50);
	features[66] = normalize_min_max(features[66], 15);
	features[67] = normalize_min_max(features[67], 100);
	features[68] = normalize_min_max(features[68], 60);
	features[69] = normalize_min_max(features[69], 160);
	features[70] = normalize_min_max(features[70], 50);
	features[71] = normalize_min_max(features[71], 100);
	features[72] = normalize_min_max(features[72], 15);
	features[73] = normalize_min_max(features[73], 10);
	features[74] = normalize_min_max(features[74], 2);
	features[75] = normalize_min_max(features[75], 10);
	features[76] = normalize_min_max(features[76], 4);
	features[77] = normalize_min_max(features[77], 50);
	features[78] = normalize_min_max(features[78], 2);
	features[79] = normalize_min_max(features[79], 4);
	features[80] = normalize_min_max(features[80], 10);
	features[81] = normalize_min_max(features[81], 6);
	features[82] = normalize_min_max(features[82], 10);
	features[83] = normalize_min_max(features[83], 8);
	features[84] = normalize_min_max(features[84], 20);
	features[85] = normalize_min_max(features[85], 70);
	features[86] = normalize_min_max(features[86], 8);
	features[87] = normalize_min_max(features[87], 2);
	features[88] = normalize_min_max(features[88], 10);
	features[89] = normalize_min_max(features[89], 25);
	features[90] = normalize_min_max(features[90], 10);
	features[91] = normalize_min_max(features[91], 8);
	features[92] = normalize_min_max(features[92], 2);
	features[93] = normalize_min_max(features[93], 10);
	features[94] = normalize_min_max(features[94], 4);
	features[95] = normalize_min_max(features[95], 1);
}

void PredictModel::normalize_feature_pvp(vector<double>& features)
{
	features[0] = normalize_min_max(features[0], 60000);
	features[1] = normalize_min_max(features[1], 15000);
	features[2] = normalize_min_max(features[2], 80000);
	features[3] = normalize_min_max(features[3], 7000);
	features[4] = normalize_min_max(features[4], 35000);
	features[5] = normalize_min_max(features[5], 400);
	features[6] = normalize_min_max(features[6], 400);
	features[7] = normalize_min_max(features[7], 50);
	features[8] = normalize_min_max(features[8], 50);
	features[9] = normalize_min_max(features[9], 120);
	features[10] = normalize_min_max(features[10], 20);
	features[11] = normalize_min_max(features[11], 15);
	features[12] = normalize_min_max(features[12], 30);
	features[13] = normalize_min_max(features[13], 20);
	features[14] = normalize_min_max(features[14], 15);
	features[15] = normalize_min_max(features[15], 10);
	features[16] = normalize_min_max(features[16], 20);
	features[17] = normalize_min_max(features[17], 250);
	features[18] = normalize_min_max(features[18], 25);
	features[19] = normalize_min_max(features[19], 20);
	features[20] = normalize_min_max(features[20], 20);
	features[21] = normalize_min_max(features[21], 15);
	features[22] = normalize_min_max(features[22], 10);
	features[23] = normalize_min_max(features[23], 45);
	features[24] = normalize_min_max(features[24], 10);
	features[25] = normalize_min_max(features[25], 30);
	features[26] = normalize_min_max(features[26], 3);
	features[27] = normalize_min_max(features[27], 3);
	features[28] = normalize_min_max(features[28], 75);
	features[29] = normalize_min_max(features[29], 4);
	features[30] = normalize_min_max(features[30], 15);
	features[31] = normalize_min_max(features[31], 2);
	features[32] = normalize_min_max(features[32], 2);
	features[33] = normalize_min_max(features[33], 2);
	features[34] = normalize_min_max(features[34], 2);
	features[35] = normalize_min_max(features[35], 2);
	features[36] = normalize_min_max(features[36], 6);
	features[37] = normalize_min_max(features[37], 3);
	features[38] = normalize_min_max(features[38], 3);
	features[39] = normalize_min_max(features[39], 3);
	features[40] = normalize_min_max(features[40], 3);
	features[41] = normalize_min_max(features[41], 1);
	features[42] = normalize_min_max(features[42], 1);
	features[43] = normalize_min_max(features[43], 3);
	features[44] = normalize_min_max(features[44], 1);
	features[45] = normalize_min_max(features[45], 1);
	features[46] = normalize_min_max(features[46], 1);
	features[47] = normalize_min_max(features[47], 1);
	features[48] = normalize_min_max(features[48], 1);
	features[49] = normalize_min_max(features[49], 1);
	features[50] = normalize_min_max(features[50], 1);
	features[51] = normalize_min_max(features[51], 0);
	features[52] = normalize_min_max(features[52], 1);
	features[53] = normalize_min_max(features[53], 1);
	features[54] = normalize_min_max(features[54], 1);
	features[55] = normalize_min_max(features[55], 1);
	features[56] = normalize_min_max(features[56], 1);
	features[57] = normalize_min_max(features[57], 1);
	features[58] = normalize_min_max(features[58], 1);
	features[59] = normalize_min_max(features[59], 1);
	features[60] = normalize_min_max(features[60], 1);
	features[61] = normalize_min_max(features[61], 1);
	features[62] = normalize_min_max(features[62], 20);
	features[63] = normalize_min_max(features[63], 15);
	features[64] = normalize_min_max(features[64], 10);
	features[65] = normalize_min_max(features[65], 35);
	features[66] = normalize_min_max(features[66], 35);
	features[67] = normalize_min_max(features[67], 100);
	features[68] = normalize_min_max(features[68], 15);
	features[69] = normalize_min_max(features[69], 4);
	features[70] = normalize_min_max(features[70], 50);
	features[71] = normalize_min_max(features[71], 15);
	features[72] = normalize_min_max(features[72], 35);
	features[73] = normalize_min_max(features[73], 15);
	features[74] = normalize_min_max(features[74], 250);
	features[75] = normalize_min_max(features[75], 10);
	features[76] = normalize_min_max(features[76], 15);
	features[77] = normalize_min_max(features[77], 8);
	features[78] = normalize_min_max(features[78], 1);
	features[79] = normalize_min_max(features[79], 8);
	features[80] = normalize_min_max(features[80], 2);
	features[81] = normalize_min_max(features[81], 3);
	features[82] = normalize_min_max(features[82], 1);
	features[83] = normalize_min_max(features[83], 3);
	features[84] = normalize_min_max(features[84], 20);
	features[85] = normalize_min_max(features[85], 10);
	features[86] = normalize_min_max(features[86], 2);
	features[87] = normalize_min_max(features[87], 50);
	features[88] = normalize_min_max(features[88], 30);
	features[89] = normalize_min_max(features[89], 4);
	features[90] = normalize_min_max(features[90], 1);
	features[91] = normalize_min_max(features[91], 3);
	features[92] = normalize_min_max(features[92], 10);
	features[93] = normalize_min_max(features[93], 2);
}
