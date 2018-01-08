#include "StrategyManager.h"

using namespace MyBot;

StrategyManager & StrategyManager::Instance()
{
	static StrategyManager instance;
	return instance;
}

StrategyManager::StrategyManager()
{
	isFullScaleAttackStarted = false;
	isInitialBuildOrderFinished = false;
}

void StrategyManager::onStart()
{	
	setInitialBuildOrder();
}

void StrategyManager::onEnd(bool isWinner)
{	
}

void StrategyManager::update()
{
	if (BuildManager::Instance().buildQueue.isEmpty()) {
		isInitialBuildOrderFinished = true;
	}
		
	executeWorkerTraining();

	executeSupplyManagement();

	executeBasicCombatUnitTraining();

	executeCombat();
}

void StrategyManager::setInitialBuildOrder()
{
	BuildManager::Instance().buildQueue.queueAsLowestPriority(InformationManager::Instance().getWorkerType());
	BuildManager::Instance().buildQueue.queueAsLowestPriority(InformationManager::Instance().getWorkerType());
	BuildManager::Instance().buildQueue.queueAsLowestPriority(InformationManager::Instance().getWorkerType());
	BuildManager::Instance().buildQueue.queueAsLowestPriority(InformationManager::Instance().getWorkerType());
	BuildManager::Instance().buildQueue.queueAsLowestPriority(InformationManager::Instance().getWorkerType());
	BuildManager::Instance().buildQueue.queueAsLowestPriority(InformationManager::Instance().getBasicSupplyProviderUnitType(), BuildOrderItem::SeedPositionStrategy::MainBaseLocation);
	BuildManager::Instance().buildQueue.queueAsLowestPriority(InformationManager::Instance().getWorkerType());
	BuildManager::Instance().buildQueue.queueAsLowestPriority(InformationManager::Instance().getWorkerType());
	BuildManager::Instance().buildQueue.queueAsLowestPriority(BWAPI::UnitTypes::Terran_Barracks, BuildOrderItem::SeedPositionStrategy::MainBaseLocation);
	BuildManager::Instance().buildQueue.queueAsLowestPriority(InformationManager::Instance().getWorkerType());
	BuildManager::Instance().buildQueue.queueAsLowestPriority(BWAPI::UnitTypes::Terran_Refinery, BuildOrderItem::SeedPositionStrategy::MainBaseLocation);
	BuildManager::Instance().buildQueue.queueAsLowestPriority(InformationManager::Instance().getWorkerType());
	BuildManager::Instance().buildQueue.queueAsLowestPriority(InformationManager::Instance().getWorkerType());
}

// 일꾼 계속 추가 생산
void StrategyManager::executeWorkerTraining()
{
	// InitialBuildOrder 진행중에는 아무것도 하지 않습니다
	if (isInitialBuildOrderFinished == false) {
		return;
	}

	if (BWAPI::Broodwar->self()->minerals() >= 50) {
		// workerCount = 현재 일꾼 수 + 생산중인 일꾼 수
		int workerCount = BWAPI::Broodwar->self()->allUnitCount(InformationManager::Instance().getWorkerType());

		
		for (auto & unit : BWAPI::Broodwar->self()->getUnits())
		{
			if (unit->getType().isResourceDepot())
			{
				if (unit->isTraining()) {
					workerCount += unit->getTrainingQueue().size();
				}
			}
		}

		if (workerCount < 30) {
			for (auto & unit : BWAPI::Broodwar->self()->getUnits())
			{
				if (unit->getType().isResourceDepot())
				{
					if (unit->isTraining() == false) {

						// 빌드큐에 일꾼 생산이 1개는 있도록 한다
						if (BuildManager::Instance().buildQueue.getItemCount(InformationManager::Instance().getWorkerType()) == 0) {
							//std::cout << "worker enqueue" << std::endl;
							BuildManager::Instance().buildQueue.queueAsLowestPriority(MetaType(InformationManager::Instance().getWorkerType()), false);
						}
					}
				}
			}
		}
	}
}

