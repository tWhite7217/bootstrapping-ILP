// import sys

// bootstrapping_latency = 12

#include <iostream>
#include <numeric>
// #include <algorithm>

#include "solution_validator.h"

SolutionValidator::SolutionValidator(SolutionValidatorConfig config) : config{config}
{
    unstarted_operations.resize(config.operations.size());
    std::iota(unstarted_operations.begin(), unstarted_operations.end(), 1);
}

void SolutionValidator::validate_solution()
{
    check_bootstrapping_constraints_are_met();

    while (program_is_not_complete())
    {
        clock_cycle += 1;

        bootstrapping_operations = decrement_cycles_left_for(bootstrapping_operations);
        running_operations = decrement_cycles_left_for(running_operations);

        bootstrapping_operations = remove_finished_operations(bootstrapping_operations);
        add_necessary_running_operations_to_bootstrapping_queue();
        running_operations = remove_finished_operations(running_operations);
        start_bootstrapping_ready_operations_from_queue();

        auto ready_operations = get_ready_operations();
        start_running_ready_operations(ready_operations);

        // std::cout << clock_cycle << std::endl;
        // std::cout << "Unstarted: ";
        // for (auto operation_id : unstarted_operations)
        // {
        //     std::cout << operation_id << " ";
        // }
        // std::cout << std::endl;
        // std::cout << "Running: ";
        // for (auto [operation_id, cycles_left] : running_operations)
        // {
        //     std::cout << operation_id << "," << cycles_left << " ";
        // }
        // std::cout << std::endl;
        // std::cout << "Ready to BS: ";
        // for (auto operation_id : operations_ready_to_bootstrap)
        // {
        //     std::cout << operation_id << " ";
        // }
        // std::cout << std::endl;
        // std::cout << "Bootstrapping: ";
        // for (auto [operation_id, cycles_left] : bootstrapping_operations)
        // {
        //     std::cout << operation_id << "," << cycles_left << " ";
        // }
        // std::cout << std::endl;
        // std::cout << std::endl;
    }

    if (clock_cycle != config.solver_latency)
    {
        std::cout << "Error: The latencies mismatch" << std::endl;
        std::cout << "Solver latency: " << config.solver_latency << std::endl;
        std::cout << "Calculated latency: " << clock_cycle << std::endl;
    }
    else
    {
        std::cout << "ILP model results are correct" << std::endl;
    }
}

void SolutionValidator::check_bootstrapping_constraints_are_met()
{
    for (auto path : config.bootstrapping_paths)
    {
        bool path_satisfied = false;
        if (config.used_some_children_model)
        {
            for (auto i = 0;
                 i < config.bootstrapped_operation_ids.size() && !path_satisfied;
                 i += 2)
            {
                for (auto j = 0; j < path.size() - 1; j++)
                {
                    if (config.bootstrapped_operation_ids[i] == path[j] &&
                        config.bootstrapped_operation_ids[i + 1] == path[j + 1])
                    {
                        path_satisfied = true;
                        break;
                    }
                }
            }
        }
        else
        {
            for (auto bootstrapped_operation_id : config.bootstrapped_operation_ids)
            {
                if (vector_contains_element(path, bootstrapped_operation_id))
                {
                    path_satisfied = true;
                    break;
                }
            }
        }

        if (!path_satisfied)
        {
            std::cout << "Error: The following path was not satisfied" << std::endl;
            std::cout << "Path: ";
            for (auto operation_id : path)
            {
                std::cout << operation_id << ", ";
            }
            std::cout << std::endl;
            exit(0);
        }
    }
}

// void SolutionValidator::bootstrapping_constraints_are_met_for_some_children_model()
// {
// }

bool SolutionValidator::program_is_not_complete()
{
    return (unstarted_operations.size() > 0) ||
           (running_operations.size() > 0) ||
           (operations_ready_to_bootstrap.size() > 0) ||
           (bootstrapping_operations.size() > 0);
}

SolutionValidator::OperationIdToRemainingCyclesMap SolutionValidator::decrement_cycles_left_for(OperationIdToRemainingCyclesMap operation_map)
{
    for (auto [operation, cycles_left] : operation_map)
    {
        operation_map[operation] = cycles_left - 1;
    }
    return operation_map;
}

SolutionValidator::OperationIdToRemainingCyclesMap SolutionValidator::remove_finished_operations(OperationIdToRemainingCyclesMap operation_map)
{
    OperationIdToRemainingCyclesMap new_operation_map;
    for (auto [operation, cycles_left] : operation_map)
    {
        if (cycles_left > 0)
        {
            new_operation_map[operation] = cycles_left;
        }
    }
    return new_operation_map;
}

void SolutionValidator::add_necessary_running_operations_to_bootstrapping_queue()
{
    for (auto [operation_id, cycles_left] : running_operations)
    {
        if (cycles_left == 0 && operation_is_bootstrapped(operation_id))
        {
            operations_ready_to_bootstrap.push_back(operation_id);
        }
    }
}

bool SolutionValidator::operation_is_bootstrapped(int operation_id)
{
    if (config.used_some_children_model)
    {
        for (auto i = 0; i < config.bootstrapped_operation_ids.size(); i += 2)
        {
            if (config.bootstrapped_operation_ids[i] == operation_id)
            {
                return true;
            }
        }
        return false;
    }
    else
    {
        return vector_contains_element(config.bootstrapped_operation_ids, operation_id);
    }
}

