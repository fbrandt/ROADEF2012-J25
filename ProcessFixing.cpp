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

#include <vector>
#include <list>
#include <algorithm>
#include "ProcessFixing.h"

ProcessFixing::ProcessFixing(Instance& instance) : instance(instance) {}

void ProcessFixing::reset() {
    for (int p = 0; p < instance.num_processes; p++)
        instance.process[p].fixed = false;
    instance.movable_processes_by_size = std::vector<int>(instance.processes_by_size);
    instance.num_movable_processes = instance.num_processes;
}

void ProcessFixing::updateMovableProcesses() {
    int count = 0;
    instance.movable_processes_by_size = std::vector<int>(instance.num_processes);
    for (int i = 0; i < instance.num_processes; i++) {
        int p = instance.processes_by_size[i];
        if (!instance.process[p].fixed)
            instance.movable_processes_by_size[count++] = p;
    }
    instance.movable_processes_by_size.resize(count);
    instance.num_movable_processes = count;
    #ifdef LOGGING
    std::cerr << "Fixed processes: " << instance.num_processes - count << "/" << instance.num_processes << std::endl;    
    #endif
}

void ProcessFixing::fixTransient(float safety) {
    reset();
    
    if (!instance.hasTransientResources())
        return;
    
    std::vector<unsigned int> capacity(instance.num_resources); 
    std::vector<unsigned int> safety_capacity(instance.num_resources); 
    std::vector<unsigned int> used(instance.num_resources);
    std::vector<unsigned int> buffer(instance.num_resources);
    
    for (int r = 0; r < instance.num_resources; r++) {
        for (int m = 0; m < instance.num_machines; m++) {
            capacity[r] += instance.machine[m].capacity[r];
            safety_capacity[r] += instance.machine[m].safety_capacity[r];
        }
        for (int p = 0; p < instance.num_processes; p++)
            used[r] += instance.process[p].requirement[r];
        buffer[r] = capacity[r] - used[r];
    }
    
    // usage relative to available resource buffer of transient resources
    std::vector<ProcessCost> trans_usage(instance.num_processes);
    
    for (int p = 0; p < instance.num_processes; p++) {
        trans_usage[p].index = p;
        for (int r = 0; r < instance.num_resources; r++) {
            if (instance.resource[r].is_transient) {
                trans_usage[p].cost += 1.0 * instance.process[p].requirement[r] / buffer[r] * 1e8;
            }
        }
    }
    sort(trans_usage.begin(), trans_usage.end());
    
    // resources used by fixed processes
    InstanceLoad fixed_usage(instance.num_machines, MachineLoad(instance.num_resources));
    std::vector<unsigned int> fixed_resource_usage(instance.num_resources);
    
    int num_fixed = 0;
    
    for (int i = 0; i < instance.num_processes; i++) {
        int p = trans_usage[i].index;
        int m = instance.process[p].original_machine;
        
        bool has_space = true;
        for (int r = 0; r < instance.num_resources; r++) {
            if (fixed_usage[m][r] + instance.process[p].requirement[r] > safety * instance.machine[m].safety_capacity[r])
                has_space = false;
        }
        
        if (has_space) {
            instance.process[p].fixed = true;
            num_fixed++;
            for (int r = 0; r < instance.num_resources; r++) {
                fixed_usage[m][r] += instance.process[p].requirement[r];
                fixed_resource_usage[r] += instance.process[p].requirement[r];
            }
        }
    }
    
    #ifdef LOGGING
    for (int r = 0; r < instance.num_resources; r++) {
        std::cerr << "Resource " << r << ": " << 1.0 * fixed_resource_usage[r] / safety_capacity[r] << " "
        << 1.0 * fixed_resource_usage[r] / capacity[r] << (instance.resource[r].is_transient ? " T" : "") << std::endl;
    }
    #endif
    
    updateMovableProcesses();
    
}
