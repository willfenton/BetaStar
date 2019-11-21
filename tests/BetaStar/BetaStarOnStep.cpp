#include "BetaStar.h"

using namespace sc2;


// compute useful statistics and store them in the class
// for things we want to compute each step which won't change until next step
void BetaStar::OnStepComputeStatistics()
{
    //const ObservationInterface* observation = Observation();

    // how much supply we have left
    //m_supply_left = (observation->GetFoodCap() - observation->GetFoodUsed());
}

// train workers if we need them
// some things in this function could be sped up with caching, if neccessary.
void BetaStar::OnStepTrainWorkers()
{
    const ObservationInterface* observation = Observation();

    int num_minerals = observation->GetMinerals();
    int supply_left = (observation->GetFoodCap() - observation->GetFoodUsed());

    // check whether we have enough minerals to train a worker
    if (num_minerals < 50) {
        return;
    }

    // check whether we have enough supply to train a worker
    if (supply_left == 0) {
        return;
    }

    int sum_ideal_harvesters = 0;                         // ideal # of workers
    int total_workers = (int)CountUnitType(m_worker_typeid);   // total # of workers (including those in training)

    const Units bases = FriendlyUnitsOfType(m_base_typeid);
    const Units gases = FriendlyUnitsOfType(m_gas_building_typeid);

    // compute base worker counts
    // compute number of workers being trained
    for (const auto& base : bases) {
        // base still under construction
        if (base->build_progress != 1) {
            sum_ideal_harvesters += 16;  // train workers for the base under construction
            continue;
        }
        sum_ideal_harvesters += base->ideal_harvesters;
    }

    // compute gas worker counts
    for (const auto& gas : gases) {
        // gas still under construction
        if (gas->build_progress != 1) {
            sum_ideal_harvesters += 3;  // train 3 workers so that the gas can be worked once it's done
            continue;
        }
        sum_ideal_harvesters += gas->ideal_harvesters;
    }

    for (const auto& base : bases) {
        // check whether we should train another worker, if not then return
        if (total_workers >= sum_ideal_harvesters) {
            return;
        }
        if (num_minerals < 50) {
            return;
        }
        if (supply_left == 0) {
            return;
        }

        // check whether the base can train a worker, if not then continue
        if (base->build_progress != 1) {
            continue;
        }

        // attempt to train a worker - will not fill queue, which leaves us with more resource flexibility
        // (and our bot has instant response, so doesn't need a full queue)
        if (TrainUnit(base, m_worker_typeid))
        {
            // update stats if worker was added to training queue
            ++total_workers;
            --supply_left;
            num_minerals -= 50;
        }
    }
}

// constructs additional pylons
// TODO: better placement logic
void BetaStar::OnStepBuildPylons()
{
    const ObservationInterface* observation = Observation();

    // build a pylon if we have less than supply_threshold supply left
    const int supply_threshold = 6;

    int num_minerals = observation->GetMinerals();
    int supply_left = (observation->GetFoodCap() - observation->GetFoodUsed());

    // check whether we can afford a pylon
    if (num_minerals < 100) {
        return;
    }

    // check whether we're over or equal the supply threshold to build a pylon
    if (supply_left >= supply_threshold) {
        return;
    }

    const Units pylons = FriendlyUnitsOfType(m_supply_building_typeid);

    // check whether we are already building a pylon
    for (const auto& pylon : pylons) {
        if (pylon->build_progress != 1) {
            return;
        }
    }

    Units workers = FriendlyUnitsOfType(m_worker_typeid);

    // maybe unneccessary, we're kind of screwed anyways if this is the case
    if (workers.empty()) {
        return;
    }

    for (const auto& worker : workers) {
        for (const auto& order : worker->orders) {
            if (order.ability_id == m_supply_building_abilityid) {
                return;
            }
        }
    }

    // select the worker to build the pylon
    const Unit* worker_to_build = nullptr;
    for (const auto& worker : workers) {
        if (worker->tag != m_initial_scouting_probe->tag) {
            worker_to_build = worker;
            break;
        }
    }

    if (worker_to_build == nullptr) {
        return;
    }

    // find a position where we can build the pylon
    Point2D pylon_pos;

    if (pylons.size() < m_first_pylon_positions.size()) {
        pylon_pos = m_first_pylon_positions[pylons.size()];
        pylon_pos = rotate_position(pylon_pos, m_starting_quadrant);

        // Add pylon to vector of placed pylon positions
        m_placed_pylon_positions.push_back(pylon_pos);
    }
    else if (pylons.size() < m_placed_pylon_positions.size()) {
        // A pylon must have been destroyed, repair it
        for (const Point2D& pylon_position : m_placed_pylon_positions) {
            // Check if this was the destroyed pylon
            if (Query()->Placement(m_supply_building_abilityid, pylon_position)) {
                //std::cout << "Pylon pos: (" << pylon_position.x << ", " << pylon_position.y << ")" << std::endl;
                // Pylon destroyed, place new one here
                pylon_pos = pylon_position;
                break;
            }
        }
    }
    else {
        // if placement is invalid, try again next game loop to remove possibility of infinite loop
        float rx = GetRandomScalar();
        float ry = GetRandomScalar();
        pylon_pos = Point2D((worker_to_build->pos.x + (rx * 10.0f)), (worker_to_build->pos.y + (ry * 10.0f)));

        // Add pylon to vector of placed pylon positions
        m_placed_pylon_positions.push_back(pylon_pos);
    }

    // build a pylon if the position is free
    if (Query()->Placement(m_supply_building_abilityid, pylon_pos))
    {
        Actions()->UnitCommand(worker_to_build, m_supply_building_abilityid, pylon_pos);
    }
}

