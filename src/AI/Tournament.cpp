
#include "Tournament.h"

Tournament::Tournament()
{

}

void Tournament::addAI(sevenWD::AIInterface* pAI)
{ 
	std::cout << "Add AI : " << pAI->getName() << std::endl;
	m_AIs.push_back(pAI); 
	m_numWins.push_back(std::make_pair(0u, 0u)); 
	m_avgThinkingMsPerGame.push_back(0.0);
	m_winTypes.emplace_back(); 
}

void Tournament::generateDataset(const sevenWD::GameContext& context, u32 datasetSize)
{
	using namespace sevenWD;

	m_numGameInDataset = (u32)m_dataset[0].m_data.size();
	std::vector<std::array<ML_Toolbox::Dataset, 3>> perThreadDataset(16); // 16 threads

	std::for_each(Tournament::ExecPolicy, perThreadDataset.begin(), perThreadDataset.end(), [&](auto& threadSafeDataset) {
		std::vector<void*> perThreadAIContext(m_AIs.size());
		for (size_t i = 0; i < m_AIs.size(); ++i) {
			perThreadAIContext[i] = m_AIs[i]->createPerThreadContext();
		}

		while (m_numGameInDataset < datasetSize) {
			// Match every AIs against every others
			for (u32 i = 0; i < m_AIs.size(); ++i) {
				for (u32 j = 0; j < m_AIs.size(); ++j) {
					if (i != j)
						playOneGame(context, threadSafeDataset, i, j, perThreadAIContext[i], perThreadAIContext[j]);
				}
			}

			std::cout << m_numGameInDataset << " / " << datasetSize << std::endl;
		}

		for (size_t i = 0; i < m_AIs.size(); ++i) {
			m_AIs[i]->destroyPerThreadContext(perThreadAIContext[i]);
		}
	});

	for (const auto& dataset : perThreadDataset) {
		m_dataset[0] += dataset[0];
		m_dataset[1] += dataset[1];
		m_dataset[2] += dataset[2];
	}

	m_dataset[0].shuffle(context);
	m_dataset[1].shuffle(context);
	m_dataset[2].shuffle(context);
}

void Tournament::generateDatasetFromAI(const sevenWD::GameContext& context, sevenWD::AIInterface* pAI, u32 datasetSize)
{
	using namespace sevenWD;

	if (pAI) {
		addAI(pAI);
	}

	m_numGameInDataset = (u32)m_dataset[0].m_data.size();
	std::vector<std::array<ML_Toolbox::Dataset, 3>> perThreadDataset(16); // 16 threads

	std::atomic_uint printCounter = 0;

	std::for_each(Tournament::ExecPolicy, perThreadDataset.begin(), perThreadDataset.end(), [&](auto& threadSafeDataset) {
		u32 parity = 0;

		std::vector<void*> perThreadAIContext(m_AIs.size());
		for (size_t i = 0; i < m_AIs.size(); ++i) {
			perThreadAIContext[i] = m_AIs[i]->createPerThreadContext();
		}

		while (m_numGameInDataset < datasetSize) {
			// Match the last AI against every others
			for (u32 i = 0; i < m_AIs.size() - 1; ++i) {
				u32 aiIndex[2] = { u32(m_AIs.size() - 1), i };
				if (parity) {
					std::swap(aiIndex[0], aiIndex[1]);
				}
				playOneGame(context, threadSafeDataset, aiIndex[0], aiIndex[1], perThreadAIContext[aiIndex[0]], perThreadAIContext[aiIndex[1]]);
			}
			parity = (parity + 1) % 2;

			u32 counter = printCounter.fetch_add(1);
			if (counter % 100 == 0) {
				std::cout << m_numGameInDataset << " / " << datasetSize << std::endl;
			}
		}

		for (size_t i = 0; i < m_AIs.size(); ++i) {
			m_AIs[i]->destroyPerThreadContext(perThreadAIContext[i]);
		}
	});

	for (const auto& dataset : perThreadDataset) {
		m_dataset[0] += dataset[0];
		m_dataset[1] += dataset[1];
		m_dataset[2] += dataset[2];
	}

	m_dataset[0].shuffle(context);
	m_dataset[1].shuffle(context);
	m_dataset[2].shuffle(context);
}

void Tournament::playOneGame(const sevenWD::GameContext& context, u32 i, u32 j)
{
	using namespace sevenWD;
	std::array<ML_Toolbox::Dataset, 3> db;
	playOneGame(context, db, i, j, nullptr, nullptr);

	m_dataset[0] += db[0];
	m_dataset[1] += db[1];
	m_dataset[2] += db[2];
}

