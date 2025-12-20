#include "ML.h"

u32 ML_Toolbox::generateOneGameDatasSet(const sevenWD::GameContext& sevenWDContext, 
	sevenWD::AIInterface* AIs[2], void* AIThreadContexts[2], std::vector<sevenWD::GameState>(&data)[3], sevenWD::WinType& winType, double(&thinkingTime)[2])
{
	using namespace sevenWD;
	GameController game(sevenWDContext);

	thinkingTime[0] = 0.0;
	thinkingTime[1] = 0.0;

	std::vector<Move> moves;
	Move move;
	do
	{
		if (game.m_gameState.getNumTurnPlayed() > 0) {
			data[game.m_gameState.getCurrentAge()].push_back(game.m_gameState);
		}

		u32 curPlayerTurn = game.m_gameState.getCurrentPlayerTurn();
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
	} while (!game.play(move));

	winType = game.m_winType;
	return game.m_state == GameController::State::WinPlayer0 ? 0 : 1;
}

void ML_Toolbox::Dataset::fillBatches(
	u32 batchSize,
	std::vector<Batch>& batches,
	bool useExtraTensorData) const
{
	using namespace sevenWD;

	const u32 tensorSize = GameState::TensorSize + (useExtraTensorData ? GameState::ExtraTensorSize : 0);

#ifdef USE_TINY_DNN
	// Tiny-dnn: store each batch as vector of vec_t
	for (size_t i = 0; i < m_data.size(); i += batchSize) {
		std::vector<tiny_dnn::vec_t> batchInputs;
		std::vector<tiny_dnn::vec_t> batchLabels;

		for (size_t j = i; j < std::min(i + batchSize, m_data.size()); ++j) {
			tiny_dnn::vec_t input(tensorSize);
			m_data[j].m_state.fillTensorData(input.data(), 0);
			if (useExtraTensorData)
				m_data[j].m_state.fillExtraTensorData(input.data() + GameState::TensorSize);

			tiny_dnn::vec_t label(1);
			label[0] = (m_data[j].m_winner == 0) ? 1.0f : 0.0f;

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
void ML_Toolbox::Dataset::fillBatches(bool useExtraTensorData, tiny_dnn::tensor_t& outData, tiny_dnn::tensor_t& outLabels) const
{
	outData.clear();
	outLabels.clear();

	const u32 baseSize = sevenWD::GameState::TensorSize;
	const u32 extraSize = useExtraTensorData ? sevenWD::GameState::ExtraTensorSize : 0;
	const u32 tensorSize = baseSize + extraSize;

	outData.reserve(m_data.size());
	outLabels.reserve(m_data.size());

	for (const Point& pt : m_data) {
		tiny_dnn::vec_t input(tensorSize);
		pt.m_state.fillTensorData(input.data(), 0);
		if (useExtraTensorData) {
			pt.m_state.fillExtraTensorData(input.data() + baseSize);
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
	using namespace sevenWD;
	std::ofstream os(filename, std::ios::binary);
	if (!os.good()) return false;

	// header
	os.put('7'); os.put('W'); os.put('D'); os.put('S');
	u8 version = 1;
	os.write(reinterpret_cast<const char*>(&version), sizeof(version));

	u32 count = (u32)m_data.size();
	os.write(reinterpret_cast<const char*>(&count), sizeof(count));

	for (const Point& pt : m_data)
	{
		u8 winner = (u8)pt.m_winner;
		u8 winType = (u8)pt.m_winType;
		os.write(reinterpret_cast<const char*>(&winner), sizeof(winner));
		os.write(reinterpret_cast<const char*>(&winType), sizeof(winType));

		std::vector<u8> blob = sevenWD::Helper::serializeGameState(pt.m_state);
		u32 blobSize = (u32)blob.size();
		os.write(reinterpret_cast<const char*>(&blobSize), sizeof(blobSize));
		if (blobSize > 0)
			os.write(reinterpret_cast<const char*>(blob.data()), blobSize);

		if (!os.good()) return false;
	}

	return os.good();
}

bool ML_Toolbox::Dataset::loadFromFile(const sevenWD::GameContext& context, const std::string& filename)
{
	using namespace sevenWD;
	std::ifstream is(filename, std::ios::binary);
	if (!is.good()) return false;

	// header
	char magic[4];
	is.read(magic, 4);
	if (!is.good()) return false;
	if (magic[0] != '7' || magic[1] != 'W' || magic[2] != 'D' || magic[3] != 'S') return false;

	u8 version = 0;
	is.read(reinterpret_cast<char*>(&version), sizeof(version));
	if (!is.good() || version != 1) return false;

	u32 count = 0;
	is.read(reinterpret_cast<char*>(&count), sizeof(count));
	if (!is.good()) return false;

	m_data.clear();
	m_data.reserve(count);

	for (u32 i = 0; i < count; ++i)
	{
		u8 winner = 0;
		u8 winType = 0;
		is.read(reinterpret_cast<char*>(&winner), sizeof(winner));
		is.read(reinterpret_cast<char*>(&winType), sizeof(winType));
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
		GameState state(context);
		if (!sevenWD::Helper::deserializeGameState(context, blob, state))
			return false;

		Point pt;
		pt.m_state = std::move(state);
		pt.m_winner = (u32)winner;
		pt.m_winType = (WinType)winType;
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

void ML_Toolbox::trainNet(u32 age, u32 epoch, const std::vector<Batch>& batches, BaseNN* pNet)
{
	// Optimizer (persistent across batches)
	tiny_dnn::adam optimizer;
	optimizer.alpha = float(1e-4);

	BaseNN::TinyDNN_Net& net = pNet->getNetwork();

	for (u32 i = 0; i < epoch; ++i) {
		float avgLoss = 0.0f;
		float avgAcc = 0.0f;

		int batchId = 0;
		int counter = 0;
		for (const auto& batch : batches) {
			net.fit<tiny_dnn::mse, tiny_dnn::adam>(optimizer, batch.data, batch.labels, batch.data.size(), 1);

			if (batchId % 16 == 15) {
				std::vector<tiny_dnn::vec_t> prediction = net.predict(batch.data);
				auto [loss, acc] = evalMeanLoss(prediction, batch.labels, {});
				avgAcc += acc;
				avgLoss += loss;
				counter++;
			}
			batchId++;
		}

		avgLoss /= std::max(1, counter);
		avgAcc /= std::max(1, counter);

		std::cout << std::setprecision(4)
			<< "Epoch:" << i << "/" << epoch << " | ";

		for (u32 j = 0; j < age; ++j)
			std::cout << "                                ";

		std::cout << "Loss:" << avgLoss << " | Acc: " << avgAcc << std::endl;
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
	str << "../7wDataset/net_" << netName << (useExtraTensorData ? "_extra" : "_base") << "_" << namePrefix << "_gen" << generation << "_age" << age;
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

void ML_Toolbox::saveNet(std::string namePrefix, u32 generation, const std::array<std::shared_ptr<BaseNN>, 3>& net)
{
	for (u32 i = 0; i < 3; ++i) {
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
	case NetworkType::Net_TwoLayer64:
		return std::make_shared<TwoLayers64>(type, hasExtraData);
	default:
		return nullptr;
	}
}

bool ML_Toolbox::loadNet(NetworkType netType, std::string namePrefix, u32 generation, std::array<std::shared_ptr<BaseNN>, 3>& net, bool useExtraTensorData)
{
	for (u32 i = 0; i < 3; ++i) {
		std::string filename = buildNetFilename(BaseNN::getNetworkName(netType), namePrefix, useExtraTensorData, i, generation);
		if (std::filesystem::exists(filename)) {
			net[i] = constructNet(netType, useExtraTensorData);
#ifdef USE_TINY_DNN
			net[i]->getNetwork().load(filename);
#else
			torch::load(net[i], filename);
#endif
		}
		else return false;
	}
	return true;
}

bool ML_Toolbox::loadLastGenNet(NetworkType netType, std::string namePrefix, bool useExtraTensorData, u32& outGeneration, std::array<std::shared_ptr<BaseNN>, 3>& net, std::string& outFullName)
{
	std::vector<std::string> networksFilenames[3];
	try {
		for (const auto& entry : std::filesystem::directory_iterator("../7wDataset")) {
			if (!entry.is_regular_file()) continue;
			std::string filename = entry.path().filename().u8string();
			std::string networkName = std::string(BaseNN::getNetworkName(netType)) + (useExtraTensorData ? "_extra" : "_base") + "_" + namePrefix;
			if (filename.find(networkName) != std::string::npos) {
				if (filename.find("_age0") != std::string::npos) networksFilenames[0].push_back(filename);
				else if (filename.find("_age1") != std::string::npos) networksFilenames[1].push_back(filename);
				else if (filename.find("_age2") != std::string::npos) networksFilenames[2].push_back(filename);
			}
		}
	}
	catch (std::exception e) {
		return false;
	}

	if (networksFilenames[0].empty() || networksFilenames[0].size() != networksFilenames[1].size() || networksFilenames[0].size() != networksFilenames[2].size())
		return false;

	for (int age = 0; age < 3; ++age) std::sort(networksFilenames[age].begin(), networksFilenames[age].end());

	u32 index = 0;
	u32 mostRecentGen = 0;
	for (size_t i = 0; i < networksFilenames[0].size(); ++i) {
		u32 gen = parseGenerationFromNetFilename(networksFilenames[0][i]);
		if (gen >= mostRecentGen) {
			mostRecentGen = gen;
			index = (u32)i;
		}
	}

	for (int age = 0; age < 3; ++age) {
		net[age] = constructNet(netType, useExtraTensorData);
		std::string netFilePath = std::string("../7wDataset/") + networksFilenames[age][index];
#ifdef USE_TINY_DNN
		net[age]->getNetwork().load(netFilePath);
#else
		torch::load(net[age], netFilePath);
#endif
	}

	std::stringstream networkName;
	networkName << BaseNN::getNetworkName(netType) << (useExtraTensorData ? "_extra" : "_base") << "_" << namePrefix << "_gen" << mostRecentGen;
	outFullName = networkName.str();
	outGeneration = mostRecentGen;
	return true;
}