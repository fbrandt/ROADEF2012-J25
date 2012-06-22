/*
 * Authors: 
 *   Felix Brandt <brandt@fzi.de>, 
 *   Jochen Speck <speck@kit.edu>, 
 *   Markus Voelker <markus.voelker@kit.edu>
 *
 * Copyright (c) 2012 Felix Brandt, Jochen Speck, Markus Voelker
 *
 * Permission is hereby granted, free of charge, to any person obtaining 
 * a copy of this software and associated documentation files (the 
 * "Software"), to deal in the Software without restriction, including 
 * without limitation the rights to use, copy, modify, merge, publish, 
 * distribute, sublicense, and/or sell copies of the Software, and to 
 * permit persons to whom the Software is furnished to do so, subject to 
 * the following conditions:
 *
 * The above copyright notice and this permission notice shall be included 
 * in all copies or substantial portions of the Software.
 *
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS 
 * OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF 
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. 
 * IN NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY 
 * CLAIM, DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, 
 * TORT OR OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE 
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */

#include <algorithm>
#include <map>

#include "Instance.h"

using namespace std;

Resource::Resource () { }

Resource::Resource (istream& in) : total_load(0)
{
    in >> is_transient >> weight_load_cost;
}

Machine::Machine () { }

Machine::Machine (istream& in, int resources, int machines) :
initial_usage(resources)
{
    in >> neighborhood >> location;
    Instance::read(in, resources, capacity);
    Instance::read(in, resources, safety_capacity);
    Instance::read(in, machines, move_cost);
    
    max_move_cost = 0;
    for (std::vector<unsigned int>::const_iterator iter = move_cost.begin(); iter != move_cost.end(); ++iter) {
        if (*iter > max_move_cost) max_move_cost = *iter;
    }
}

Service::Service () { }

Service::Service (istream& in)
{
    int dependencies = 0;
    
    in >> min_spread >> dependencies;
    Instance::read(in, dependencies, depends_on);
}

Process::Process () { }

Process::Process (istream& in, unsigned int resources) :
original_machine(-1)
{
    in >> service;
    Instance::read(in, resources, requirement);
    in >> move_cost;
    fixed = false;
}

Balance::Balance () { }

Balance::Balance (istream& in)
{
    in >> resource1 >> resource2 >> balance >> weight_balance_cost;
}

Instance::Instance (istream& in)
{
    this->read(in, resource);
    
    transient_count = resource.size();
    
    int machines;
    in >> machines;
    
    machine.resize(machines);
    for (int i = 0; i < machines; ++i)
    {
        new (&(machine[i])) Machine(in, resource.size(), machines);
    }
    
    for (int i = 0; i < machines; ++i)
    {
        if (neighborhood.size() <= machine[i].neighborhood)
        {
            neighborhood.resize(machine[i].neighborhood + 1);
        }
        neighborhood[machine[i].neighborhood].push_back(i);
        
        if (location.size() <= machine[i].location)
        {
            location.resize(machine[i].location + 1);
        }
        location[machine[i].location].push_back(i);
    }
    
    this->read(in, service);
    this->read(in, process, resource.size());
    
    for (unsigned int p = 0; p < process.size(); ++p) {
        service[process[p].service].process.push_back(p);
        
        for (unsigned int r = 0; r < resource.size(); ++r) {
            resource[r].total_load += process[p].requirement[r];
        }
    }
    
    this->read(in, balance);
    
    in >> weight_process_move_cost;
    in >> weight_service_move_cost;
    in >> weight_machine_move_cost;
    
    this->initializeServiceDependencies(service);
    this->initializeBalanceData(balance);
    
    num_processes = (int)process.size();
    num_machines = (int)machine.size();
    num_resources = (int)resource.size();
    
    // sort processes by increasing resource demand
    processes_by_size.resize(num_processes);
    multimap<unsigned int, int> process_map;
    for (int p = 0; p < num_processes; p++) {
        unsigned int demand = 0;
        for (int r = 0; r < num_resources; r++)
            demand += process[p].requirement[r];
        process_map.insert(pair<unsigned int, int>(demand, p));
    }
    int i = 0; 
    for (multimap<unsigned int, int>::iterator it = process_map.begin(); it != process_map.end(); it++)
        processes_by_size[i++] = it->second;
    
    // sort machines by increasing safety capacity
    machines_by_size.resize(num_machines);
    multimap<unsigned int, int> machine_map;
    for (int m = 0; m < num_machines; m++) {
        unsigned int safety = 0;
        for (int r = 0; r < num_resources; r++)
            safety += machine[m].safety_capacity[r];
        machine_map.insert(pair<unsigned int, int>(safety, m));
    }
    i = 0; 
    for (multimap<unsigned int, int>::iterator it = machine_map.begin(); it != machine_map.end(); it++)
        machines_by_size[i] = it->second;
    
    num_movable_processes = num_processes;
    movable_processes_by_size = std::vector<int>(processes_by_size);
}

Instance::~Instance ()
{ }

bool Instance::hasTransientResources() {
    for (int r = 0; r < num_resources; r++)
        if (resource[r].is_transient)
            return true;
    return false;
}

void Instance::initializeServiceDependencies (std::vector<Service>& service) const
{
    for (unsigned int s = 0; s < service.size(); ++s)
    {
        for (unsigned int d = 0; d < service[s].depends_on.size(); ++d)
        {
            service[service[s].depends_on[d]].required_by.push_back(s);
        }
    }
    
}

