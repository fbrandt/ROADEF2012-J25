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
#ifndef __ROADEF_COSTPROPAGATOR_H__
#define __ROADEF_COSTPROPAGATOR_H__

#include <gecode/int.hh>
#include "Instance.h"
#include "RescheduleSpace.h"

/**
 * Adjust cost and balance variables and space delta.
 */
class CostPropagator : public Gecode::Propagator
{
protected:
    /** Index of variables associated with considered process */
    unsigned int m_index;
    /** Id of the considered process */
    unsigned int m_process_id;
    /** Machine the process is assigned to */
    Gecode::Int::IntView m_process;
    /** Excess load for this single process */
    Gecode::Int::IntView m_cost;
    /** Index of last cached machine from the space's modified_machines */
    int cache_stage;
    /** Last checked cost bound */
    std::pair<int, int> m_lastbound;
    
    std::pair<int, int> initCache (RescheduleSpace& space, ProcessList& blacklist);
    std::pair<int, int> updateCache (RescheduleSpace& space, ProcessList& blacklist, int cache_pos, bool need_sweep = false);
    
    std::pair<int, int> getAdditionalCost(const RescheduleSpace& space, const Process& process, unsigned int machine_id);
    int getExcessCost (const RescheduleSpace& space, const Process& process, unsigned int machine_id, const Machine& machine, const MachineLoad& load, const MachineLoad& transient, const MachinePatch* patch = NULL);
    std::pair<int, int> getBalanceCost (const RescheduleSpace& space, const Process& process, const Machine& machine, const MachineBalance& balance, const MachinePatch* patch = NULL);
    
public:
    /** Initializing constructor */
    CostPropagator (Gecode::Home home, unsigned int index, unsigned int process_id, Gecode::IntVar& process, Gecode::IntVar& cost);
    /** Copy constructor for Gecode search */
    CostPropagator (Gecode::Home home, bool share, CostPropagator& p);
    
    /** Propagator copying for Gecode search */
    virtual CostPropagator* copy (Gecode::Space& home, bool share);
    /** Propagator destruction for Gecode search */
    virtual size_t dispose (Gecode::Space& home);
    /** Just return constant execution cost */
    virtual Gecode::PropCost cost (const Gecode::Space& home, const Gecode::ModEventDelta& delta) const;
    /** Propagate */
    virtual Gecode::ExecStatus propagate (Gecode::Space& home, const Gecode::ModEventDelta& delta);
    /** Setup method */
    static Gecode::ExecStatus post (Gecode::Home home, unsigned int index, unsigned int process);
};


#endif /* __ROADEF_COSTPROPAGATOR_H__ */
