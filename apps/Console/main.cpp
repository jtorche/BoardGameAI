#include <array>
#include <iostream>
#include <windows.h>
#include <execution>

#include "7WDuel/GameController.h"

#include "AI/AI.h"
#include "AI/ML.h"
#include "AI/MCTS.h"
#include "AI/Tournament.h"

namespace sevenWD
{
	void costTest();

	const char* toString(WinType _type)
	{
		switch (_type)
		{
		case WinType::Civil:
			return "Civil";
		case WinType::Military:
			return "Military";
		case WinType::Science:
			return "Science";
		default:
			return "None";
		}
	}

	const char* toString(Move::Action _action)
	{
		switch (_action)
		{
		case Move::Action::BuildWonder:
			return "BuildWonder";
		case Move::Action::Pick:
			return "Pick";
		case Move::Action::Burn:
			return "Burn";
		case Move::Action::ScienceToken:
			return "ScienceToken";
		default:
			return "None";
		}
	}


}

#if 1
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
	unsigned int datasetSize = 200'000;
	std::string namePrefix = datasetSize < 50'000 ? "_small" : "";

	u32 generation;
	Tournament tournament;
	// tournament.addAI(new MonteCarloAI(50));
	// tournament.addAI(new MonteCarloAI(50));
	// tournament.addAI(new MonteCarloAI(200));
	// tournament.addAI(new NoBurnAI());
	// tournament.addAI(new RandAI());

	AIInterface* newGenAI;

	{
		// Load a previous trained AI as an oppponent
		auto [pAI, gen] = ML_Toolbox::loadAIFromFile<MCTS_Simple>(NetworkType::Net_TwoLayer8, "", false);
		pAI->m_depth = 10;
		pAI->m_numSimu = 50;
		if(pAI)
			tournament.addAI(pAI);
	}

	tournament.playOneGame(sevenWDContext, 0, 0);

	//{
	//	// Load a previous trained AI as an oppponent
	//	auto [pAI, gen] = ML_Toolbox::loadAIFromFile<SimpleNetworkAI>(NetworkType::Net_TwoLayer8, "", false);
	//	if (pAI) {
	//		tournament.addAI(pAI);
	//	}
	//}

	{
		auto [pAI, gen] = ML_Toolbox::loadAIFromFile<AIType>(netType, namePrefix, useExtraTensorData);

		if (pAI) {
			newGenAI = pAI;
			setupAI(pAI);
			generation = gen+1;
		} else {
			newGenAI = new MonteCarloAI(5);
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

	
	
	while (1)
	{
		tournament.resetTournament(0.f);
		tournament.generateDatasetFromAI(sevenWDContext, newGenAI, datasetSize);
		tournament.print();

		ML_Toolbox::Dataset dataset[3];
		tournament.fillDataset(dataset);

		std::shared_ptr<BaseNN> net[3] = {
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

			// tiny_dnn::tensor_t data;
			// tiny_dnn::tensor_t labels;
			// dataset[i].fillBatches(useExtraTensorData, data, labels);
			// ML_Toolbox::trainNet(i, 16, data, labels, net[i].get());
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

#if 0
int main()
{
	sevenWD::costTest();

	std::cout << "Sizoef Card = " << sizeof(sevenWD::Card) << std::endl;
	std::cout << "Sizoef GameState = " << sizeof(sevenWD::GameState) << std::endl;
	sevenWD::GameContext sevenWDContext(u32(time(nullptr)));
	sevenWD::GameController game(sevenWDContext);

	sevenWD::MinMaxAIHeuristic* pHeuristic = ML_Toolbox::loadAIFromFile<BaseLine>(false, 0).first;
	sevenWD::MinMaxAI* pAI = new sevenWD::MinMaxAI(pHeuristic, 6);

	u32 turn = 0;
	while (1)
	{
		std::cout << std::endl;
		std::cout << "Turn " << ++turn <<" Player " << game.m_gameState.getCurrentPlayerTurn() << std::endl;
		std::cout << "Player0 win proba = " << (pHeuristic ? pHeuristic->computeScore(game.m_gameState, 0) : 0.0f) << std::endl;
		game.m_gameState.printGameState(std::cout) << std::endl;

		std::vector<sevenWD::Move> moves;
		game.enumerateMoves(moves);

		sevenWD::Move aiMove = pAI->selectMove(sevenWDContext, game, moves);
		std::cout << "AI move: "; game.printMove(std::cout, aiMove) << std::endl;

		int moveId = 0;
		for (sevenWD::Move move : moves) {
			std::cout << "(" << ++moveId << ") ";
			game.printMove(std::cout, move) << std::endl;
		}

		std::cout << "Choice : ";
		std::cin >> moveId;

		sevenWD::Move choice;
		if (moveId == 0) {
			choice = pAI->selectMove(sevenWDContext, game, moves); // debug
		}
		else {
			choice = moves[moveId - 1];
		}
		bool end = game.play(moves[moveId-1]);

		if (end) {
			continue;
		}

		// sevenWD::SpecialAction action = sevenWD::SpecialAction::Nothing;
		// if (pick == 1)
		// 	action = state.pick(cardIndex-1);
		// else
		// 	state.burn(cardIndex-1);
		// 
		// if (action == sevenWD::SpecialAction::TakeScienceToken)
		// {
		// 	state.printAvailableTokens();
		// 
		// 	u32 tokenIndex = 0;
		// 	std::cout << "Pick science token:";
		// 	std::cin >> tokenIndex;
		// 	action = state.pickScienceToken(tokenIndex-1);
		// }
		// 
		// if (action == sevenWD::SpecialAction::MilitaryWin || action == sevenWD::SpecialAction::ScienceWin)
		// {
		// 	std::cout << "Winner is " << state.getCurrentPlayerTurn() << " (military or science win)" << std::endl;
		// 	return 0;
		// }
		// 
		// sevenWD::GameState::NextAge ageState = state.nextAge();
		// if (ageState == sevenWD::GameState::NextAge::None)
		// {
		// 	if (action != sevenWD::SpecialAction::Replay)
		// 		state.nextPlayer();
		// }
		// else if (ageState == sevenWD::GameState::NextAge::EndGame)
		// {
		// 	std::cout << "Winner is " << state.findWinner() << std::endl;
		// 	return 0;
		// }
	}

	std::cout << "Unit tests succeeded" << std::endl << std::endl;
	system("pause");

	return 0;
}
#endif