// Supply DeadLock 예방 및 SupplyProvider 가 부족해질 상황 에 대한 선제적 대응으로서 SupplyProvider를 추가 건설/생산한다
void StrategyManager::executeSupplyManagement()
{
	// BasicBot 1.1 Patch Start ////////////////////////////////////////////////
	// 가이드 추가 및 콘솔 출력 명령 주석 처리

	// InitialBuildOrder 진행중 혹은 그후라도 서플라이 건물이 파괴되어 데드락이 발생할 수 있는데, 이 상황에 대한 해결은 참가자께서 해주셔야 합니다.
	// 오버로드가 학살당하거나, 서플라이 건물이 집중 파괴되는 상황에 대해  무조건적으로 서플라이 빌드 추가를 실행하기 보다 먼저 전략적 대책 판단이 필요할 것입니다

	// BWAPI::Broodwar->self()->supplyUsed() > BWAPI::Broodwar->self()->supplyTotal()  인 상황이거나
	// BWAPI::Broodwar->self()->supplyUsed() + 빌드매니저 최상단 훈련 대상 유닛의 unit->getType().supplyRequired() > BWAPI::Broodwar->self()->supplyTotal() 인 경우
	// 서플라이 추가를 하지 않으면 더이상 유닛 훈련이 안되기 때문에 deadlock 상황이라고 볼 수도 있습니다.
	// 저그 종족의 경우 일꾼을 건물로 Morph 시킬 수 있기 때문에 고의적으로 이런 상황을 만들기도 하고, 
	// 전투에 의해 유닛이 많이 죽을 것으로 예상되는 상황에서는 고의적으로 서플라이 추가를 하지 않을수도 있기 때문에
	// 참가자께서 잘 판단하셔서 개발하시기 바랍니다.

	// InitialBuildOrder 진행중에는 아무것도 하지 않습니다
	if (isInitialBuildOrderFinished == false) {
		return;
	}

	// 1초에 한번만 실행
	if (BWAPI::Broodwar->getFrameCount() % 24 != 0) {
		return;
	}

	// 게임에서는 서플라이 값이 200까지 있지만, BWAPI 에서는 서플라이 값이 400까지 있다
	// 저글링 1마리가 게임에서는 서플라이를 0.5 차지하지만, BWAPI 에서는 서플라이를 1 차지한다
	if (BWAPI::Broodwar->self()->supplyTotal() <= 400)
	{
		// 서플라이가 다 꽉찼을때 새 서플라이를 지으면 지연이 많이 일어나므로, supplyMargin (게임에서의 서플라이 마진 값의 2배)만큼 부족해지면 새 서플라이를 짓도록 한다
		// 이렇게 값을 정해놓으면, 게임 초반부에는 서플라이를 너무 일찍 짓고, 게임 후반부에는 서플라이를 너무 늦게 짓게 된다
		int supplyMargin = 12;

		// currentSupplyShortage 를 계산한다
		int currentSupplyShortage = BWAPI::Broodwar->self()->supplyUsed() + supplyMargin - BWAPI::Broodwar->self()->supplyTotal();

		if (currentSupplyShortage > 0) {

			// 생산/건설 중인 Supply를 센다
			int onBuildingSupplyCount = 0;
						
			onBuildingSupplyCount += ConstructionManager::Instance().getConstructionQueueItemCount(InformationManager::Instance().getBasicSupplyProviderUnitType()) * InformationManager::Instance().getBasicSupplyProviderUnitType().supplyProvided();
			
			//std::cout << "currentSupplyShortage : " << currentSupplyShortage << " onBuildingSupplyCount : " << onBuildingSupplyCount << std::endl;

			if (currentSupplyShortage > onBuildingSupplyCount) {

				// BuildQueue 최상단에 SupplyProvider 가 있지 않으면 enqueue 한다
				bool isToEnqueue = true;
				if (!BuildManager::Instance().buildQueue.isEmpty()) {
					BuildOrderItem currentItem = BuildManager::Instance().buildQueue.getHighestPriorityItem();
					if (currentItem.metaType.isUnit() && currentItem.metaType.getUnitType() == InformationManager::Instance().getBasicSupplyProviderUnitType()) {
						isToEnqueue = false;
					}
				}
				if (isToEnqueue) {
					// 주석처리
					//std::cout << "enqueue supply provider " << InformationManager::Instance().getBasicSupplyProviderUnitType().getName().c_str() << std::endl;
					BuildManager::Instance().buildQueue.queueAsHighestPriority(MetaType(InformationManager::Instance().getBasicSupplyProviderUnitType()), true);
				}
			}

		}
	}
	// BasicBot 1.1 Patch End ////////////////////////////////////////////////
}

