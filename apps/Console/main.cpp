#include <array>
#include <iostream>
#include <windows.h>
#include <execution>

#include "7WDuel/GameController.h"

#include "AI/AI.h"
#include "AI/ML.h"
#include "AI/MCTS.h"
#include "AI/Tournament.h"
#include "Core/cxxopts.h"
#include "Core/StringUtil.h"

using namespace std::chrono;
using namespace sevenWD;

static NetworkType parseNetType(const std::string& netTypeStr)
{
    if (netTypeStr == "BaseLine") return NetworkType::Net_BaseLine;
    else if (netTypeStr == "TwoLayers8") return NetworkType::Net_TwoLayer8;
    else if (netTypeStr == "TwoLayers24") return NetworkType::Net_TwoLayer24;
    else if (netTypeStr == "TwoLayers64") return NetworkType::Net_TwoLayer64;
    else if (netTypeStr == "TwoLayers4_PUCT") return NetworkType::Net_TwoLayer4_PUCT;
    else if (netTypeStr == "TwoLayers8_PUCT") return NetworkType::Net_TwoLayer8_PUCT;
    else if (netTypeStr == "TwoLayers16_PUCT") return NetworkType::Net_TwoLayer16_PUCT;
    else if (netTypeStr == "TwoLayers32_PUCT") return NetworkType::Net_TwoLayer32_PUCT;
    return NetworkType::Net_BaseLine;
}

