#pragma once

#include "7WDuel/GameController.h"
#include "AI.h"
#include "ML.h"

class Tournament
{
public:
	static constexpr auto ExecPolicy = std::execution::par;

	Tournament();

	void addAI(sevenWD::AIInterface* pAI);
	void generateDataset(const sevenWD::GameContext& context, u32 numGameToPlay, u32 numThreads);
	void generateDatasetFromAI(const sevenWD::GameContext& context, sevenWD::AIInterface* pAI, u32 datasetSize);
	void removeWorstAI(u32 amountOfAIsToKeep);

	void fillDataset(ML_Toolbox::Dataset (&dataset)[3]) const;
	void resetTournament(float percentageOfGamesToKeep);
	void playOneGame(const sevenWD::GameContext& context, std::array<ML_Toolbox::Dataset, 3>& threadSafeDataset, u32 i, u32 j, void* pAIContextI, void* pAIContextJ);
	void playOneGame(const sevenWD::GameContext& context, u32 i, u32 j);

	void print() const;
	void serializeDataset(const std::string& filenamePrefix) const;
	void deserializeDataset(const std::string& filenamePrefix) const;

private:
	static constexpr u32 NumStatesToSamplePerGame = 16;

	struct WinTypeCounter {
		u32 civil = 0;
		u32 military = 0;
		u32 science = 0;

		void incr(sevenWD::WinType type) {
			switch (type) {
			case sevenWD::WinType::Civil:
				civil++; break;
			case sevenWD::WinType::Military:
				military++; break;
			case sevenWD::WinType::Science:
				science++; break;
			}
		}

		u32 get(sevenWD::WinType type) const {
			switch (type) {
			case sevenWD::WinType::Civil:
				return civil;
			case sevenWD::WinType::Military:
				return military;
			case sevenWD::WinType::Science:
				return science;
			}
			return 0;
		}
	};

	std::vector<sevenWD::AIInterface*> m_AIs;

	std::atomic_uint m_numGameInDataset = 0;
	std::atomic_uint m_numGamePlayed = 0;
	std::mutex m_statsMutex;
	std::vector<std::pair<u32, u32>> m_numWins;
	std::vector<WinTypeCounter> m_winTypes;
	std::vector<double> m_avgThinkingMsPerGame;

	ML_Toolbox::Dataset m_dataset[3];
};