// build more gas if neccessary
// this could probably be optimized further
void BetaStar::OnStepBuildGas()
{
    const ObservationInterface* observation = Observation();

    int num_minerals = observation->GetMinerals();

    // check whether we can afford a gas
    if (num_minerals < 75) {
        return;
    }

    Units bases = FriendlyUnitsOfType(m_base_typeid);
    Units gases = FriendlyUnitsOfType(m_gas_building_typeid);

    int num_complete_bases = 0;

    for (const auto& base : bases) {
        if (base->build_progress == 1) {
            ++num_complete_bases;
        }
    }

    // check whether we have any gas open
    if (gases.size() >= (2 * num_complete_bases)) {
        return;
    }

    // if we are already building gas then return
    // if gas isn't saturated then return
    for (const auto& gas : gases) {
        if (gas->build_progress != 1) {
            return;
        }
        if (gas->assigned_harvesters < gas->ideal_harvesters) {
            return;
        }
    }

    //NOTE: Commented out because it can needlessly delay moving through the build order if, for example, one worker is building/scouting so minerals are desaturated
    // if we don't have enough workers on minerals then return
    /*for (const auto& base : bases) {
        if (base->build_progress != 1) {
            continue;
        }
        if (base->assigned_harvesters < base->ideal_harvesters) {
            return;
        }
    }*/

    Units geysers = observation->GetUnits(Unit::Alliance::Neutral, IsUnit(UNIT_TYPEID::NEUTRAL_VESPENEGEYSER));

    // find the closest geyser to one of our bases
    float minimum_distance = 225.0f;
    const Unit* closest_geyser = nullptr;
    for (const auto& geyser : geysers) {
        for (const auto& base : bases) {
            float distance = DistanceSquared2D(base->pos, geyser->pos);
            if (distance < minimum_distance) {
                if (Query()->Placement(m_gas_building_abilityid, geyser->pos)) {
                    minimum_distance = distance;
                    closest_geyser = geyser;
                }
            }
        }
    }

    // we didn't find a geyser close enough to one of our bases
    if (closest_geyser == nullptr) {
        return;
    }

    Units workers = FriendlyUnitsOfType(m_worker_typeid);

    if (workers.empty()) {
        return;
    }

    // check to see if there is already a worker heading out to build it
    for (const auto& worker : workers) {
        for (const auto& order : worker->orders) {
            if (order.ability_id == m_gas_building_abilityid) {
                return;
            }
        }
    }

    // find closest worker to build it
    const Unit* closest_worker = nullptr;
    float closest_distance = std::numeric_limits<float>::max();
    for (const auto& worker : workers) {
        float distance = DistanceSquared2D(closest_geyser->pos, worker->pos);
        if (distance < closest_distance) {
            closest_worker = worker;
            closest_distance = distance;
        }
    }

    if (closest_worker == nullptr) {
        std::cerr << "This should never happen (OnStepBuildGas)" << std::endl;
        exit(1);
    }

    // build gas (no need for additional checks since we've already checked to make sure we have the minerals)
    Actions()->UnitCommand(closest_worker, m_gas_building_abilityid, closest_geyser);
}