static sevenWD::AIInterface* createAIByName(const std::string& name)
{
	using namespace StringUtil;

    // Map textual name -> AI instance. Add more mappings as needed.
    if (name == "RandAI")
        return new RandAI();

    if (name.rfind("MonteCarloAI", 0) == 0) {
        u32 numSimulations = 100;
        // expected forms: MonteCarloAI or MonteCarloAI(100)
        std::string inner;
        if (extractBetweenParentheses(name, inner)) {
            if (!parseUint(trim_copy(inner), numSimulations)) {
                std::cout << "MonteCarloAI: invalid parameter '" << inner << "'" << std::endl;
                return nullptr;
            }
        }
        return new MonteCarloAI(numSimulations);
    }

    if (name.rfind("MCTS_Simple", 0) == 0) {
        // "MCTS_Simple(numSimu;depth;modelName;netName)" -> strict 4-part form, semicolon-separated
        std::string inner;
        if (!extractBetweenParentheses(name, inner)) {
            std::cout << "MCTS_Simple: missing parameter list" << std::endl;
            return nullptr;
        }

        auto parts = split_char(inner, ';'); // strict semicolon split
        if (parts.size() != 4) {
            std::cout << "MCTS_Simple: expect exactly 4 fields (numSimu;depth;modelName;netName) got " << parts.size() << std::endl;
            return nullptr;
        }

        u32 numSimu = 0;
        u32 depth = 0;
        if (!parseUint(trim_copy(parts[0]), numSimu)) {
            std::cout << "MCTS_Simple: invalid numSimu '" << parts[0] << "'" << std::endl;
            return nullptr;
        }
        if (!parseUint(trim_copy(parts[1]), depth)) {
            std::cout << "MCTS_Simple: invalid depth '" << parts[1] << "'" << std::endl;
            return nullptr;
        }

        std::string modelName = trim_copy(parts[2]);
        std::string netName   = trim_copy(parts[3]);

        if (modelName.empty() || netName.empty()) {
            std::cout << "MCTS_Simple: modelName and netName must not be empty" << std::endl;
            return nullptr;
        }

        // Attempt to load network-backed AI. If load fails, do NOT silently fallback to defaults.
        auto loaded = ML_Toolbox::loadAIFromFile<MCTS_Simple>(parseNetType(modelName), netName, false);
        MCTS_Simple* pAI = loaded.first;
        if (!pAI) {
            std::cout << "MCTS_Simple: failed to load AI for model '" << modelName << "' net '" << netName << "'" << std::endl;
            return nullptr;
        }

        pAI->m_numSimu = numSimu;
        pAI->m_depth = depth;
        return pAI;
    }

	bool isMCTS_Deterministic = name.rfind("MCTS_Deterministic", 0) == 0;
	bool isMCTS_Zero = name.rfind("MCTS_Zero", 0) == 0;
    if (isMCTS_Deterministic || isMCTS_Zero) {

		const char* prefix = isMCTS_Deterministic ? "MCTS_Deterministic" : "MCTS_Zero";

        u32 numMoves = 1000;
        u32 numSimu = 20;
        std::string inner;

        // If no parentheses -> use defaults
        if (!extractBetweenParentheses(name, inner)) {
            std::cout << prefix << ": invalid parenthesis" << std::endl;
            return nullptr;
        }

        // Strict semicolon split for main forms
        auto parts = split_char(inner, ';');

        if (parts.size() <= 3) {
            // form: MCTS_Deterministic(numMoves;numSimu)
            if (!parseUint(trim_copy(parts[0]), numMoves)) {
                std::cout << prefix  << ": invalid numMoves '" << parts[0] << "'" << std::endl;
                return nullptr;
            }
            if (!parseUint(trim_copy(parts[1]), numSimu)) {
                std::cout << prefix << ": invalid numSimu '" << parts[1] << "'" << std::endl;
                return nullptr;
            }

			MCTS_Deterministic::HeuristicType heuristic = MCTS_Deterministic::Heuristic_RandomRollout;
            if (parts.size() == 3) {
                std::string heuristicStr = trim_copy(parts[2]);
                if (heuristicStr == "NoBurn") {
                    heuristic = MCTS_Deterministic::Heuristic_NoBurnRollout;
                }
            }

            if (isMCTS_Zero) {
                MCTS_Zero* pAI = new MCTS_Zero(numMoves, numSimu);
				return pAI;
            } else {
                MCTS_Deterministic* pAI = new MCTS_Deterministic(numMoves, numSimu);
                pAI->m_heuristic = heuristic;
                return pAI;
            }
        }
        else if (parts.size() >= 4) {
            // form: MCTS_Deterministic(numMoves;numSimu;modelName;netName)
            if (!parseUint(trim_copy(parts[0]), numMoves)) {
                std::cout << prefix << ": invalid numMoves '" << parts[0] << "'" << std::endl;
                return nullptr;
            }
            if (!parseUint(trim_copy(parts[1]), numSimu)) {
                std::cout << prefix << ": invalid numSimu '" << parts[1] << "'" << std::endl;
                return nullptr;
            }

            std::string modelName = trim_copy(parts[2]);
            std::string netName = trim_copy(parts[3]);
            if (modelName.empty() || netName.empty()) {
                std::cout << prefix << ": modelName and netName must not be empty" << std::endl;
                return nullptr;
            }

            if (isMCTS_Zero) {
                auto loaded = ML_Toolbox::loadAIFromFile<MCTS_Zero>(parseNetType(modelName), netName, true);
                MCTS_Zero* pAI = loaded.first;
                if (!pAI) {
                    std::cout << prefix << ": failed to load AI for model '" << modelName << "' net '" << netName << "'" << std::endl;
                    return nullptr;
                }
                pAI->m_numMoves = numMoves;
                pAI->m_numSampling = numSimu;

                if (parts.size() >= 5) {
                    parseFloat(trim_copy(parts[4]), pAI->C);
                }
                if (parts.size() == 6) {
                    parseFloat(trim_copy(parts[5]), pAI->m_scienceBoost);
                }

                return pAI;
            }
            else {
                auto loaded = ML_Toolbox::loadAIFromFile<MCTS_Deterministic>(parseNetType(modelName), netName, false);
                MCTS_Deterministic* pAI = loaded.first;
                if (!pAI) {
                    std::cout << prefix << ": failed to load AI for model '" << modelName << "' net '" << netName << "'" << std::endl;
                    return nullptr;
                }
                pAI->m_heuristic = MCTS_Deterministic::Heuristic_UseDNN;
                pAI->m_numMoves = numMoves;
                pAI->m_numSampling = numSimu;
                return pAI;
            }
        }
        else {
            std::cout << prefix << ": unexpected parameter count, expected 0,2 or 4 fields (got " << parts.size() << ")" << std::endl;
            return nullptr;
        }
     }
    // Network-based AI loader: expects filename to contain network info (not handled here)
    // Fallback: null
    return nullptr;
}

