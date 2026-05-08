#include "ML.h"
#include "NetworkDef.h"

u32 ML_Toolbox::generateOneGameDatasSet(const bg::GameContext& sevenWDContext, 
									    AIInterface* AIs[2], void* AIThreadContexts[2], std::vector<Dataset::Point>(&data)[bg::cNumNetworks], bg::WinType& winType, double(&thinkingTime)[2])
{
	using namespace bg;
	GameController game(sevenWDContext);

	thinkingTime[0] = 0.0;
	thinkingTime[1] = 0.0;

	u32 prevPlayerTurn = u32(-1);
	std::vector<Move> moves;
	Move move;
	do
	{
		if (prevPlayerTurn != u32(-1)) {
			data[game.getNetId()].push_back({ game });
			AIs[prevPlayerTurn]->fillPUCTPriors(AIThreadContexts[prevPlayerTurn], data[game.getNetId()].back().m_puctPriors);
		}

		u32 curPlayerTurn = game.getCurrentPlayerTurn();
		game.enumerateMoves(moves);

		{
			using std::chrono::high_resolution_clock;
			using std::chrono::duration_cast;
			using std::chrono::duration;
			using std::chrono::milliseconds;

			auto t1 = high_resolution_clock::now();
			float score;
			std::tie(move, score) = AIs[curPlayerTurn]->selectMove(sevenWDContext, game, moves, AIThreadContexts[curPlayerTurn]);
			auto t2 = high_resolution_clock::now();

			duration<double, std::milli> ms_double = t2 - t1;
			thinkingTime[curPlayerTurn] += ms_double.count();
		}

		prevPlayerTurn = curPlayerTurn;
	} while (!game.play(move));

	winType = game.m_winType;
	DEBUG_ASSERT(game.getWinner() != UINT_MAX);
	return game.getWinner();
}

void ML_Toolbox::Dataset::printStats()
{
	using namespace bg;

	u32 winTypeCounts[(u32)WinType::Count] = {};
	memset(winTypeCounts, 0, sizeof(winTypeCounts));
	u32 winnerCounts[2] = { 0, 0 };

	for (const Point& pt : m_data) {
		const u32 wt = (pt.m_winType == WinType::None) ? 0u : (u32)pt.m_winType;
		if (wt < 4) {
			winTypeCounts[wt]++;
		}

		if (pt.m_winner < 2) {
			winnerCounts[pt.m_winner]++;
		}
	}

	const u32 total = (u32)m_data.size();

	std::cout << "Dataset stats:\n";
	std::cout << "Total points: " << total << "\n";
	std::cout << "Winner counts:\n";
	std::cout << "  Player0: " << winnerCounts[0] << "\n";
	std::cout << "  Player1: " << winnerCounts[1] << "\n";

#if defined(BUILD_FOR_7WDUEL)
	std::cout << "Win type counts:\n";
	std::cout << "  None:     " << winTypeCounts[(u32)WinType::None] << "\n";
	std::cout << "  Civil:    " << winTypeCounts[(u32)WinType::Civil] << "\n";
	std::cout << "  Military: " << winTypeCounts[(u32)WinType::Military] << "\n";
	std::cout << "  Science:  " << winTypeCounts[(u32)WinType::Science] << "\n";

	if (total > 0) {
		auto pct = [total](u32 v) -> double { return 100.0 * double(v) / double(total); };

		std::cout << "Win type %:\n";
		std::cout << "  None:     " << pct(winTypeCounts[(u32)WinType::None]) << "\n";
		std::cout << "  Civil:    " << pct(winTypeCounts[(u32)WinType::Civil]) << "\n";
		std::cout << "  Military: " << pct(winTypeCounts[(u32)WinType::Military]) << "\n";
		std::cout << "  Science:  " << pct(winTypeCounts[(u32)WinType::Science]) << "\n";

		std::cout << "Winner %:\n";
		std::cout << "  Player0:  " << pct(winnerCounts[0]) << "\n";
		std::cout << "  Player1:  " << pct(winnerCounts[1]) << "\n";
	}
#endif
}

