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

    int sum_assigned_harvesters = 0;    // # of workers currently working
    int sum_ideal_harvesters = 0;       // ideal # of workers

    int workers_training = 0;   // # of workers currently being trained

    const Units bases = FriendlyUnitsOfType(m_base_typeid);
    const Units gases = FriendlyUnitsOfType(m_gas_building_typeid);

    // compute base worker counts
    // compute number of workers being trained
    for (const auto& base : bases) {
        // base still under construction
        if (base->build_progress != 1) {
            continue;
        }
        sum_assigned_harvesters += base->assigned_harvesters;
        sum_ideal_harvesters += base->ideal_harvesters;

        // check for workers being trained
        for (const auto& order : base->orders) {
            if (order.ability_id == m_worker_train_abilityid) {
                ++workers_training;
            }
        }
    }

    // compute gas worker counts
    for (const auto& gas : gases) {
        // gas still under construction
        if (gas->build_progress != 1) {
            continue;
        }
        sum_assigned_harvesters += gas->assigned_harvesters;
        sum_ideal_harvesters += gas->ideal_harvesters;
    }

    for (const auto& base : bases) {
        // check whether we should train another worker, if not then return
        if (sum_assigned_harvesters + workers_training >= sum_ideal_harvesters) {
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
        if (base->orders.size() > 0) {
            continue;
        }

        // train a worker
        Actions()->UnitCommand(base, m_worker_train_abilityid);

        // update stats
        ++workers_training;
        --supply_left;
        num_minerals -= 50;
    }
}

// constructs additional pylons
// TODO: better placement logic
void BetaStar::OnStepBuildPylons()
{
    const ObservationInterface* observation = Observation();

    // build a pylon if we have less than supply_threshold supply left
    const int supply_threshold = 4;

    int num_minerals = observation->GetMinerals();
    int supply_left = (observation->GetFoodCap() - observation->GetFoodUsed());

    // check whether we can afford a pylon
    if (num_minerals < 100) {
        return;
    }

    // check whether we're over the supply threshold to build a pylon
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
    const Unit* worker_to_build = GetRandomEntry(workers);

    // find a position where we can build the pylon
    float rx = GetRandomScalar();
    float ry = GetRandomScalar();
    Point2D pylon_pos = Point2D((worker_to_build->pos.x + (rx * 10.0f)), (worker_to_build->pos.y + (ry * 10.0f)));
    while (!Query()->Placement(m_supply_building_abilityid, pylon_pos)) {
        rx = GetRandomScalar();
        ry = GetRandomScalar();
        pylon_pos = Point2D((worker_to_build->pos.x + (rx * 10.0f)), (worker_to_build->pos.y + (ry * 10.0f)));
    }

    // build a pylon
    Actions()->UnitCommand(worker_to_build, m_supply_building_abilityid, pylon_pos);
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

    // check whether we have any gas open
    if (gases.size() >= (2 * bases.size())) {
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

    // if we don't have enough workers on minerals then return
    for (const auto& base : bases) {
        if (base->build_progress != 1) {
            continue;
        }
        if (base->assigned_harvesters < base->ideal_harvesters) {
            return;
        }
    }

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

    // build gas
    Actions()->UnitCommand(closest_worker, m_gas_building_abilityid, closest_geyser);
}

void BetaStar::OnStepExpand()
{
    const ObservationInterface* observation = Observation();

    int num_minerals = observation->GetMinerals();

    // check whether we can afford a new base
    if (num_minerals < 400) {
        return;
    }

    Units bases = FriendlyUnitsOfType(m_base_typeid);

    for (const auto& base : bases) {
        if (base->build_progress != 1) {
            return;
        }
        if (base->assigned_harvesters < base->ideal_harvesters) {
            return;
        }
    }

    Units gases = FriendlyUnitsOfType(m_gas_building_typeid);

    if (gases.size() < (2 * bases.size())) {
        return;
    }

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

    for (const auto& worker : workers) {
        for (const auto& order : worker->orders) {
            if (order.ability_id == m_base_building_abilityid) {
                return;
            }
        }
    }

    float minimum_distance = std::numeric_limits<float>::max();
    Point3D closest_expansion;
    for (const auto& expansion : expansion_locations) {
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

    Actions()->UnitCommand(closest_worker, m_base_building_abilityid, closest_expansion);
}

void BetaStar::OnStepManageWorkers()
{

}