int main(int argc, char** argv)
{
    try {
        cxxopts::Options options("Play7WDuel", "Console tool: generate dataset or train network");
        options.add_options()
            ("mode", "Mode: generate or train or stats", cxxopts::value<std::string>()->default_value("generate"))
            ("size", "Dataset size (number of games)", cxxopts::value<uint32_t>()->default_value("100"))
            // allow multiple --ai entries, default is two AIs (RandAI and MonteCarloAI)
            ("ai", "AI to include in generation (repeatable).\nList: RandAI MonteCarloAI(numSimu) MCTS_Simple(numSimu;depth;modelName;netName) MCTS_Deterministic(numMove, numSimu)",
                 cxxopts::value<std::vector<std::string>>()->default_value("RandAI,MonteCarloAI"))
            ("in", "Input filename prefix (for dataset or nets)", cxxopts::value<std::string>()->default_value(""))
            ("out", "Output filename prefix (for dataset or nets)", cxxopts::value<std::string>()->default_value(""))
            ("net", "Network type for training: BaseLine, TwoLayer8, TwoLayer64", cxxopts::value<std::string>()->default_value("TwoLayer8"))
            ("gen", "Generatio of the network, only impact out filename.", cxxopts::value<u32>()->default_value("0"))
            ("extra", "Use extra tensor data for network", cxxopts::value<bool>()->default_value("false"))
            ("epochs", "Training epochs", cxxopts::value<uint32_t>()->default_value("16"))
            ("batch", "Batch size as \"age1;age2;age3\"", cxxopts::value<std::string>()->default_value("32;32;32"))
            ("alpha", "Learning rate for optimizer as \"age1;age2;age3\"", cxxopts::value<std::string>()->default_value("0.001;0.001;0.001"))
            ("threads", "Num threads", cxxopts::value<uint32_t>()->default_value("16"))
            ("help", "Print help");

        auto result = options.parse(argc, argv);

        if (result.count("help")) {
            std::cout << options.help({ "" }) << std::endl;
            return 0;
        }

        std::string mode = result["mode"].as<std::string>();
        std::string outPrefix = result["out"].as<std::string>();
        std::string inPrefix = result["in"].as<std::string>();

        if (mode == "generate") {
            uint32_t size = result["size"].as<uint32_t>();

            // read multiple --ai entries
            std::vector<std::string> aiNames = result["ai"].as<std::vector<std::string>>();

            // build GameContext with time seed
            GameContext context((unsigned)time(nullptr));

            Tournament tournament;

			u32 numAIsAdded = 0;
            for (const auto& name : aiNames) {
                sevenWD::AIInterface* p = createAIByName(name);
                if (!p) {
                    std::cout << "Unknown AI name: " << name << " - skipping" << std::endl;
                    continue;
                }
                tournament.addAI(p);
                numAIsAdded++;
            }

            if (numAIsAdded < 2) {
                std::cout << "Need at least two valid AIs to generate dataset" << std::endl;
                return 1;
            }

            if (inPrefix.empty() == false) {
                // Load existing dataset to append to
                tournament.deserializeDataset(inPrefix);
			}

            uint32_t numThreads = result["threads"].as<uint32_t>();
            std::cout << "Generating dataset of " << size << " games using " << numAIsAdded << " AIs with " << numThreads << " threads" << std::endl;
            tournament.generateDataset(context, size, numThreads);
            tournament.print();

            tournament.serializeDataset(outPrefix);
            // Write a copy if
            {
                std::stringstream ss;
                ss << "_gen" << std::chrono::duration_cast<std::chrono::seconds>(system_clock::now().time_since_epoch()).count();
                std::string outPrefixCpy = "copy_" + outPrefix + ss.str();
                tournament.serializeDataset(outPrefixCpy);
            }

            std::cout << "Dataset generation complete. Files written with prefix: " << outPrefix << std::endl;
            return 0;
        }
        else if (mode == "train") {
            // Train networks from an existing serialized dataset
            std::string netTypeStr = result["net"].as<std::string>();
            bool useExtra = result["extra"].as<bool>();
            uint32_t epochs = result["epochs"].as<uint32_t>();

            uint32_t batchSizes[3] = { 32, 32, 32 };
            {
                auto batchStrs = StringUtil::split_char(result["batch"].as<std::string>(), ';');
                if (batchStrs.size() != 3) {
                    std::cout << "--batch must have 3 semicolon-separated values" << std::endl;
                    return 1;
                }
                for (size_t i = 0; i < 3; ++i) {
                    try {
                        batchSizes[i] = std::stoul(StringUtil::trim_copy(batchStrs[i]));
                    }
                    catch (...) {
                        std::cout << "Invalid uint value for batch[" << i << "]: " << batchStrs[i] << std::endl;
                        return 1;
                    }
                }
            };

            float alphas[3] = { 0.001f, 0.001f, 0.001f };
            {
                auto alphaStrs = StringUtil::split_char(result["alpha"].as<std::string>(), ';');
                if (alphaStrs.size() != 3) {
                    std::cout << "--alpha must have 3 semicolon-separated values" << std::endl;
                    return 1;
                }
                for (size_t i = 0; i < 3; ++i) {
                    try {
                        alphas[i] = std::stof(StringUtil::trim_copy(alphaStrs[i]));
                    }
                    catch (...) {
                        std::cout << "Invalid float value for alpha[" << i << "]: " << alphaStrs[i] << std::endl;
                        return 1;
                    }
                }
            };

			NetworkType netType = parseNetType(netTypeStr);
			bool isPUCT = (netType >= NetworkType::Net_TwoLayer4_PUCT && netType <= NetworkType::Net_TwoLayer32_PUCT);
            // Load datasets (3 ages) from ../7wDataset/<inPrefix>dataset_ageX.bin
            if (inPrefix.empty()) {
                std::cout << "For training you must provide --in <datasetPrefix> (prefix used when dataset was serialized)." << std::endl;
                return 1;
            }

            std::string datasetDir = "../7wDataset/";
            GameContext context(42); // deterministic card tables; seed doesn't matter for deserializing

            ML_Toolbox::Dataset dataset[3];
            for (u32 age = 0; age < 3; ++age) {
                std::stringstream ss;
                ss << datasetDir << inPrefix << "_dataset_age" << age << ".bin";
                std::string path = ss.str();
                if (!std::filesystem::exists(path)) {
                    std::cout << "Dataset file not found: " << path << std::endl;
                    return 1;
                }
                bool ok = dataset[age].loadFromFile(context, path);
                if (!ok) {
                    std::cout << "Failed to load dataset: " << path << std::endl;
                    return 1;
                }

                dataset[age].prepareForTraining(context, 2, 2); // shuffle before training
                std::cout << "Loaded age " << age << " dataset: " << dataset[age].m_data.size() << " points." << std::endl;
            }

            // Construct nets for 3 ages
            std::array<std::shared_ptr<BaseNN>, 3> nets = {
                ML_Toolbox::constructNet(netType, useExtra),
                ML_Toolbox::constructNet(netType, useExtra),
                ML_Toolbox::constructNet(netType, useExtra)
            };

            // Train per-age in parallel
            std::cout << "Lerning rates: " << alphas[0] << ", " << alphas[1] << ", " << alphas[2] << std::endl;
            std::cout << "batch sizes: " << batchSizes[0] << ", " << batchSizes[1] << ", " << batchSizes[2] << std::endl;
            std::vector<int> ages = { 0, 1, 2 };
            std::for_each(std::execution::par, ages.begin(), ages.end(), [&](int age) {
                std::vector<ML_Toolbox::Batch> batches;
                dataset[age].fillBatches(batchSizes[age], batches, useExtra, isPUCT);
                std::cout << "Training net for age " << age << " over " << epochs << " epochs, " << batches.size() << " batches." << std::endl;
                ML_Toolbox::trainNet((u32)age, epochs, batches, nets[age].get(), alphas[age]);
            });

            u32 generation = result["gen"].as<u32>();
            ML_Toolbox::saveNet(outPrefix, generation, nets);
            std::cout << "Training complete. Networks saved with prefix: " << outPrefix << " gen=" << generation << std::endl;
            return 0;
        }
        else if (mode == "stats") {
            if (inPrefix.empty()) {
                std::cout << "For stats you must provide --in <datasetPrefix> (prefix used when dataset was serialized)." << std::endl;
                return 1;
            }

            Tournament tournament;
            tournament.deserializeDataset(inPrefix);

            ML_Toolbox::Dataset dataset[3];
            tournament.fillDataset(dataset);

            for (u32 age = 0; age < 3; ++age) {
                std::cout << "----------------------------------------" << std::endl;
                std::cout << "Age " << age+1 << " (" << dataset[age].m_data.size() << " points)" << std::endl;
                dataset[age].printStats();
            }

            return 0;
        }
        else {
            std::cout << "Unknown mode: " << mode << ". Use 'generate' or 'train'." << std::endl;
            return 1;
        }
    }
    catch (const cxxopts::OptionException& e) {
        std::cout << "Error parsing options: " << e.what() << std::endl;
        return 1;
    }
    
    // return 0;
}