void ML_Toolbox::Dataset::prepareForTraining(const bg::GameContext& sevenWDContext, u32(&victoryTypeWeight)[(u32)bg::WinType::Count])
{
	using namespace bg;

	u32 winTypeCounts[(u32)WinType::Count] = {};
	memset(winTypeCounts, 0, sizeof(winTypeCounts));
	u32 winnerCounts[2] = { 0, 0 };

	for (const Point& pt : m_data) {
		const u32 wt = (pt.m_winType == WinType::None) ? 0u : (u32)pt.m_winType;
		if (pt.m_winner < 2) {
			winnerCounts[pt.m_winner]++;
		}
	}

	// Balance win between both player to not bias the dataset
	u32 minNumWin = std::min(winnerCounts[0], winnerCounts[1]);
	winnerCounts[0] = 0;
	winnerCounts[1] = 0;

	auto cpy = std::move(m_data);
	for (const Point& pt : cpy) {
		if (pt.m_winner < 2 && ((winnerCounts[pt.m_winner]++) < minNumWin)) {
			u32 weight = victoryTypeWeight[(u32)pt.m_winType];
			for (u32 i = 0; i < weight; ++i) {
				m_data.push_back(pt);
			}
		}
	}

	std::shuffle(m_data.begin(), m_data.end(), sevenWDContext.rand());
}

void ML_Toolbox::Dataset::fillBatches(
	u32 batchSize,
	std::vector<Batch>& batches,
	bool useExtraTensorData, bool usePUCT) const
{
	using namespace bg;

	const u32 tensorSize = GameController::TensorSize + (useExtraTensorData ? GameController::ExtraTensorSize : 0);

#ifdef USE_TINY_DNN
	// Tiny-dnn: store each batch as vector of vec_t
	for (size_t i = 0; i < m_data.size(); i += batchSize) {
		std::vector<tiny_dnn::vec_t> batchInputs;
		std::vector<tiny_dnn::vec_t> batchLabels;

		for (size_t j = i; j < std::min(i + batchSize, m_data.size()); ++j) {
			tiny_dnn::vec_t input(tensorSize);
			u32 curPlayer = m_data[j].m_state.getCurrentPlayerTurn();
			m_data[j].m_state.fillTensorData(input.data(), curPlayer);
			if (useExtraTensorData)
				m_data[j].m_state.fillExtraTensorData(input.data() + GameController::TensorSize, curPlayer);

			tiny_dnn::vec_t label(1 + (usePUCT ? GameController::cMaxNumMoves : 0));
			label[0] = (m_data[j].m_winner == curPlayer) ? 1.0f : 0.0f;
			if (usePUCT) {
				for (u32 k = 0; k < GameController::cMaxNumMoves; ++k) {
					label[1 + k] = m_data[j].m_puctPriors[k];
				}
			}

			batchInputs.push_back(std::move(input));
			batchLabels.push_back(std::move(label));
		}

		batches.emplace_back();
		batches.back().data = std::move(batchInputs);
		batches.back().labels = std::move(batchLabels);
	}

#else
	// Libtorch version
	std::vector<float> pData(batchSize * tensorSize);
	std::vector<float> pLabels(batchSize);

	float* pWritePtr = pData.data();
	float* pWriteLabelsPtr = pLabels.data();

	for (u32 i = 0; i < m_data.size(); ++i) {
		*pWriteLabelsPtr = (m_data[i].m_winner == 0) ? 1.0f : 0.0f;
		m_data[i].m_state.fillTensorData(pWritePtr, 0);

		pWritePtr += GameState::TensorSize;
		if (useExtraTensorData) {
			m_data[i].m_state.fillExtraTensorData(pWritePtr);
			pWritePtr += GameState::ExtraTensorSize;
		}
		pWriteLabelsPtr++;

		if ((i + 1) % batchSize == 0) {
			batches.emplace_back();
			batches.back().data = torch::from_blob(pData.data(), { int(batchSize), tensorSize }, torch::kFloat).clone();
			batches.back().labels = torch::from_blob(pLabels.data(), { int(batchSize), 1 }, torch::kFloat).clone();

			pWritePtr = pData.data();
			pWriteLabelsPtr = pLabels.data();
		}
	}
#endif
}