void Instance::reorderResources ()
{
    std::vector<unsigned int> resource_map;
    
    for (unsigned int i = 0; i < resource.size(); ++i) {
        if (resource[i].is_transient) {
            resource_map.push_back(i);
        }
    }
    
    transient_count = resource_map.size();
    
    // stop if no transient resources are present
    if (transient_count == 0) {
        return;
    }
    
    for (unsigned int i = 0; i < resource.size(); ++i) {
        if (!resource[i].is_transient) {
            resource_map.push_back(i);
        }
    }
    
    std::vector<Resource> resource_copy = resource;
    for (unsigned int r = 0; r < resource.size(); ++r) {
        resource[r] = resource_copy[resource_map[r]];
    }
    
    for (unsigned int m = 0; m < machine.size(); ++m) {
        MachineLoad capacity_copy = machine[m].capacity;
        MachineLoad safety_capacity_copy = machine[m].safety_capacity;
        
        for (unsigned int r = 0; r < machine[m].capacity.size(); ++r) {
            machine[m].capacity[r] = capacity_copy[resource_map[r]];
            machine[m].safety_capacity[r] = safety_capacity_copy[resource_map[r]];
        }
    }
    
    for (unsigned int p = 0; p < process.size(); ++p) {
        MachineLoad requirement_copy = process[p].requirement;
        
        for (unsigned int r = 0; r < process[p].requirement.size(); ++r) {
            process[p].requirement[r] = requirement_copy[resource_map[r]];
        }
    }
    
    for (unsigned int b = 0; b < balance.size(); ++b) {
        balance[b].resource1 = std::find(resource_map.begin(), resource_map.end(), balance[b].resource1) - resource_map.begin();
        balance[b].resource2 = std::find(resource_map.begin(), resource_map.end(), balance[b].resource2) - resource_map.begin();
    }
}

void Instance::initializeBalanceData (std::vector<Balance>& _balance) const
{
    for (std::vector<Balance>::iterator balance = _balance.begin(); balance != _balance.end(); ++balance) {
        int r1 = balance->resource1;
        int r2 = balance->resource2;
        
        long long cap1 = 0;
        long long cap2 = 0;
        
        for (std::vector<Machine>::const_iterator m = machine.begin(); m != machine.end(); ++m) {
            cap1 += m->capacity[r1];
            cap2 += m->capacity[r2];
        }
        
        long long load1 = 0;
        long long load2 = 0;
        
        for (std::vector<Process>::const_iterator p = process.begin(); p != process.end(); ++p) {
            load1 += p->requirement[r1];
            load2 += p->requirement[r2];
        }
        
        balance->min_balance_units = balance->balance * (cap1 - load1) - (cap2 - load2);
        
        #ifdef LOGGING
        std::cerr << "Balance lower bound: " << balance->min_balance_units << std::endl;
        #endif
    }
}

void Instance::setAssignment (const Assignment& assignment, ReAssignment* state)
{
    this->assignment = assignment;
    
    state->instance = this;
    state->assignment = assignment;
    state->excess.resize(this->machine.size(), MachineLoad(this->resource.size()));
    state->transient.resize(this->machine.size(), MachineLoad(this->transient_count));
    state->balance.resize(this->machine.size(), MachineBalance(this->balance.size()));
    
    state->load_cost = 0;
    state->balance_cost = 0;
    state->process_moves = 0;
    state->machine_moves = 0;
    
    for (unsigned int p = 0; p < this->assignment.size(); ++p) {
        unsigned int machine = this->assignment[p];
        process[p].original_machine = machine;
        
        for (unsigned int r = 0; r < this->resource.size(); ++r) {
            state->excess[machine][r] += process[p].requirement[r];
            if (r < transient_count) {
                state->transient[machine][r] += process[p].requirement[r];
            }
        }
    }
    
    for (std::vector<Service>::iterator s = this->service.begin(); s != this->service.end(); ++s) {
        std::map<int, int> count;
        
        for (std::vector<unsigned int>::const_iterator p = s->process.begin(); p != s->process.end(); ++p) {
            count[this->machine[this->assignment[*p]].location]++;
        }
        
        s->cur_spread = count.size();
    }
    
    std::vector<long long> load_units(this->resource.size());
    std::vector<long long> balance_units(this->balance.size());
    
    for (unsigned int m = 0; m < this->machine.size(); ++m) {
        const Machine& machine = this->machine[m];
        MachineLoad& excess = state->excess[m];
        
        for (unsigned int r = 0; r < load_units.size(); ++r) {
            excess[r] -= machine.safety_capacity[r];
            load_units[r] += std::max(0, excess[r]);
        }
        
        for (unsigned int b = 0; b < balance_units.size(); ++b) {
            const Balance& bal = this->balance[b];
            state->balance[m][b] = bal.balance *
            (machine.capacity[bal.resource1] - machine.safety_capacity[bal.resource1] - excess[bal.resource1]) - 
            (machine.capacity[bal.resource2] - machine.safety_capacity[bal.resource2] - excess[bal.resource2]);
            balance_units[b] += std::max(0, state->balance[m][b]);
        }
    }
    
    for (unsigned int r = 0; r < load_units.size(); ++r) {
        state->load_cost += load_units[r] * this->resource[r].weight_load_cost;
    }
    
    for (unsigned int b = 0; b < balance_units.size(); ++b) {
        state->balance_cost += balance_units[b] * this->balance[b].weight_balance_cost;
    }
    
    #ifdef LOGGING
    std::cerr << "Initial cost: " << state->load_cost << " " << state->balance_cost << " " << state->load_cost + state->balance_cost << std::endl;
    #endif
}
