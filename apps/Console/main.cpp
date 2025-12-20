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


using namespace std::chrono;
using namespace sevenWD;

static sevenWD::AIInterface* createAIByName(const std::string& name)
{
    // Map textual name -> AI instance. Add more mappings as needed.
    if (name == "RandAI")
        return new RandAI();
    if (strstr(name.c_str(), "MonteCarloAI")) {
		u32 numSimulations = 100;
        sscanf_s(name.c_str(), "MonteCarloAI(%u)", &numSimulations);
        return new MonteCarloAI(numSimulations);
    }
    if (strstr(name.c_str(), "MCTS_Deterministic")) {
        u32 numMoves = 1000;
        u32 numSimu = 20;
        sscanf_s(name.c_str(), "MCTS_Deterministic(%u;%u)", &numMoves, &numSimu);
        return new MCTS_Deterministic(numMoves, numSimu);
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
            ("mode", "Mode: generate or train", cxxopts::value<std::string>()->default_value("generate"))
            ("size", "Dataset size (number of games)", cxxopts::value<uint32_t>()->default_value("100"))
            // allow multiple --ai entries, default is two AIs (RandAI and MonteCarloAI)
            ("ai", "AI to include in generation (repeatable). Examples: --ai=RandAI --ai=MonteCarloAI(100)",
                 cxxopts::value<std::vector<std::string>>()->default_value("RandAI,MonteCarloAI"))
            ("in", "Input filename prefix (for dataset or nets)", cxxopts::value<std::string>()->default_value(""))
            ("out", "Output filename prefix (for dataset or nets)", cxxopts::value<std::string>()->default_value(""))
            ("net", "Network type for training: BaseLine, TwoLayer8, TwoLayer64", cxxopts::value<std::string>()->default_value("TwoLayer8"))
            ("extra", "Use extra tensor data for network", cxxopts::value<bool>()->default_value("false"))
            ("epochs", "Training epochs", cxxopts::value<uint32_t>()->default_value("16"))
            ("batch", "Batch size", cxxopts::value<uint32_t>()->default_value("64"))
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

            std::cout << "Generating dataset of " << size << " games using " << numAIsAdded << " AIs..." << std::endl;
            tournament.generateDataset(context, size);
            tournament.print();

            tournament.serializeDataset(outPrefix);
            // Write a copy if
            {
                std::stringstream ss;
                ss << "gen_" << std::chrono::duration_cast<std::chrono::seconds>(system_clock::now().time_since_epoch()).count() << "_";
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
            uint32_t batchSize = result["batch"].as<uint32_t>();

            NetworkType netType = NetworkType::Net_TwoLayer8;
            if (netTypeStr == "BaseLine") netType = NetworkType::Net_BaseLine;
            else if (netTypeStr == "TwoLayer8") netType = NetworkType::Net_TwoLayer8;
            else if (netTypeStr == "TwoLayer64") netType = NetworkType::Net_TwoLayer64;

            // Load datasets (3 ages) from ../7wDataset/<outPrefix>dataset_ageX.bin
            if (outPrefix.empty()) {
                std::cout << "For training you must provide --out <datasetPrefix> (prefix used when dataset was serialized)." << std::endl;
                return 1;
            }

            std::string datasetDir = "../7wDataset/";
            GameContext context(42); // deterministic card tables; seed doesn't matter for deserializing

            ML_Toolbox::Dataset dataset[3];
            for (u32 age = 0; age < 3; ++age) {
                std::stringstream ss;
                ss << datasetDir << outPrefix << "dataset_age" << age << ".bin";
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
                std::cout << "Loaded age " << age << " dataset: " << dataset[age].m_data.size() << " points." << std::endl;
            }

            // Construct nets for 3 ages
            std::array<std::shared_ptr<BaseNN>, 3> nets = {
                ML_Toolbox::constructNet(netType, useExtra),
                ML_Toolbox::constructNet(netType, useExtra),
                ML_Toolbox::constructNet(netType, useExtra)
            };

            // Train per-age in parallel (simple loop here; can be parallelized)
            for (int age = 0; age < 3; ++age) {
                std::cout << "Preparing batches for age " << age << " (batch size " << batchSize << ")..." << std::endl;
                std::vector<ML_Toolbox::Batch> batches;
                dataset[age].fillBatches(batchSize, batches, useExtra);
                std::cout << "Training net for age " << age << " over " << epochs << " epochs, " << batches.size() << " batches." << std::endl;
                ML_Toolbox::trainNet((u32)age, epochs, batches, nets[age].get());
            }

            // Save networks to disk with generation 0 (or timestamp)
            u32 generation = 0;
            if (!outPrefix.empty()) {
                // use a timestamp-based generation to avoid overwriting
                generation = (u32)std::chrono::duration_cast<std::chrono::seconds>(system_clock::now().time_since_epoch()).count();
            }

            ML_Toolbox::saveNet(outPrefix, generation, nets);
            std::cout << "Training complete. Networks saved with prefix: " << outPrefix << " gen=" << generation << std::endl;
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