#ifdef USE_TINY_DNN
void ML_Toolbox::Dataset::fillBatches(bool useExtraTensorData, bool usePUCT, tiny_dnn::tensor_t& outData, tiny_dnn::tensor_t& outLabels) const
{
	using namespace bg;
	outData.clear();
	outLabels.clear();

	const u32 baseSize = GameController::TensorSize;
	const u32 extraSize = useExtraTensorData ? GameController::ExtraTensorSize : 0;
	const u32 tensorSize = baseSize + extraSize;

	outData.reserve(m_data.size());
	outLabels.reserve(m_data.size());

	for (const Point& pt : m_data) {
		tiny_dnn::vec_t input(tensorSize);
		pt.m_state.fillTensorData(input.data(), 0);
		if (useExtraTensorData) {
			pt.m_state.fillExtraTensorData(input.data() + baseSize, 0);
		}
		outData.emplace_back(std::move(input));

		tiny_dnn::vec_t label(1);
		label[0] = (pt.m_winner == 0) ? 1.0f : 0.0f;
		outLabels.emplace_back(std::move(label));
	}
}
#endif

// ---------------------------------------------------------------------------
// Dataset serialization implementation
// ---------------------------------------------------------------------------
bool ML_Toolbox::Dataset::saveToFile(const std::string& filename) const
{
	using namespace bg;
	std::ofstream os(filename, std::ios::binary);
	if (!os.good()) return false;

	// header
	os.put('7'); os.put('W'); os.put('D'); os.put('S');
	u8 version = 2;
	os.write(reinterpret_cast<const char*>(&version), sizeof(version));

	u32 count = (u32)m_data.size();
	os.write(reinterpret_cast<const char*>(&count), sizeof(count));

	for (const Point& pt : m_data)
	{
		u8 winner = (u8)pt.m_winner;
		u8 winType = (u8)pt.m_winType;
		os.write(reinterpret_cast<const char*>(&winner), sizeof(winner));
		os.write(reinterpret_cast<const char*>(&winType), sizeof(winType));
		os.write(reinterpret_cast<const char*>(pt.m_puctPriors), sizeof(pt.m_puctPriors));

		std::vector<u8> blob = pt.m_state.serializeGameState();
		u32 blobSize = (u32)blob.size();
		os.write(reinterpret_cast<const char*>(&blobSize), sizeof(blobSize));
		if (blobSize > 0)
			os.write(reinterpret_cast<const char*>(blob.data()), blobSize);

		if (!os.good()) return false;
	}

	return os.good();
}

bool ML_Toolbox::Dataset::loadFromFile(const bg::GameContext& context, const std::string& filename)
{
	using namespace bg;
	std::ifstream is(filename, std::ios::binary);
	if (!is.good()) return false;

	// header
	char magic[4];
	is.read(magic, 4);
	if (!is.good()) return false;
	if (magic[0] != '7' || magic[1] != 'W' || magic[2] != 'D' || magic[3] != 'S') return false;

	u8 version = 0;
	is.read(reinterpret_cast<char*>(&version), sizeof(version));
	if (!is.good() || version != 2) return false;

	u32 count = 0;
	is.read(reinterpret_cast<char*>(&count), sizeof(count));
	if (!is.good()) return false;

	m_data.clear();
	m_data.reserve(count);

	for (u32 i = 0; i < count; ++i)
	{
		u8 winner = 0;
		u8 winType = 0;
		float puctPriors[GameController::cMaxNumMoves] = { 0.0f };
		is.read(reinterpret_cast<char*>(&winner), sizeof(winner));
		is.read(reinterpret_cast<char*>(&winType), sizeof(winType));
		is.read(reinterpret_cast<char*>(puctPriors), sizeof(puctPriors));
		if (!is.good()) return false;

		u32 blobSize = 0;
		is.read(reinterpret_cast<char*>(&blobSize), sizeof(blobSize));
		if (!is.good()) return false;

		std::vector<u8> blob(blobSize);
		if (blobSize > 0) {
			is.read(reinterpret_cast<char*>(blob.data()), blobSize);
			if (!is.good()) return false;
		}

		// deserialize into a GameState constructed with provided context
		SerializableGameState state(context);
		if (!state.deserializeGameState(context, blob))
			return false;

		Point pt;
		pt.m_state = std::move(state);
		pt.m_winner = (u32)winner;
		pt.m_winType = (WinType)winType;
		memcpy(pt.m_puctPriors, puctPriors, sizeof(puctPriors));
		m_data.push_back(std::move(pt));
	}

	return true;
}

