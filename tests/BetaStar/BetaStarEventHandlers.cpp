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

    // for finding positions of points
    //for (const auto& pylon : FriendlyUnitsOfType(UNIT_TYPEID::PROTOSS_PYLON)) {
    //    std::cout << "(" << pylon->pos.x << "," << pylon->pos.y << ")" << std::endl;
    //}
    //if (CountUnitType(UNIT_TYPEID::PROTOSS_PYLON) > 0) {
    //    std::cout << std::endl;
    //}

    OnStepTrainWorkers();

    OnStepBuildPylons();

    //OnStepBuildGas();

    //OnStepExpand();

    OnStepManageWorkers();

    OnStepBuildArmy();

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
    m_starting_quadrant = get_starting_position_of_point(m_starting_pos);
    std::cout << "Start location: (" << m_starting_pos.x << "," << m_starting_pos.y << ")" << std::endl;

    // calculate all expansion locations (this takes a while so we do it at the start of the game)
    m_expansion_locations = search::CalculateExpansionLocations(Observation(), Query());

    /* SCOUTING */

    Units workers = FriendlyUnitsOfType(m_worker_typeid);

    const Unit* scouting_probe_1 = workers[0];
    const Unit* scouting_probe_2 = workers[1];
    
    m_scouting_probes.push_back(scouting_probe_1);
    m_scouting_probes.push_back(scouting_probe_2);

    switch (m_starting_quadrant) {
        case (SW): {    // (33.5, 33.5)
            Actions()->UnitCommand(scouting_probe_1, ABILITY_ID::MOVE, Point2D(33.5, 158.5));
            Actions()->UnitCommand(scouting_probe_1, ABILITY_ID::MOVE, Point2D(158.5, 158.5), true);
            Actions()->UnitCommand(scouting_probe_2, ABILITY_ID::MOVE, Point2D(158.5, 33.5));
            Actions()->UnitCommand(scouting_probe_2, ABILITY_ID::MOVE, Point2D(158.5, 158.5), true);
            break;
        }
        case (NW): {    // (33.5, 158.5)
            Actions()->UnitCommand(scouting_probe_1, ABILITY_ID::MOVE, Point2D(33.5, 33.5));
            Actions()->UnitCommand(scouting_probe_1, ABILITY_ID::MOVE, Point2D(158.5, 33.5), true);
            Actions()->UnitCommand(scouting_probe_2, ABILITY_ID::MOVE, Point2D(158.5, 158.5));
            Actions()->UnitCommand(scouting_probe_2, ABILITY_ID::MOVE, Point2D(158.5, 33.5), true);
            break;
        }
        case (NE): {    // (158.5, 158.5)
            Actions()->UnitCommand(scouting_probe_1, ABILITY_ID::MOVE, Point2D(33.5, 158.5));
            Actions()->UnitCommand(scouting_probe_1, ABILITY_ID::MOVE, Point2D(33.5, 33.5), true);
            Actions()->UnitCommand(scouting_probe_2, ABILITY_ID::MOVE, Point2D(158.5, 33.5));
            Actions()->UnitCommand(scouting_probe_2, ABILITY_ID::MOVE, Point2D(33.5, 33.5), true);
            break;
        }
        case (SE): {    // (158.5, 33.5)
            Actions()->UnitCommand(scouting_probe_1, ABILITY_ID::MOVE, Point2D(33.5, 33.5));
            Actions()->UnitCommand(scouting_probe_1, ABILITY_ID::MOVE, Point2D(33.5, 158.5), true);
            Actions()->UnitCommand(scouting_probe_2, ABILITY_ID::MOVE, Point2D(158.5, 158.5));
            Actions()->UnitCommand(scouting_probe_2, ABILITY_ID::MOVE, Point2D(33.5, 158.5), true);
            break;
        }
    }

    // Testing basic blink stalker strat (should be set dynamically based on intelligence about enemy and our win/loss record)
    SetStrategy(Strategy::Blink_Stalker_Rush);
}

// Called each time a unit has been built and has no orders or the unit had orders in the previous step and currently does not
// Both buildings and units are considered units and are represented with a Unit object.
void BetaStar::OnUnitIdle(const Unit* unit)
{
    switch (unit->unit_type.ToType()) {

        //case UNIT_TYPEID::PROTOSS_PROBE: {
        //    const Unit* target = FindResourceToGather(unit->pos);
        //    Actions()->UnitCommand(unit, ABILITY_ID::HARVEST_GATHER, target);
        //    break;
        //}

        default: {
            //std::cout << "Idle unit of type " << unit->unit_type.to_string() << std::endl;
            break;
        }
    }
}

void BetaStar::OnBuildingConstructionComplete(const Unit* unit)
{
    switch (unit->unit_type.ToType()) {

        case UNIT_TYPEID::PROTOSS_PYLON: {
            if (!m_proxy_pylon_completed && DistanceSquared2D(unit->pos, rotate_position(m_proxy_pylon_pos, m_enemy_base_quadrant)) < 5) {
                m_proxy_pylon_completed = true;
            }
            break;
        }

        case UNIT_TYPEID::PROTOSS_PHOTONCANNON: {
            if (!m_proxy_cannon_completed && DistanceSquared2D(unit->pos, rotate_position(m_proxy_cannon_positions[0], m_enemy_base_quadrant)) < 5) {
                m_proxy_cannon_completed = true;
            }
            break;
        }

        case UNIT_TYPEID::PROTOSS_NEXUS: {
            m_building_nexus = false;
            break;
        }
    }
}

void BetaStar::OnUnitEnterVision(const Unit* unit)
{
    // initial scouting probe found it
    if (!m_enemy_base_scouted && IsStructure(unit->unit_type)) {
        for (const auto& scouting_probe : m_scouting_probes) {
            float distance = DistanceSquared2D(unit->pos, scouting_probe->pos);
            if (!m_enemy_base_scouted && distance < 150) {
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
                    m_enemy_base_quadrant = get_starting_position_of_point(m_enemy_base_pos);
                    m_enemy_base_scouted = true;
                }
            }
        }
        if (m_enemy_base_scouted) {
            for (const auto& scouting_probe : m_scouting_probes) {
                const_cast<Unit*>(scouting_probe)->orders.clear();
                Actions()->UnitCommand(scouting_probe, ABILITY_ID::MOVE, rotate_position(Point2D(32, 81), m_enemy_base_quadrant));
            }
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