// logic for building new bases
// TODO: check for units blocking the expansion
void BetaStar::OnStepExpand()
{
    const ObservationInterface* observation = Observation();

    int num_minerals = observation->GetMinerals();

    // check whether we can afford a new base
    if (num_minerals < 400) {
        return;
    }

    Units bases = FriendlyUnitsOfType(m_base_typeid);

    // check whether we have enough bases
    if (bases.size() >= m_max_bases) {
        return;
    }

    // check if any of our bases still need workers, if so then return
    for (const auto& base : bases) {
        if (base->build_progress != 1) {
            return;
        }
        if (base->assigned_harvesters < base->ideal_harvesters) {
            return;
        }
    }

    Units gases = FriendlyUnitsOfType(m_gas_building_typeid);

    // check whether we still need to build gases
    //if (gases.size() < (2 * bases.size())) {
        //return;
    //}

    // check if any of our gases still need workers, if so then return
    for (const auto& gas : gases) {
        if (gas->build_progress != 1) {
            continue;
        }
        if (gas->assigned_harvesters < gas->ideal_harvesters) {
            return;
        }
    }

    Units workers = FriendlyUnitsOfType(m_worker_typeid);

    if (workers.empty()) {
        return;
    }

    // check whether a worker is already on its way to build a base
    for (const auto& worker : workers) {
        for (const auto& order : worker->orders) {
            if (order.ability_id == m_base_building_abilityid) {
                return;
            }
        }
    }

    // find the closest expansion location
    float minimum_distance = std::numeric_limits<float>::max();
    Point3D closest_expansion;
    for (const auto& expansion : m_expansion_locations) {
        float current_distance = Distance2D(m_starting_pos, expansion);
        if (current_distance < .01f) {
            continue;
        }
        if (current_distance < minimum_distance) {
            if (Query()->Placement(m_base_building_abilityid, expansion)) {
                closest_expansion = expansion;
                minimum_distance = current_distance;
            }
        }
    }

    // find closest worker to build it
    const Unit* closest_worker = nullptr;
    float closest_distance = std::numeric_limits<float>::max();
    for (const auto& worker : workers) {
        float distance = DistanceSquared2D(closest_expansion, worker->pos);
        if (distance < closest_distance) {
            closest_worker = worker;
            closest_distance = distance;
        }
    }

    // order worker to build the base (no need for additional checks since we've already made sure we have the minerals)
    Actions()->UnitCommand(closest_worker, m_base_building_abilityid, closest_expansion);
}

// Struct used in OnStepManageWorkers()
struct UnevenResource
{
    int worker_diff;
    const Unit* unit;

    UnevenResource(const Unit* unit)
        : unit(unit)
    {
        worker_diff = abs(unit->ideal_harvesters - unit->assigned_harvesters);
    }
};

