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
#ifndef __ROADEF_RESCHEDULESPACE_H__
#define __ROADEF_RESCHEDULESPACE_H__

#include <map>
#include <vector>

#include <gecode/int.hh>

#include "Instance.h"

template<typename _Ty> class gVector : public std::vector<_Ty, Gecode::space_allocator<_Ty> >
{
public:
    typedef Gecode::space_allocator<_Ty> _Alloc;
    
    gVector (unsigned int _Count, const _Ty& _Val, const _Alloc& _Al) :
    std::vector<_Ty, _Alloc>(_Count, _Val, _Al)
    { }
    
    template<class _Iter> gVector(_Iter _First, _Iter _Last, const _Alloc& _Al) :
    std::vector<_Ty, _Alloc>(_First, _Last, _Al)
    { }
};

template<typename _Kty, typename _Ty, typename _Pr = std::less<_Kty> >
class gMap : public std::map<_Kty, _Ty, _Pr, Gecode::space_allocator<std::pair<_Kty, _Ty> > >
{
public:
    typedef std::map<_Kty, _Ty, _Pr, Gecode::space_allocator<std::pair<_Kty, _Ty> > > base_map_type;
    typedef Gecode::space_allocator<std::pair<_Kty, _Ty> > allocator_type;
    
    /** Initializing default constructor */
    gMap(const allocator_type& _Al) :
    base_map_type (typename base_map_type::key_compare(), _Al)
    { }
    
    /** Copy constructor */
    template<class _Iter>
    gMap(_Iter _First, _Iter _Last, const allocator_type& _Al) :
    base_map_type (_First, _Last, typename base_map_type::key_compare(), _Al)
    { }
};

struct BoundMachine {
    unsigned int machine;
    long long cost;
    
    BoundMachine (unsigned int _machine, int _cost) : machine(_machine), cost(_cost) { }
};

struct CostBound {
    BoundMachine min;
    BoundMachine max;
    
    CostBound () :
    min(0, Gecode::Int::Limits::max),
    max(0, Gecode::Int::Limits::min)
    { }
};

class ProcessCostMap
{
public:
    typedef gMap<unsigned int, std::pair<int, int> > CostMap;
    
protected:
    unsigned int size;
    CostBound* cost_bound;
    CostMap* cost_map;
    
public:
    /** Initializing constructor */
    ProcessCostMap (unsigned int size, Gecode::Space& space);
    /** Copy constructor */
    ProcessCostMap(const ProcessCostMap& p, Gecode::Space& space);
    
    /** Set expected cost of assignment */
    void setCost (unsigned int process, unsigned int machine, CostMap::mapped_type value);
    /** Get expected cost of assignment */
    CostMap::mapped_type getCost (unsigned int process, unsigned int machine) const;
    
    void setBound (unsigned int process, CostBound& bound);
    CostBound& bound (unsigned int process);
    void remove (unsigned int process, unsigned int machine);
};

class MachinePatch {
    
public:
    typedef gVector<int>::allocator_type allocator_type;
    
    gVector<int> excess;
    gVector<int> transient;
    gVector<int> balance;
    
    MachinePatch (const Instance& instance, const gVector<int>::allocator_type& _alloc) :
    excess(instance.num_resources, 0, _alloc),
    transient(instance.transient_count, 0, _alloc),
    balance((unsigned int)instance.balance.size(), 0, _alloc)
    { }
    
    MachinePatch (const MachinePatch& o, const gVector<int>::allocator_type& _alloc) :
    excess(o.excess.begin(), o.excess.end(), _alloc),
    transient(o.transient.begin(), o.transient.end(), _alloc),
    balance(o.balance.begin(), o.balance.end(), _alloc)
    { }
    
};

/**
 * The base std::map can not handle an allocator without a default constructor.
 * Therefore we create a new constructor that fits our needs.
 */
class PatchMap : public std::map<unsigned int, MachinePatch, std::less<unsigned int>, Gecode::space_allocator<std::pair<const unsigned int, MachinePatch> > >
{
public:
    typedef Gecode::space_allocator<std::pair<const unsigned int, MachinePatch> > allocator_type;
    
    /** Initializing default constructor */
    PatchMap(const allocator_type& _Al) :
    std::map<unsigned int, MachinePatch, std::less<unsigned int>, allocator_type>(key_compare(), _Al)
    { }
    
    /** Copy constructor */
    PatchMap (const PatchMap& o, const allocator_type& _Al) :
    std::map<unsigned int, MachinePatch, std::less<unsigned int>, allocator_type>(key_compare(), _Al)
    {
        for (PatchMap::const_iterator iter = o.begin(); iter != o.end(); ++iter) {
            insert(value_type(iter->first, MachinePatch(iter->second, MachinePatch::allocator_type(_Al.space))));
        }
    }
};

/**
 * Gecode search space for a fixed neighborhood exploration
 */
class RescheduleSpace : public Gecode::Space
{
public:
    /** assigned machine per moved process */
    Gecode::IntVarArray process;
    /** aggregated process move cost */
    Gecode::IntVarArray process_move_cost;
    
    /** Total cost outside the CP scope (~offset) */
    long long base_total_cost;
    /** Total cost inside the CP scope */
    Gecode::IntVar total_cost;
    
    /** General instance data containing the initial assignment */
    const Instance& instance;
    /** Current assignment state at the beginning of this iteration */
    const ReAssignment& state;
    /** Processes considered as neighborhood, i.e., they can move */
    const ProcessList& moved;
    
    /** Replacements for updated load and balance entries */
    PatchMap& delta;
    
    /** Log of modified machines, with this log we know which process cost need to be updated */
    gVector<int> modified_machines;
    /** Cache of expected cost when assigning a process to a machine */
    ProcessCostMap cost_cache;
    
    /** Minimum unassigned balance to approximate minimum balance cost when scheduling a process */
    gVector<int> min_unassigned_balance;
    /** Maximum unassigned balance to approximate maximum balance cost when scheduling a process */
    gVector<int> max_unassigned_balance;
    
public:
    
    /** Initializing contructor setting up the model */
    RescheduleSpace (const Instance& instance, const ReAssignment& state, const ProcessList& movables);
    /** Copy constructor for Gecode search */
    RescheduleSpace (bool share, RescheduleSpace& s);
    /** Space copier, for Gecode search */
    virtual Gecode::Space* copy (bool share);
    
    /** Constrain the space when a best solution is found */
    virtual void constrain (const Gecode::Space& best);
    /** Assemble result state from CP solution */
    virtual ReAssignment* getResultState () const;
    /** Serialize space state */
    void print (std::ostream& out) const;
protected:
    
    /** Prepare load data and setup capacity constraint/load costs and balance cost */
    void setupLoadConstraints ();
    /** Add the capacity constraint (see Section 1.2.1) and transient usage constraint (see Section 1.2.5.) to the model  */
    void setupLoadCost ();
    /** Add calculation of balance costs */
    void setupBalanceCost ();
    /** Add the conflict constraint to the model (see Section 1.2.2) */
    void setupConflictConstraint ();
    /** Add the spread constraint to the model (see Section 1.2.3.) */
    void setupSpreadConstraint ();
    /** Add the dependency constraint to the model (see Section 1.2.4.) */
    void setupDependencyConstraint ();
    
    /** Setup the calculation of the objective function value */
    void setupObjectiveFunction ();
    
};

#endif /* __ROADEF_RESCHEDULESPACE_H__ */
