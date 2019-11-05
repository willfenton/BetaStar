#include <iostream>
#include "sc2api/sc2_api.h"
#include "sc2api/sc2_args.h"
#include "sc2lib/sc2_lib.h"
#include "sc2utils/sc2_manage_process.h"
#include "sc2utils/sc2_arg_parser.h"

#include "LadderInterface.h"

class BetaStar : public sc2::Agent {
public:
    // this function runs at the start of the game
	virtual void OnGameStart() final {
		const sc2::ObservationInterface* observation = Observation();
		std::cout << "I am player number " << observation->GetPlayerID() << std::endl;
	};

    // called each time the coordinator steps the simulation forward
	virtual void OnStep() final {

        // interface that contains functions to examine the current game state
		//const sc2::ObservationInterface* observation = Observation();

        // issues actions to units
		//sc2::ActionInterface* action = Actions();

        TryBuildSupplyDepot();

        TryBuildBarracks();

	};

private:

    // Called each time a unit has been built and has no orders or the unit had orders in the previous step and currently does not
    // Both buildings and units are considered units and are represented with a Unit object.
    virtual void OnUnitIdle(const sc2::Unit* unit) final {
        switch (unit->unit_type.ToType()) {
            case sc2::UNIT_TYPEID::TERRAN_COMMANDCENTER: {
                Actions()->UnitCommand(unit, sc2::ABILITY_ID::TRAIN_SCV);
                break;
            }
            case sc2::UNIT_TYPEID::TERRAN_SCV: {
                const sc2::Unit* mineral_target = FindNearestNeutralUnit(unit->pos, sc2::UNIT_TYPEID::NEUTRAL_MINERALFIELD);
                if (!mineral_target) {
                    break;
                }
                // The ability type of SMART is equivalent to a right click in Starcraft 2 when you have a unit selected.
                // Right clicking a mineral patch with an SCV commands it to mine from that patch
                Actions()->UnitCommand(unit, sc2::ABILITY_ID::SMART, mineral_target);
                break;
            }
            case sc2::UNIT_TYPEID::TERRAN_BARRACKS: {
                Actions()->UnitCommand(unit, sc2::ABILITY_ID::TRAIN_MARINE);
                break;
            }
            case sc2::UNIT_TYPEID::TERRAN_MARINE: {
                const sc2::GameInfo& game_info = Observation()->GetGameInfo();
                Actions()->UnitCommand(unit, sc2::ABILITY_ID::ATTACK_ATTACK, game_info.enemy_start_locations.front());
                break;
            }
            default: {
                break;
            }
        }
    }

    size_t CountUnitType(sc2::UNIT_TYPEID unit_type) {
        return Observation()->GetUnits(sc2::Unit::Alliance::Self, sc2::IsUnit(unit_type)).size();
    }

    bool TryBuildStructure(sc2::ABILITY_ID ability_type_for_structure, sc2::UNIT_TYPEID unit_type = sc2::UNIT_TYPEID::TERRAN_SCV)
    {
        const sc2::ObservationInterface* observation = Observation();

        // If a unit already is building a supply structure of this type, do nothing.
        // Also get an scv to build the structure.
        const sc2::Unit* unit_to_build = nullptr;
        sc2::Units units = observation->GetUnits(sc2::Unit::Alliance::Self);
        for (const auto& unit : units) {
            for (const auto& order : unit->orders) {
                if (order.ability_id == ability_type_for_structure) {
                    return false;
                }
            }

            if (unit->unit_type == unit_type) {
                unit_to_build = unit;
            }
        }

        float rx = sc2::GetRandomScalar();
        float ry = sc2::GetRandomScalar();

        Actions()->UnitCommand(unit_to_build, ability_type_for_structure, sc2::Point2D(unit_to_build->pos.x + rx * 15.0f, unit_to_build->pos.y + ry * 15.0f));

        return true;
    }

    bool TryBuildSupplyDepot() {
        const sc2::ObservationInterface* observation = Observation();

        // If we are not supply capped, don't build a supply depot.
        if (observation->GetFoodUsed() <= observation->GetFoodCap() - 2) {
            return false;
        }

        // Try and build a depot. Find a random SCV and give it the order.
        return TryBuildStructure(sc2::ABILITY_ID::BUILD_SUPPLYDEPOT);
    }

    bool TryBuildBarracks() {
        const sc2::ObservationInterface* observation = Observation();

        if (CountUnitType(sc2::UNIT_TYPEID::TERRAN_SUPPLYDEPOT) < 1) {
            return false;
        }

        if (CountUnitType(sc2::UNIT_TYPEID::TERRAN_BARRACKS) > 0) {
            return false;
        }

        return TryBuildStructure(sc2::ABILITY_ID::BUILD_BARRACKS);
    }

    const sc2::Unit* FindNearestNeutralUnit(const sc2::Point2D& start, sc2::UNIT_TYPEID target_unit_type) {
        std::cout << Observation()->GetUnits(sc2::Unit::Alliance::Neutral).size() << std::endl;

        sc2::Units units = Observation()->GetUnits(sc2::Unit::Alliance::Neutral);

        float distance = std::numeric_limits<float>::max();
        const sc2::Unit* target = nullptr;

        for (const auto& unit : units) {
            if (unit->unit_type == target_unit_type) {
                float d = sc2::DistanceSquared2D(unit->pos, start);
                if (d < distance) {
                    distance = d;
                    target = unit;
                }
            }
        }
        return target;
    }

};

int main(int argc, char* argv[])
{
	RunBot(argc, argv, new BetaStar(), sc2::Race::Terran);
}