// Move workers from oversaturated resources to undersaturated resources
// Also gives orders to idle workers
// BUG: doesn't seem to catch workers who built assimilators, they wait around until it's done and then start working there
// This could probably be set to run every couple steps instead of every step
void BetaStar::OnStepManageWorkers()
{
    const ObservationInterface* observation = Observation();

    Units bases = FriendlyUnitsOfType(m_base_typeid);
    Units gases = FriendlyUnitsOfType(m_gas_building_typeid);

    if (bases.empty()) {
        return;
    }

    std::vector<UnevenResource> oversaturated;
    std::vector<UnevenResource> undersaturated;

    // add over / undersaturated bases to the respective vectors
    for (const auto& base : bases) {
        if (base->build_progress != 1) {
            continue;
        }
        int worker_diff = (base->ideal_harvesters - base->assigned_harvesters);
        if (worker_diff == 0) {
            continue;
        }
        if (worker_diff > 0) {
            undersaturated.push_back(UnevenResource(base));
            continue;
        }
        if (worker_diff < 0) {
            oversaturated.push_back(UnevenResource(base));
            continue;
        }
    }

    // add over / undersaturated gases to the respective vectors
    for (const auto& gas : gases) {
        if (gas->build_progress != 1) {
            continue;
        }
        int worker_diff = (gas->ideal_harvesters - gas->assigned_harvesters);
        if (worker_diff == 0) {
            continue;
        }
        if (worker_diff > 0) {
            undersaturated.push_back(UnevenResource(gas));
            continue;
        }
        if (worker_diff < 0) {
            oversaturated.push_back(UnevenResource(gas));
            continue;
        }
    }

    Units workers = FriendlyUnitsOfType(m_worker_typeid);
    Units idle_workers;

    // iterate through workers, adding idle workers and workers on oversaturated resources to idle_workers
    for (const auto& worker : workers) {
        // idle worker
        if (worker->orders.size() == 0) {
            idle_workers.push_back(worker);
            continue;
        }
        // check for working oversaturated resource
        for (const auto& order : worker->orders) {
            for (auto& uneven_resource : oversaturated) {
                // already took workers off this resource
                if (uneven_resource.worker_diff == 0) {
                    continue;
                }
                // take worker off the oversaturated resource, update the struct
                if (order.target_unit_tag == uneven_resource.unit->tag) {
                    idle_workers.push_back(worker);
                    --uneven_resource.worker_diff;
                    continue;
                }
            }
        }
    }

    if (idle_workers.size() == 0) {
        return;
    }

    // iterate throuh undersatured resources, assigning workers from idle_workers to work them
    for (auto& uneven_resource : undersaturated) {
        while (idle_workers.size() > 0 && uneven_resource.worker_diff > 0) {
            // remove worker from idle_workers
            const Unit* worker = idle_workers.back();
            idle_workers.pop_back();

            // update struct
            --uneven_resource.worker_diff;

            // set worker to mine minerals from undersaturated base
            if (uneven_resource.unit->unit_type == m_base_typeid) {
                const Unit* mineral = FindNearestNeutralUnit(uneven_resource.unit->pos, UNIT_TYPEID::NEUTRAL_MINERALFIELD);
                Actions()->UnitCommand(worker, m_worker_gather_abilityid, mineral);
            }
            // set worker to mine gas from undersaturated gas
            if (uneven_resource.unit->unit_type == m_gas_building_typeid) {
                Actions()->UnitCommand(worker, m_worker_gather_abilityid, uneven_resource.unit);
            }
        }
    }

    // maybe check here if there are still idle workers, reassign them?
}

void BetaStar::OnStepBuildOrder()
{
    const ObservationInterface* observation = Observation();

    int num_minerals = observation->GetMinerals();
    int num_gas = observation->GetVespene();

    size_t num_bases = CountUnitType(UNIT_TYPEID::PROTOSS_NEXUS);
    size_t num_gases = CountUnitType(UNIT_TYPEID::PROTOSS_ASSIMILATOR);
    size_t num_pylons = CountUnitType(UNIT_TYPEID::PROTOSS_PYLON);
    size_t num_gateways = CountUnitType(UNIT_TYPEID::PROTOSS_GATEWAY);
    size_t num_warpgates = CountUnitType(UNIT_TYPEID::PROTOSS_WARPGATE);
    size_t num_forges = CountUnitType(UNIT_TYPEID::PROTOSS_FORGE);
    size_t num_cybernetics_cores = CountUnitType(UNIT_TYPEID::PROTOSS_CYBERNETICSCORE);
    size_t num_twilight_councils = CountUnitType(UNIT_TYPEID::PROTOSS_TWILIGHTCOUNCIL);

    // Determine if a structure has been destroyed
    for (int i = 0; i < m_buildings.size(); ++i) {
        auto& building_info = m_buildings[i];
        Point2D build_location = std::get<0>(building_info);
        if (Query()->Placement(std::get<1>(building_info), build_location)) {
            // Space empty, building was destroyed here
            TryBuildStructureNearPylon(std::get<1>(building_info), m_worker_typeid);
            m_buildings.erase(m_buildings.begin() + i); // Remove tuple
            return;
        }
    }

    if (num_pylons >= 1 && (num_gateways + num_warpgates) < 1 && num_minerals >= 150) {
        TryBuildStructureNearPylon(ABILITY_ID::BUILD_GATEWAY, m_worker_typeid);
        return;
    }

    if ((num_gateways + num_warpgates) > 0 && num_gases < 1 && num_minerals >= 75) {
        OnStepBuildGas();
        return;
    }

    if (num_gases > 0 && num_cybernetics_cores < 1 && num_minerals >= 150) {
        TryBuildStructureNearPylon(ABILITY_ID::BUILD_CYBERNETICSCORE, m_worker_typeid);
        return;
    }

    if (num_twilight_councils < 1 && num_cybernetics_cores > 0 && num_minerals >= 150 && num_gas >= 100) {
        TryBuildStructureNearPylon(ABILITY_ID::BUILD_TWILIGHTCOUNCIL, m_worker_typeid);
        return;
    }

    if (num_cybernetics_cores > 0 && num_gases < 2 && num_minerals >= 75) {
        OnStepBuildGas();
        return;
    }

    if (m_blink_researching && num_gases >= 2 && (num_gateways + num_warpgates) < 4 && num_minerals >= 150) {
        TryBuildStructureNearPylon(ABILITY_ID::BUILD_GATEWAY, m_worker_typeid);
        return;
    }
}

