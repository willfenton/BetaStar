#include "BetaStar.h"

using namespace sc2;


// called each time the coordinator steps the simulation forward
void BetaStar::OnStep() {
    const ObservationInterface* observation = Observation();
    const GameInfo game_info = observation->GetGameInfo();

    //std::cout << Observation()->GetGameLoop() << std::endl;

    OnStepComputeStatistics();

    if (m_warpgate_researched) {
        for (const auto& gateway : FriendlyUnitsOfType(UNIT_TYPEID::PROTOSS_GATEWAY)) {
            if (gateway->build_progress == 1 && gateway->orders.size() == 0) {
                Actions()->UnitCommand(gateway, ABILITY_ID::MORPH_WARPGATE);
            }
        }
    }

    OnStepTrainWorkers();

    OnStepBuildPylons();

    //OnStepBuildGas();

    //OnStepExpand();

    OnStepManageWorkers();

    //OnStepBuildArmy();

    TrainBalancedArmy();

    OnStepBuildOrder();

    OnStepManageArmy();

    OnStepResearchUpgrades();
}

void BetaStar::OnGameStart()
{
    const ObservationInterface* observation = Observation();
    const GameInfo game_info = observation->GetGameInfo();

    std::cout << "I am player number " << observation->GetPlayerID() << std::endl;
    //Actions()->SendChat("gl hf");

    // get position of first command center
    m_starting_pos = observation->GetStartLocation();
    std::cout << "Start location: (" << m_starting_pos.x << "," << m_starting_pos.y << ")" << std::endl;

    // calculate all expansion locations (this takes a while so we do it at the start of the game)
    m_expansion_locations = search::CalculateExpansionLocations(Observation(), Query());

    /* SCOUTING */

    // pick a random worker to be our scout
    Units workers = FriendlyUnitsOfType(m_worker_typeid);
    m_initial_scouting_probe = GetRandomEntry(workers);

    // this is a list of points where the enemy could have started
    std::vector<Point2D> enemy_start_locations = game_info.enemy_start_locations;
    size_t size = enemy_start_locations.size();

    // command the scout probe to visit all possible enemy starting locations
    // this big block of code just makes the probe visit them in optimal order
    // after this, the scouting probe will have move orders queued for each enemy start location
    Actions()->UnitCommand(m_initial_scouting_probe, ABILITY_ID::MOVE, m_initial_scouting_probe->pos);
    Point2D last_location = m_initial_scouting_probe->pos;
    for (size_t i = 0; i < size; ++i) {
        Point2D closest_start_location;
        float closest_distance = std::numeric_limits<float>::max();
        for (const auto& enemy_start_location : enemy_start_locations) {
            float distance = DistanceSquared2D(last_location, enemy_start_location);
            if (distance < closest_distance) {
                closest_start_location = enemy_start_location;
                closest_distance = distance;
            }
        }
        Actions()->UnitCommand(m_initial_scouting_probe, ABILITY_ID::MOVE, closest_start_location, true);
        last_location = closest_start_location;
        enemy_start_locations.erase(std::find(enemy_start_locations.begin(), enemy_start_locations.end(), closest_start_location));
    }

    // Testing basic blink stalker strat (should be set dynamically based on intelligence about enemy and our win/loss record)
    SetStrategy(Strategy::Blink_Stalker_Rush);
}

// Called each time a unit has been built and has no orders or the unit had orders in the previous step and currently does not
// Both buildings and units are considered units and are represented with a Unit object.
void BetaStar::OnUnitIdle(const Unit* unit)
{
    switch (unit->unit_type.ToType()) {

        case UNIT_TYPEID::PROTOSS_PROBE: {
            const Unit* target = FindResourceToGather(unit->pos);
            Actions()->UnitCommand(unit, ABILITY_ID::HARVEST_GATHER, target);
            break;
        }

        default: {
            //std::cout << "Idle unit of type " << unit->unit_type.to_string() << std::endl;
            break;
        }
    }
}

void BetaStar::OnBuildingConstructionComplete(const Unit* unit)
{
    /*switch (unit->unit_type.ToType()) {

        case UNIT_TYPEID::PROTOSS_NEXUS: {
            m_building_nexus = false;
            break;
        }
    }*/
}

void BetaStar::OnUnitEnterVision(const Unit* unit)
{
    // initial scouting probe found it
    if (!m_enemy_base_scouted && IsStructure(unit->unit_type) && DistanceSquared2D(unit->pos, m_initial_scouting_probe->pos) < 150) {
        // find closest enemy starting location
        Point2D closest_enemy_start_location;
        float closest_distance = std::numeric_limits<float>::max();
        for (const auto& enemy_start_location : Observation()->GetGameInfo().enemy_start_locations) {
            float distance = DistanceSquared2D(unit->pos, enemy_start_location);
            if (distance < closest_distance) {
                closest_enemy_start_location = enemy_start_location;
                closest_distance = distance;
            }
        }
        if (closest_distance < std::numeric_limits<float>::max()) {
            std::cout << "Enemy start location found: (" << closest_enemy_start_location.x << "," << closest_enemy_start_location.y << ")" << std::endl;
            m_enemy_base_pos = closest_enemy_start_location;
            m_enemy_base_scouted = true;
            Actions()->UnitCommand(m_initial_scouting_probe, ABILITY_ID::MOVE, m_starting_pos);
        }
    }

    // gather meta-information about opponent - does not take any actions
    GatherIntelligence(unit);

    // TODO: Respond to threats
}

void BetaStar::OnUpgradeCompleted(UpgradeID upgrade_id)
{
    switch (upgrade_id.ToType()) {
        case UPGRADE_ID::WARPGATERESEARCH: {
            std::cout << "Warpgate research complete" << std::endl;
            m_warpgate_researched = true;
            break;
        }
        case UPGRADE_ID::BLINKTECH: {
            std::cout << "Blink research complete" << std::endl;
            m_blink_researched = true;
            break;
        }
        default: {
            break;
        }
    }
}