float ML_Toolbox::evalPrecision(
#ifdef USE_TINY_DNN
	const std::vector<tiny_dnn::vec_t>& predictions,
	const std::vector<tiny_dnn::vec_t>& labels
#else
	torch::Tensor predictions,
	torch::Tensor labels
#endif
)
{
#ifdef USE_TINY_DNN
	float correct = 0;
	for (size_t i = 0; i < predictions.size(); ++i) {
		float pred = std::round(predictions[i][0]);
		float lab = std::round(labels[i][0]);
		correct += fabsf(pred - lab);
	}
	return correct / float(predictions.size());
#else
	predictions = torch::round(predictions);
	auto p = torch::mean(torch::abs(predictions - labels));
	return p.item<float>();
#endif
}

#ifdef USE_TINY_DNN

// cross-entropy loss function for tiny_dnn. But with safe clamping to prevent log(0).
class crossEntropy {
public:
	static float_t f(const tiny_dnn::vec_t& y, const tiny_dnn::vec_t& t) {
		assert(y.size() == t.size());
		float_t d{ 0 };

		for (size_t i = 0; i < y.size(); ++i) {
			float_t yi = std::clamp(y[i], float_t(1e-7), float_t(1) - float_t(1e-7));
			d += -t[i] * std::log(yi) - (float_t(1) - t[i]) * std::log(float_t(1) - yi);
		}

		return d;
	}

	static tiny_dnn::vec_t df(const tiny_dnn::vec_t& y, const tiny_dnn::vec_t& t) {
		assert(y.size() == t.size());
		tiny_dnn::vec_t d(t.size());

		for (size_t i = 0; i < y.size(); ++i) {
			float_t yi = std::clamp(y[i], float_t(1e-7), float_t(1) - float_t(1e-7));
			d[i] = (yi - t[i]) / (yi * (float_t(1) - yi));
		}

		return d;
	}
};

std::pair<float, float> ML_Toolbox::evalMeanLoss(
	const std::vector<tiny_dnn::vec_t>& predictions,
	const std::vector<tiny_dnn::vec_t>& labels,
	const std::vector<float>& weights)
{
	float totalLoss = 0.0f;
	size_t n = predictions.size();

	for (size_t i = 0; i < n; ++i) {
		float p = predictions[i][0];
		float y = labels[i][0];
		float w = (i < weights.size()) ? weights[i] : 1.0f;
		// Binary cross-entropy: -[y*log(p) + (1-y)*log(1-p)]
		p = std::clamp(p, 1e-7f, 1.0f - 1e-7f);
		totalLoss += w * (-y * std::log(p) - (1.0f - y) * std::log(1.0f - p));
	}

	float avgLoss = totalLoss / float(n);

	float acc = evalPrecision(predictions, labels);
	return { avgLoss, acc };
}