void SolutionValidator::start_bootstrapping_ready_operations_from_queue()
{
    if (config.used_bootstrap_limited_model)
    {
        std::vector<int> operations_to_remove;
        for (auto operation_id : operations_ready_to_bootstrap)
        {
            if (clock_cycle_matches_operation_bootstrapping_start_time(operation_id))
            {
                auto [core_is_available, bootstrapping_operation] =
                    check_if_operation_bootstrapping_core_is_available(operation_id);
                if (core_is_available)
                {
                    bootstrapping_operations[operation_id] = config.bootstrapping_latency;
                    operations_to_remove.push_back(operation_id);
                }
                else
                {
                    print_bootstrapping_core_is_not_available_error(operation_id, bootstrapping_operation);
                    exit(0);
                }
            }
            else if (clock_cycle_is_later_than_operation_bootstrapping_start_time(operation_id))
            {
                print_missed_bootstrapping_deadline_error(operation_id);
                exit(0);
            }
        }
        for (auto operation_id : operations_to_remove)
        {
            remove_element_from_vector(operations_ready_to_bootstrap, operation_id);
        }
    }
    else
    {
        for (auto operation_id : operations_ready_to_bootstrap)
        {
            bootstrapping_operations[operation_id] = config.bootstrapping_latency;
        }
        operations_ready_to_bootstrap.clear();
    }
}

bool SolutionValidator::clock_cycle_matches_operation_bootstrapping_start_time(int operation_id)
{
    return clock_cycle == get_operation_from_id(operation_id).bootstrap_start_time;
}

bool SolutionValidator::clock_cycle_is_later_than_operation_bootstrapping_start_time(int operation_id)
{
    return clock_cycle > get_operation_from_id(operation_id).bootstrap_start_time;
}

std::pair<bool, int> SolutionValidator::check_if_operation_bootstrapping_core_is_available(int operation_id)
{
    for (auto [bootstrapping_operation_id, _] : bootstrapping_operations)
    {
        if (operations_bootstrap_on_same_core(operation_id, bootstrapping_operation_id))
        {
            return std::pair{false, bootstrapping_operation_id};
        }
    }
    return std::pair{true, -1};
}

bool SolutionValidator::operations_bootstrap_on_same_core(int op_id1, int op_id2)
{
    return get_operation_from_id(op_id1).core_num == get_operation_from_id(op_id2).core_num;
}

void SolutionValidator::print_bootstrapping_core_is_not_available_error(int operation_id, int bootrsapping_operation_id)
{
    std::cout << "Error: Bootstrapping operation " << bootrsapping_operation_id << " was not completed before operation " << operation_id << " started bootstrapping on core " << get_operation_from_id(operation_id).core_num << std::endl;
}

void SolutionValidator::print_missed_bootstrapping_deadline_error(int operation_id)
{
    std::cout << "Error: Bootstrapping operation " << operation_id << " did not start on time" << std::endl;
}

void SolutionValidator::print_missed_start_time_error(int operation_id)
{
    std::cout << "Error: Operation " << operation_id << " did not start on time" << std::endl;
}

std::vector<int> SolutionValidator::get_ready_operations()
{
    std::vector<int> ready_operations;
    for (auto operation_id : unstarted_operations)
    {
        if (operation_is_ready(operation_id))
        {
            ready_operations.push_back(operation_id);
        }
    }
    return ready_operations;
}

bool SolutionValidator::operation_is_ready(int operation_id)
{
    for (auto parent_id : get_operation_from_id(operation_id).parent_ids)
        if (
            vector_contains_element(unstarted_operations, parent_id) ||
            unordered_map_contains_key(running_operations, parent_id) ||
            operation_is_waiting_on_parent_to_bootstrap(operation_id, parent_id))
        {
            return false;
        }
    return true;
}

bool SolutionValidator::operation_is_waiting_on_parent_to_bootstrap(int operation_id, int parent_id)
{
    if (!config.used_some_children_model || parent_sends_bootstrapped_result_to_child(parent_id, operation_id))
    {
        {
            return vector_contains_element(operations_ready_to_bootstrap, parent_id) ||
                   unordered_map_contains_key(bootstrapping_operations, parent_id);
        }
    }
    else
    {
        return false;
    }
}

bool SolutionValidator::parent_sends_bootstrapped_result_to_child(int parent_id, int child_id)
{
    for (auto i = 0; i < config.bootstrapped_operation_ids.size(); i += 2)
    {
        if (config.bootstrapped_operation_ids[i] == parent_id &&
            config.bootstrapped_operation_ids[i + 1] == child_id)
        {
            return true;
        }
    }
    return false;
}

void SolutionValidator::start_running_ready_operations(std::vector<int> ready_operations)
{
    for (auto operation_id : ready_operations)
    {
        if (get_operation_from_id(operation_id).start_time == clock_cycle)
        {
            running_operations[operation_id] = config.operation_type_to_latency_map[get_operation_from_id(operation_id).type];
            remove_element_from_vector(unstarted_operations, operation_id);
        }
        else if (get_operation_from_id(operation_id).start_time < clock_cycle)
        {
            print_missed_start_time_error(operation_id);
            exit(0);
        }
    }
}

Operation SolutionValidator::get_operation_from_id(int id)
{
    if (id < 1 || id > config.operations.size())
    {
        throw std::runtime_error("Invalid operation id");
    }
    return config.operations[id - 1];
}