//Try to get upgrades depending on build
void BetaStar::OnStepResearchUpgrades() {
    const ObservationInterface* observation = Observation();

    std::vector<UpgradeID> upgrades = observation->GetUpgrades();

    Units bases = FriendlyUnitsOfType(m_base_typeid);
    size_t base_count = bases.size();

    for (const auto& cybernetics_core : FriendlyUnitsOfType(UNIT_TYPEID::PROTOSS_CYBERNETICSCORE)) {
        if (cybernetics_core->build_progress != 1) {
            continue;
        }
        for (const auto& order : cybernetics_core->orders) {
            if (order.ability_id == ABILITY_ID::RESEARCH_WARPGATE) {
                m_warpgate_researching = true;
            }
        }
    }

    for (const auto& twilight_council : FriendlyUnitsOfType(UNIT_TYPEID::PROTOSS_TWILIGHTCOUNCIL)) {
        if (twilight_council->build_progress != 1) {
            continue;
        }
        for (const auto& order : twilight_council->orders) {
            if (order.ability_id == ABILITY_ID::RESEARCH_BLINK) {
                m_blink_researching = true;
            }
        }
    }

    if (!m_warpgate_researching) {
        TryResearchUpgrade(ABILITY_ID::RESEARCH_WARPGATE, UNIT_TYPEID::PROTOSS_CYBERNETICSCORE);
    }
    if (!m_blink_researching) {
        TryResearchUpgrade(ABILITY_ID::RESEARCH_BLINK, UNIT_TYPEID::PROTOSS_TWILIGHTCOUNCIL);
    }

    //if (upgrades.empty()) {
    //    TryResearchUpgrade(ABILITY_ID::RESEARCH_WARPGATE, UNIT_TYPEID::PROTOSS_CYBERNETICSCORE);
    //}
    //else {
    //    for (const auto& upgrade : upgrades) {
    //        if (upgrade == UPGRADE_ID::PROTOSSGROUNDWEAPONSLEVEL1 && base_count > 2) {
    //            TryResearchUpgrade(ABILITY_ID::RESEARCH_PROTOSSGROUNDWEAPONS, UNIT_TYPEID::PROTOSS_FORGE);
    //        }
    //        else if (upgrade == UPGRADE_ID::PROTOSSGROUNDARMORSLEVEL1 && base_count > 2) {
    //            TryResearchUpgrade(ABILITY_ID::RESEARCH_PROTOSSGROUNDARMOR, UNIT_TYPEID::PROTOSS_FORGE);
    //        }
    //        else if (upgrade == UPGRADE_ID::PROTOSSGROUNDWEAPONSLEVEL2 && base_count > 3) {
    //            TryResearchUpgrade(ABILITY_ID::RESEARCH_PROTOSSGROUNDWEAPONS, UNIT_TYPEID::PROTOSS_FORGE);
    //        }
    //        else if (upgrade == UPGRADE_ID::PROTOSSGROUNDARMORSLEVEL2 && base_count > 3) {
    //            TryResearchUpgrade(ABILITY_ID::RESEARCH_PROTOSSGROUNDARMOR, UNIT_TYPEID::PROTOSS_FORGE);
    //        }
    //        else {
    //            //TryResearchUpgrade(ABILITY_ID::RESEARCH_EXTENDEDTHERMALLANCE, UNIT_TYPEID::PROTOSS_ROBOTICSBAY);
    //            TryResearchUpgrade(ABILITY_ID::RESEARCH_BLINK, UNIT_TYPEID::PROTOSS_TWILIGHTCOUNCIL);
    //            //TryResearchUpgrade(ABILITY_ID::RESEARCH_CHARGE, UNIT_TYPEID::PROTOSS_TWILIGHTCOUNCIL);
    //            TryResearchUpgrade(ABILITY_ID::RESEARCH_PROTOSSGROUNDWEAPONS, UNIT_TYPEID::PROTOSS_FORGE);
    //            TryResearchUpgrade(ABILITY_ID::RESEARCH_PROTOSSGROUNDARMOR, UNIT_TYPEID::PROTOSS_FORGE);
    //        }
    //    }
    //}
}