void Tournament::playOneGame(const sevenWD::GameContext& context, std::array<ML_Toolbox::Dataset, 3>& threadSafeDataset, u32 i, u32 j, void* pAIContextI, void* pAIContextJ)
{
	using namespace sevenWD;
	AIInterface* AIs[2] = { m_AIs[i], m_AIs[j] };
	void* AIThreadContexts[2] = { pAIContextI, pAIContextJ };
	u32 aiIndex[2] = { i, j };

	WinType winType;
	std::vector<GameState> states[3];
	double thinkingTime[2];
	u32 winner = ML_Toolbox::generateOneGameDatasSet(context, AIs, AIThreadContexts, states, winType, thinkingTime);

	// TODO atomics ?
	m_statsMutex.lock();
	m_numWins[aiIndex[winner]].first++;
	m_winTypes[aiIndex[winner]].incr(winType);
	m_numWins[aiIndex[0]].second++;
	m_numWins[aiIndex[1]].second++;
	m_avgThinkingMsPerGame[aiIndex[0]] += thinkingTime[0];
	m_avgThinkingMsPerGame[aiIndex[1]] += thinkingTime[1];
	m_statsMutex.unlock();

	for (u32 age = 0; age < 3; ++age) {
		std::vector<u32> turns(states[age].size());
		for (size_t t = 0; t < turns.size(); ++t)
			turns[t] = (u32)t;

		std::shuffle(turns.begin(), turns.end(), context.rand());
		for (u32 t = 0; t < std::min(NumStatesToSamplePerGame, (u32)states[age].size()); ++t) {
			ML_Toolbox::Dataset::Point p{ states[age][turns[t]], winner, winType };
			threadSafeDataset[age].m_data.push_back(p);
		}
	}

	m_numGameInDataset += std::min(NumStatesToSamplePerGame, (u32)states[0].size());
}

void Tournament::removeWorstAI(u32 amountOfAIsToKeep)
{
	while (m_AIs.size() > amountOfAIsToKeep) {
		unsigned int index=0;
		for (unsigned int i = 0; i < m_numWins.size(); ++i) {
			const auto& a = m_numWins[i];
			const auto& b = m_numWins[index];
			// if ((m_AIs[i]->isTrainable() && (double(a.first) / a.second) < (double(b.first) / b.second)) || (m_AIs[i]->isTrainable() && !m_AIs[index]->isTrainable())) {
			// 	index = i;
			// }
			if ((double(a.first) / a.second) < (double(b.first) / b.second)) {
				index = i;
			}
		}

		m_numWins.erase(m_numWins.begin() + index);
		m_winTypes.erase(m_winTypes.begin() + index);
		m_avgThinkingMsPerGame.erase(m_avgThinkingMsPerGame.begin() + index);

		delete m_AIs[index];
		m_AIs.erase(m_AIs.begin() + index);
	}
}

void Tournament::fillDataset(ML_Toolbox::Dataset(&dataset)[3]) const
{
	for (u32 i = 0; i < 3; ++i) 
		dataset[i] += m_dataset[i];
}

void Tournament::resetTournament(float percentageOfGamesToKeep)
{
	for (u32 i = 0; i < 3; ++i) {
		m_dataset[i].m_data.resize(size_t(((double)m_dataset[i].m_data.size()) * percentageOfGamesToKeep));
	}

	m_numWins.clear();
	m_winTypes.clear();
	m_avgThinkingMsPerGame.clear();
	for (u32 i = 0; i < m_AIs.size(); ++i) {
		m_numWins.push_back(std::make_pair(0u, 0u));
		m_winTypes.emplace_back();
		m_avgThinkingMsPerGame.push_back(0.0);
	}
}

void Tournament::print() const
{
	std::cout << "Tournament result:" << std::endl;
	for (u32 i = 0; i < m_AIs.size(); ++i)
	{
		std::cout << m_AIs[i]->getName() << " : Winrate " << std::setprecision(2) << float(m_numWins[i].first) / m_numWins[i].second << " ; "
			      << m_numWins[i].first << " / " << m_numWins[i].second << "(" << m_winTypes[i].civil << "," << m_winTypes[i].military << "," << m_winTypes[i].science << "), time : " << m_avgThinkingMsPerGame[i] / m_numWins[i].second << std::endl;
	}
}