#if 0
int main()
{


	using namespace sevenWD;
	GameContext sevenWDContext(u32(time(nullptr)));

	// Current AI to train
	using AIType = MCTS_Simple;
	auto setupAI = [](AIType* pAI) -> AIType* {
		pAI->m_depth = 10;
		pAI->m_numSimu = 100;
		return pAI;
	};

	NetworkType netType = NetworkType::Net_TwoLayer8;
	const bool useExtraTensorData = false;
	unsigned int datasetSize = 10'000;
	std::string namePrefix = datasetSize < 50'000 ? "_small" : "";

	u32 generation;
	Tournament tournament;
	
	tournament.addAI(new MonteCarloAI(100));

	AIInterface* newGenAI;

	{
		auto [pAI, gen] = ML_Toolbox::loadAIFromFile<AIType>(netType, namePrefix, useExtraTensorData);

		if (pAI) {
			newGenAI = pAI;
			setupAI(pAI);
			generation = gen+1;
		} else {
			newGenAI = new MonteCarloAI(100);
			generation = 0;
		}

#if 0
		if (pAI) {
			tournament.addAI(pAI);
		}
		tournament.generateDataset(sevenWDContext, 1000);
		tournament.print();
		return 0;
#endif
	}

	tournament.deserializeDataset(std::string("test"));
	
	while (1)
	{
		tournament.resetTournament(1.0f);
		// tournament.generateDatasetFromAI(sevenWDContext, newGenAI, datasetSize);
		// tournament.print();

		ML_Toolbox::Dataset dataset[3];
		tournament.fillDataset(dataset);

		std::array<std::shared_ptr<BaseNN>, 3> net = {
			ML_Toolbox::constructNet(netType, useExtraTensorData),
			ML_Toolbox::constructNet(netType, useExtraTensorData),
			ML_Toolbox::constructNet(netType, useExtraTensorData)
		};

		std::cout << "\nStart train generation " << generation << " over " << dataset[0].m_data.size() << " batches." << std::endl;

		auto range = { 0,1,2 };
		std::for_each(std::execution::par, range.begin(), range.end(), [&](int i)
		{
			std::vector<ML_Toolbox::Batch> batches;
			dataset[i].fillBatches(64, batches, useExtraTensorData);
			ML_Toolbox::trainNet(i, 24, batches, net[i].get());
		});

		ML_Toolbox::saveNet(namePrefix, generation, net);

		std::stringstream networkName;
		networkName << BaseNN::getNetworkName(netType) << (useExtraTensorData ? "_extra" : "_base") << namePrefix << "_gen" << generation;
		tournament.removeWorstAI(4);
		newGenAI = setupAI(new AIType(networkName.str(), net));

		generation++;
	}


	return 0;
}
#endif