void BetaStar::OnStepBuildArmy()
{
    const ObservationInterface* observation = Observation();

    int num_minerals = observation->GetMinerals();
    int num_gas = observation->GetVespene();

    Units gateways = FriendlyUnitsOfType(UNIT_TYPEID::PROTOSS_GATEWAY);
    Units warpgates = FriendlyUnitsOfType(UNIT_TYPEID::PROTOSS_WARPGATE);

    Units twilight_councils = FriendlyUnitsOfType(UNIT_TYPEID::PROTOSS_TWILIGHTCOUNCIL);

    Units cybernetics_cores = FriendlyUnitsOfType(UNIT_TYPEID::PROTOSS_CYBERNETICSCORE);

    bool core_built = false;
    for (const auto& core : cybernetics_cores) {
        if (core->build_progress == 1) {
            core_built = true;
            break;
        }
    }

    if (twilight_councils.size() == 0) {
        return;
    }

    if (core_built)
    {
        if (m_warpgate_researched && m_attacking)
        {
            for (const Unit *warpgate : FriendlyUnitsOfType(UNIT_TYPEID::PROTOSS_WARPGATE))
            {
                if (WarpUnit(warpgate, m_enemy_base_pos, UNIT_TYPEID::PROTOSS_STALKER))
                {
                    num_minerals -= 125;
                    num_gas -= 125;
                }
            }
        }

        int num_built = (int)MassTrainUnit(UNIT_TYPEID::PROTOSS_STALKER);
        num_minerals -= num_built * 125;
        num_gas -= num_built * 50;
    }
}

void BetaStar::OnStepManageArmy()
{
    // Switch to offensive mode. Trigger will be different for each strategy.
    // TODO: Turn off offensive mode if it's going to lose us the match. 
    switch (m_current_strategy)
    {
        case Strategy::Blink_Stalker_Rush:
            if (!m_attacking && m_blink_researched && m_enemy_base_scouted && Observation()->GetArmyCount() >= 12) {
                m_attacking = true;
            }
            break;
        default:
            if (!m_attacking && m_blink_researched && m_enemy_base_scouted && Observation()->GetArmyCount() >= 20) {
                m_attacking = true;
            }
            break;
    }

    // defend our base
    if (!m_attacking) {
        for (UNIT_TYPEID unitType : managed_unit_types)
        {
            BaseDefenseMacro(FriendlyUnitsOfType(unitType));
        }
    }
    // attack enemy base
    else
    {
        for (UNIT_TYPEID unitType : managed_unit_types)
        {
            EnemyBaseAttackMacro(FriendlyUnitsOfType(unitType));
        }
    }

    // This micro is always valid to perform
    StalkerBlinkMicro();
}