void StrategyManager::executeBasicCombatUnitTraining()
{
	// InitialBuildOrder 진행중에는 아무것도 하지 않습니다
	if (isInitialBuildOrderFinished == false) {
		return;
	}

	// 기본 병력 추가 훈련
	if (BWAPI::Broodwar->self()->minerals() >= 200 && BWAPI::Broodwar->self()->supplyUsed() < 390) {
		{
			for (auto & unit : BWAPI::Broodwar->self()->getUnits())
			{
				if (unit->getType() == InformationManager::Instance().getBasicCombatBuildingType()) {
					if (unit->isTraining() == false || unit->getLarva().size() > 0) {
						if (BuildManager::Instance().buildQueue.getItemCount(InformationManager::Instance().getBasicCombatUnitType()) == 0) {
							BuildManager::Instance().buildQueue.queueAsLowestPriority(InformationManager::Instance().getBasicCombatUnitType());
						}
					}
				}
			}
		}
	}
}


void StrategyManager::executeCombat()
{
	// 공격 모드가 아닐 때에는 전투유닛들을 아군 진영 길목에 집결시켜서 방어
	if (isFullScaleAttackStarted == false)		
	{
		BWTA::Chokepoint* firstChokePoint = BWTA::getNearestChokepoint(InformationManager::Instance().getMainBaseLocation(InformationManager::Instance().selfPlayer)->getTilePosition());

		for (auto & unit : BWAPI::Broodwar->self()->getUnits())
		{
			if (unit->getType() == InformationManager::Instance().getBasicCombatUnitType() && unit->isIdle()) {
				CommandUtil::attackMove(unit, firstChokePoint->getCenter());
			}
		}

		// 전투 유닛이 2개 이상 생산되었고, 적군 위치가 파악되었으면 총공격 모드로 전환
		if (BWAPI::Broodwar->self()->completedUnitCount(InformationManager::Instance().getBasicCombatUnitType()) > 2) {

			if (InformationManager::Instance().enemyPlayer != nullptr
				&& InformationManager::Instance().enemyRace != BWAPI::Races::Unknown
				&& InformationManager::Instance().getOccupiedBaseLocations(InformationManager::Instance().enemyPlayer).size() > 0)
			{				
				isFullScaleAttackStarted = true;
			}
		}

	}
	// 공격 모드가 되면, 모든 전투유닛들을 적군 Main BaseLocation 로 공격 가도록 합니다
	else {
		//std::cout << "enemy OccupiedBaseLocations : " << InformationManager::Instance().getOccupiedBaseLocations(InformationManager::Instance().enemyPlayer).size() << std::endl;
		
		if (InformationManager::Instance().enemyPlayer != nullptr
			&& InformationManager::Instance().enemyRace != BWAPI::Races::Unknown
			&& InformationManager::Instance().getOccupiedBaseLocations(InformationManager::Instance().enemyPlayer).size() > 0)
		{
			// 공격 대상 지역 결정
			BWTA::BaseLocation * targetBaseLocation = nullptr;
			double closestDistance = 100000000;

			for (BWTA::BaseLocation * baseLocation : InformationManager::Instance().getOccupiedBaseLocations(InformationManager::Instance().enemyPlayer)) {

				double distance = BWTA::getGroundDistance(
					InformationManager::Instance().getMainBaseLocation(InformationManager::Instance().selfPlayer)->getTilePosition(), 
					baseLocation->getTilePosition());

				if (distance < closestDistance) {
					closestDistance = distance;
					targetBaseLocation = baseLocation;
				}
			}

			if (targetBaseLocation != nullptr) {

				for (auto & unit : BWAPI::Broodwar->self()->getUnits())
				{
					// 건물은 제외
					if (unit->getType().isBuilding()) continue;
					// 모든 일꾼은 제외
					if (unit->getType().isWorker()) continue;

					// canAttack 유닛은 attackMove Command 로 공격을 보냅니다
					if (unit->canAttack()) {

						if (unit->isIdle()) {
							CommandUtil::attackMove(unit, targetBaseLocation->getPosition());
						}
					}
				}
			}
		}
	}
}

// BasicBot 1.1 Patch End //////////////////////////////////////////////////