void ML_Toolbox::trainNet(u32 age, u32 epoch, const std::vector<Batch>& batches, BaseNN* pNet, float alpha)
{
	// Optimizer (persistent across batches)
	tiny_dnn::momentum optimizer;
	optimizer.alpha = alpha;

	BaseNN::TinyDNN_Net& net = pNet->getNetwork();

	// bool _60percentReached = false;
	// bool _90percentReached = false;
	for (u32 i = 0; i < epoch; ++i) {
		float avgLoss = 0.0f;
		float avgAcc = 0.0f;
		float avgAbsErr = 0.0f;

		int batchId = 0;
		int counter = 0;
		for (const auto& batch : batches) {
			net.fit<crossEntropy>(optimizer, batch.data, batch.labels, batch.data.size(), 1);

			if ((batchId + i) % 8 == 7) {
				float loss = 0;
				float acc = 0;
				float absError = 0;
				for (size_t b = 0; b < batch.data.size(); ++b) {
					tiny_dnn::vec_t y = net.predict(batch.data[b]);
					loss += crossEntropy::f(y, batch.labels[b]);
					acc += (std::round(y[0]) == std::round(batch.labels[b][0])) ? 1.0f : 0.0f;

					// Normalize PUCT move policy part
					float sum = 1e-7f;
					for (u32 l=1 ; l<y.size() ; ++l) 
						sum += y[l];
					for (u32 l = 1; l < y.size(); ++l) {
						y[l] /= sum;
						absError += fabsf(y[l] - batch.labels[b][l]);
					}
				}
				loss /= float(batch.data.size());
				acc /= float(batch.data.size());
				absError /= float(batch.data.size());
				
				avgAcc += acc;
				avgLoss += loss;
				avgAbsErr += absError;
				counter++;
			}
			batchId++;
		}

		avgLoss /= std::max(1, counter);
		avgAcc /= std::max(1, counter);
		avgAbsErr /= std::max(1, counter);

		std::cout << std::setprecision(4) << "Epoch:" << i << "/" << epoch << " | ";

		for (u32 j = 0; j < age; ++j)
			std::cout << "                                ";

		std::cout << "Loss:" << avgLoss << " | Acc: " << avgAcc << " | Alpha:" << optimizer.alpha << std::endl;
		// if (!_60percentReached && float(i) >= epoch * 0.6f) {
		// 	optimizer.alpha *= 0.0f;
		// 	_60percentReached = true;
		// } else if (!_90percentReached && float(i) >= epoch * 0.9f) {
		// 	optimizer.alpha *= 0.1f;
		// 	_90percentReached = true;
		// }
	}
}

void ML_Toolbox::trainNet(u32 age, u32 epoch, tiny_dnn::tensor_t& data, tiny_dnn::tensor_t& labels, BaseNN* pNet)
{
	tiny_dnn::adam optimizer;
	optimizer.alpha = float(1e-4);

	BaseNN::TinyDNN_Net& net = pNet->getNetwork();
	size_t batch_size = 64;

	int epoch_counter = 0;

	net.fit<tiny_dnn::mse, tiny_dnn::adam>(
		optimizer,
		data,
		labels,
		batch_size,
		epoch,

		// minibatch callback (optional)
		[&]() {
			// this runs after each minibatch; you can log time, etc.
		},

		// epoch callback
		[&]() {
			float loss = net.get_loss<tiny_dnn::mse>(data, labels);

			std::cout << std::setprecision(4) << "Epoch:" << epoch_counter++ << "/" << epoch << " | ";

			for (u32 j = 0; j < age; ++j)
				std::cout << "                                ";

			std::cout << "Loss:" << loss << std::endl;
		}, 

		false, 16);
}

#else // libtorch

std::pair<float, float> ML_Toolbox::evalMeanLoss(torch::Tensor predictions,
	torch::Tensor labels,
	torch::Tensor weights)
{
	torch::NoGradGuard _{};
	torch::Tensor loss = torch::binary_cross_entropy(predictions, labels, weights);
	return { loss.item<float>(), evalPrecision(predictions, labels) };
}