void BetaStar::OnStepChronoBoost()
{
    // TODO: Could possibly create more explicit ordering to break ties for concurrent upgrades
    //       would need to have a custom ordering for each strategy though

    Units nexuses = FriendlyUnitsOfType(UNIT_TYPEID::PROTOSS_NEXUS);

    // minimum energy required to chrono boost
    float minEnergy = 50.0f;
    // minimum energy we want to have before spending some on non-essential chrono boosting
    float minEnergyStingy = 100.0f;

    // make sure at least one nexus has enough energy to chrono
    float maxEnergy = 0.0f;
    for (const Unit *unit : nexuses)
    {
        if (unit->energy >= maxEnergy)
        {
            maxEnergy = unit->energy;
        }
    }

    // don't have energy for chrono, don't need to look for things to chrono
    if (maxEnergy < minEnergy)
    {
        return;
    }

    Units allUnits = Observation()->GetUnits(Unit::Alliance::Self);
    // chrono boost lasts 20 seconds in the current game version, but we can't grab this with the current API
    // chrono boost performs 30 seconds of work, so this is how much time we want remaining for an optimal application
    float chronoBoostPseudoTime = 30.0f;

    // unit to apply chrono boost to this frame
    const Unit *bestChronoCandidate = nullptr;
    // highest priority is 3, lowest is 0, -1 is invalid
    int bestCandidatePriority = -1;

    for (const Unit *unit : allUnits)
    {
        // save some time since we'll never beat this priority
        if (bestCandidatePriority == 3)
        {
            break;
        }

        // Chrono Boost only works on structures
        if (IsStructure(unit->unit_type))
        {
            // if we don't find the buff, possibly apply it to this unit (281 is Chrono Boost)
            if (std::find(unit->buffs.begin(), unit->buffs.end(), BuffID(281)) == unit->buffs.end())
            {
                for (UnitOrder order : unit->orders)
                {
                    // buildings with research queues
                    if (bestCandidatePriority < 3 && maxEnergy >= minEnergy)
                    {
                        // calculate how much research time this building still has to work through
                        float totalRemainingTime = 0.0f;
                        for (UpgradeData ud : all_upgrades)
                        {
                            if (ud.ability_id == order.ability_id)
                            {
                                totalRemainingTime += ud.research_time * order.progress;
                            }
                        }
                        // if we will fully utilize the chrono boost, apply it
                        if (totalRemainingTime >= chronoBoostPseudoTime)
                        {
                            bestChronoCandidate = unit;
                            bestCandidatePriority = 3;
                            break;
                        }
                    }

                    // special case for a Nexus that's trying to build workers
                    if (bestCandidatePriority < 2 && maxEnergy >= minEnergy)
                    {
                        if (unit->unit_type == UNIT_TYPEID::PROTOSS_NEXUS)
                        {
                            if (m_worker_train_abilityid == order.ability_id)
                            {
                                bestChronoCandidate = unit;
                                bestCandidatePriority = 2;
                                break;
                            }
                        }
                    }

                    // buildings with unit production queues
                    // std::find with functor would likely be faster
                    if (bestCandidatePriority < 1 && maxEnergy >= minEnergyStingy)
                    {
                        for (UnitTypeData utd : all_unit_type_data)
                        {
                            if (utd.ability_id == order.ability_id)
                            {
                                //totalRemainingTime += utd.build_time * order.progress;
                                // our bot doesn't build real queues, so assume they're infinite as a workaround
                                bestChronoCandidate = unit;
                                bestCandidatePriority = 1;
                                break;
                            }
                        }
                    }
                }

                // computationally expensive solution for warpgate chrono (don't think there's a way to do it without a query)
                if (bestCandidatePriority < 0 && maxEnergy >= minEnergyStingy)
                {
                    if (unit->unit_type == UNIT_TYPEID::PROTOSS_WARPGATE)
                    {
                        size_t abilityCount = Query()->GetAbilitiesForUnit(unit, true).abilities.size();
                        size_t availableCount = Query()->GetAbilitiesForUnit(unit).abilities.size();

                        // can't check cooldowns, so assume we'll be building as soon as it's off cooldown (== infinite queue)
                        if (abilityCount != availableCount)
                        {
                            bestChronoCandidate = unit;
                            bestCandidatePriority = 1;
                        }
                    }
                }
            }
        }
    }

    if (bestCandidatePriority > -1)
    {
        for (const auto &nexus : nexuses) {
            // If successful, break. Otherwise, try next Nexus
            if(TryIssueCommand(nexus, AbilityID(3755), bestChronoCandidate))
            {
                break;
            }
        }
    }
}
