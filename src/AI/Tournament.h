#pragma once

#include "BuildConfig.h"
#include "AI.h"
#include "ML.h"

class Tournament
{
public:
	static constexpr auto ExecPolicy = std::execution::par;

	Tournament();

	void addAI(AIInterface* pAI);
	void generateDataset(const bg::GameContext& context, u32 numGameToPlay, u32 numThreads);
	void generateDatasetFromAI(const bg::GameContext& context, AIInterface* pAI, u32 datasetSize);
	void removeWorstAI(u32 amountOfAIsToKeep);

	void fillDataset(ML_Toolbox::Dataset (&dataset)[bg::cNumNetworks]) const;
	void resetTournament(float percentageOfGamesToKeep);
	void playOneGame(const bg::GameContext& context, std::array<ML_Toolbox::Dataset, bg::cNumNetworks>& threadSafeDataset, u32 i, u32 j, void* pAIContextI, void* pAIContextJ);
	void playOneGame(const bg::GameContext& context, u32 i, u32 j);

	void print() const;
	void serializeDataset(const std::string& filenamePrefix) const;
	void deserializeDataset(const std::string& filenamePrefix) const;

private:
	static constexpr u32 NumStatesToSamplePerGame = 16;

	struct WinTypeCounter {
		u32 civil = 0;
		u32 military = 0;
		u32 science = 0;

		void incr(bg::WinType type) {
			//switch (type) {
			//case sevenWD::WinType::Civil:
			//	civil++; break;
			//case sevenWD::WinType::Military:
			//	military++; break;
			//case sevenWD::WinType::Science:
			//	science++; break;
			//}
		}

		u32 get(bg::WinType type) const {
			//switch (type) {
			//case sevenWD::WinType::Civil:
			//	return civil;
			//case sevenWD::WinType::Military:
			//	return military;
			//case sevenWD::WinType::Science:
			//	return science;
			//}
			return 0;
		}
	};

	std::vector<AIInterface*> m_AIs;

	std::atomic_uint m_numGameInDataset = 0;
	std::atomic_uint m_numGamePlayed = 0;
	std::mutex m_statsMutex;
	std::vector<std::pair<u32, u32>> m_numWins;
	std::vector<WinTypeCounter> m_winTypes;
	std::vector<double> m_avgThinkingMsPerGame;

	ML_Toolbox::Dataset m_dataset[bg::cNumNetworks];
};