void ML_Toolbox::trainNet(u32 age, u32 epoch, const std::vector<Batch>& batches, BaseNN* pNet)
{
	using OptimizerType = torch::optim::AdamW;
	std::unique_ptr<OptimizerType> optimizer =
		std::make_unique<OptimizerType>(pNet->parameters(), 1e-4);

	for (u32 i = 0; i < epoch; ++i)
	{
		float avgLoss = 0;
		float avgPrecision = 0;

		for (u32 b = 0; b < batches.size(); ++b)
		{
			optimizer->zero_grad();
			torch::Tensor prediction = pNet->forward(batches[b].data);
			torch::Tensor loss = torch::binary_cross_entropy(prediction, batches[b].labels);

			loss.backward();
			optimizer->step();

			avgLoss += loss.item<float>();
			avgPrecision += ML_Toolbox::evalPrecision(prediction, batches[b].labels);
		}

		avgLoss /= batches.size();
		avgPrecision /= batches.size();

		std::cout << std::setprecision(4) << "Epoch:" << i << "/" << epoch << " | ";

		for (u32 j = 0; j < age; ++j)
			std::cout << "                                ";

		std::cout << "Loss:" << avgLoss << " | Acc: " << avgPrecision << std::endl;
	}
}

#endif

std::string ML_Toolbox::buildNetFilename(std::string netName, std::string namePrefix, bool useExtraTensorData, u32 age, u32 generation) {
	std::stringstream str;
	str << bg::s_bgPrefix << "_Dataset/net_" << netName << (useExtraTensorData ? "_extra" : "_base") << "_" << namePrefix << "_gen" << generation << "_age" << age;
#if defined(USE_TINY_DNN)
	str << "_tinydnn";
#endif
	str << ".bin";
	return str.str();
}

u32 ML_Toolbox::parseGenerationFromNetFilename(std::string filename) {
	const char* begin = strstr(filename.c_str(), "_gen") + strlen("_gen");
	const char* end = strstr(filename.c_str(), "_age");
	std::string_view genStr(begin, end - begin);
	char buffer[16] = {};
	memcpy(buffer, genStr.data(), genStr.size());
	return (u32)atoi(buffer);
}

void ML_Toolbox::saveNet(std::string namePrefix, u32 generation, const std::array<std::shared_ptr<BaseNN>, bg::cNumNetworks>& net)
{
	for (u32 i = 0; i < bg::cNumNetworks; ++i) {
		std::string filename = buildNetFilename(net[i]->getNetName(), namePrefix, net[i]->m_extraTensorData, i, generation);
#ifdef USE_TINY_DNN
		net[i]->getNetwork().save(filename);
#else
		torch::save(net[i], buildNetFilename(net[i]->getNetName(), namePrefix, net[i]->m_extraTensorData, i, generation));
#endif
	}
}

std::shared_ptr<BaseNN> ML_Toolbox::constructNet(NetworkType type, bool hasExtraData)
{
	switch (type) {
	case NetworkType::Net_BaseLine:
		return std::make_shared<BaseLine>(type, hasExtraData);
	case NetworkType::Net_TwoLayer8:
		return std::make_shared<TwoLayers8>(type, hasExtraData);
	case NetworkType::Net_TwoLayer24:
		return std::make_shared<TwoLayers24>(type, hasExtraData);
	case NetworkType::Net_TwoLayer64:
		return std::make_shared<TwoLayers64>(type, hasExtraData);
	case NetworkType::Net_TwoLayer4_PUCT:
		return std::make_shared<TwoLayersPUCT<4>>(type);
	case NetworkType::Net_TwoLayer8_PUCT:
		return std::make_shared<TwoLayersPUCT<8>>(type);
	case NetworkType::Net_TwoLayer16_PUCT:
		return std::make_shared<TwoLayersPUCT<16>>(type);
	case NetworkType::Net_TwoLayer32_PUCT:
		return std::make_shared<TwoLayersPUCT<32>>(type);
	case NetworkType::Net_ThreeLayer32_PUCT:
		return std::make_shared<ThreeLayersPUCT<32>>(type);
	case NetworkType::Net_FourLayer32_PUCT:
		return std::make_shared<FourLayersPUCT<32>>(type);
	default:
		return nullptr;
	}
}

