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

#pragma once
#ifndef __ROADEF_INSTANCE_H__
#define __ROADEF_INSTANCE_H__

#include <iostream>
#include <vector>

/** Global logging output can be activated here **/
// #define LOGGING

/** Mapping from process id to hosting machine */
typedef std::vector<unsigned int> Assignment;
/** List of process ids */
typedef std::vector<unsigned int> ProcessList;

typedef std::vector<int> MachineLoad;
typedef std::vector<MachineLoad> InstanceLoad;

typedef std::vector<int> MachineBalance;
typedef std::vector<MachineBalance> InstanceBalance;

struct Resource
{
    bool is_transient;
    int weight_load_cost;
    int total_load;
    
    Resource ();
    Resource (std::istream& in);
};

struct Machine
{
    unsigned int neighborhood;
    unsigned int location;
    MachineLoad capacity;         //! Capacity per resource
    MachineLoad initial_usage;    //! Utilization per resource in initial assignment
    MachineLoad safety_capacity;  //! Safety capacity per resource
    std::vector<unsigned int> move_cost;   //! Cost when moving a process from this to another machine
    unsigned int max_move_cost;
    
    Machine ();
    Machine (std::istream& in, int resources, int machines);
};

typedef ProcessList ServiceList;

struct Service
{
    unsigned int min_spread;
    unsigned int cur_spread;
    ServiceList depends_on;
    ServiceList required_by;
    ProcessList process;
    
    Service ();
    Service (std::istream& in);
};

struct Process
{
    unsigned int service;
    MachineLoad requirement;
    unsigned int move_cost;
    /** Initially assigned machine (-1 unknown) */
    int original_machine;
    bool fixed;
    
    Process ();
    Process (std::istream& in, unsigned int resources);
};


struct ProcessCost {
    int index;
    long long cost;
    
    ProcessCost (int _index = 0, long long _cost = 0) : index(_index), cost(_cost) { }
    bool operator< (const ProcessCost& o) const
    {
        return o.cost < cost;
    }
};

struct Balance
{
    unsigned int resource1;
    unsigned int resource2;
    unsigned int balance;
    unsigned int weight_balance_cost;
    long long min_balance_units;
    
    Balance ();
    Balance (std::istream& in);
};

class ReAssignment;

/**
 * Parsing and management of the given problem instance.
 */
class Instance
{
public:
    std::vector<Resource> resource;
    std::vector<Machine> machine;
    std::vector<ProcessList> neighborhood;
    std::vector<ProcessList> location;
    std::vector<Service> service;
    std::vector<Process> process;
    std::vector<Balance> balance;
    Assignment assignment;
    
    // process ids in order of increasing resource demand
    std::vector<int> processes_by_size;
    // process ids of movable processes in order of increasing resource demand
    std::vector<int> movable_processes_by_size;
    // machine ids in order of increasing safety capacities
    std::vector<int> machines_by_size;
    
    int num_processes;
    int num_movable_processes;
    int num_machines;
    int num_resources;
    
    unsigned int transient_count;
    
    int weight_process_move_cost;
    int weight_service_move_cost;
    int weight_machine_move_cost;
    
    Instance (std::istream& in);
    virtual ~Instance();
    
    bool hasTransientResources();
    
    /** Set initial assignment and return state (assignment + machine usage) */
    virtual void setAssignment (const Assignment&, ReAssignment*);
    
    /** Calculate the @c required_by entries by the @c depends_on vectors. */
    virtual void initializeServiceDependencies (std::vector<Service>& service) const;
    virtual void initializeBalanceData (std::vector<Balance>& balance) const;
    virtual void reorderResources ();
    
    template<typename T> static void read (std::istream& in, std::vector<T>& data)
    {
        int n;
        in >> n;
        data.resize(n);
        
        for (int i = 0; i < n; ++i) {
            new (&(data[i])) T(in);
        }
    }
    
    template<typename T> static void read (std::istream& in, std::vector<T>& data, int param1)
    {
        int n;
        in >> n;
        data.resize(n);
        
        for (int i = 0; i < n; ++i) {
            new (&(data[i])) T(in, param1);
        }
    }
    
    template<typename T> static void read(std::istream& in, int n, std::vector<T>& data)
    {
        data.resize(n);
        
        for (int i = 0; i < n; ++i) {
            in >> data[i];
        }
    }
};

#include "ReAssignment.h"

#endif /* __ROADEF_INSTANCE_H__ */