bool ML_Toolbox::loadNet(NetworkType netType, std::string namePrefix, u32 generation, std::array<std::shared_ptr<BaseNN>, bg::cNumNetworks>& net, bool useExtraTensorData)
{
	for (u32 i = 0; i < bg::cNumNetworks; ++i) {
		std::string filename = buildNetFilename(BaseNN::getNetworkName(netType), namePrefix, useExtraTensorData, i, generation);
		if (std::filesystem::exists(filename)) {
			net[i] = constructNet(netType, useExtraTensorData);
#ifdef USE_TINY_DNN
			net[i]->getNetwork().load(filename);
#else
			torch::load(net[i], filename);
#endif
			net[i]->prepareAfterLoad();
		}
		else return false;
	}
	return true;
}

bool ML_Toolbox::loadLastGenNet(NetworkType netType, std::string namePrefix, bool useExtraTensorData, u32& outGeneration, std::array<std::shared_ptr<BaseNN>, bg::cNumNetworks>& net, std::string& outFullName)
{
	std::vector<std::string> networksFilenames[bg::cNumNetworks];
	try {
		std::string folder = std::string(bg::s_bgPrefix) + "_Dataset/";
		for (const auto& entry : std::filesystem::directory_iterator(folder.c_str())) {
			if (!entry.is_regular_file()) continue;
			std::string filename = entry.path().filename().u8string();
			std::string networkName = std::string(BaseNN::getNetworkName(netType)) + (useExtraTensorData ? "_extra" : "_base") + "_" + namePrefix;
			if (filename.find(networkName) != std::string::npos) {
				if constexpr (bg::cNumNetworks > 0) { if (filename.find("_age0") != std::string::npos) networksFilenames[0].push_back(filename); }
				if constexpr (bg::cNumNetworks > 1) { if (filename.find("_age1") != std::string::npos) networksFilenames[1].push_back(filename); }
				if constexpr (bg::cNumNetworks > 2) { if (filename.find("_age2") != std::string::npos) networksFilenames[2].push_back(filename); }
			}
		}
	}
	catch (std::exception e) {
		return false;
	}

	if (networksFilenames[0].empty() || networksFilenames[0].size() != networksFilenames[1].size() || networksFilenames[0].size() != networksFilenames[2].size())
		return false;

	for (int netId = 0; netId < bg::cNumNetworks; ++netId) std::sort(networksFilenames[netId].begin(), networksFilenames[netId].end());

	u32 index = 0;
	u32 mostRecentGen = 0;
	for (size_t i = 0; i < networksFilenames[0].size(); ++i) {
		u32 gen = parseGenerationFromNetFilename(networksFilenames[0][i]);
		if (gen >= mostRecentGen) {
			mostRecentGen = gen;
			index = (u32)i;
		}
	}

	for (int netId = 0; netId < bg::cNumNetworks; ++netId) {
		net[netId] = constructNet(netType, useExtraTensorData);
		std::string netFilePath = std::string(bg::s_bgPrefix) + "_Dataset/" + networksFilenames[netId][index];
#ifdef USE_TINY_DNN
		net[netId]->getNetwork().load(netFilePath);
#else
		torch::load(net[netId], netFilePath);
#endif
		net[netId]->prepareAfterLoad();
	}

	std::stringstream networkName;
	networkName << BaseNN::getNetworkName(netType) << (useExtraTensorData ? "_extra" : "_base") << "_" << namePrefix << "_gen" << mostRecentGen;
	outFullName = networkName.str();
	outGeneration = mostRecentGen;
	return true;
}

tiny_dnn::vec_t BaseNN::forward(const tiny_dnn::vec_t& x, void* pThreadContext, u32 netAge)
{
	BaseNetworkAI::ThreadContext* pCtx = reinterpret_cast<BaseNetworkAI::ThreadContext*>(pThreadContext);
	if (pCtx) {
		return pCtx->m_net[netAge].predict(x);
	}
	else {
		return m_net.predict(x);
	}
}