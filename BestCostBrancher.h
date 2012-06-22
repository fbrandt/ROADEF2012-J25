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
#ifndef __ROADEF_BESTCOSTBRANCHER_H__
#define __ROADEF_BESTCOSTBRANCHER_H__

#include <gecode/int.hh>

class ProcessChoice : public Gecode::Choice
{
public:
    int process;
    int machine;
    
    ProcessChoice (const Gecode::Brancher& b, int process, int machine);
    ProcessChoice (const Gecode::Brancher& b, Gecode::Archive& e);
    virtual void archive (Gecode::Archive& e) const;
    virtual size_t size() const;
};

/**
 * Gecode brancher that aims at minimizing costs.
 */

class BestCostBrancher : public Gecode::Brancher
{
protected:
    Gecode::ViewArray<Gecode::Int::IntView> process;
    Gecode::ViewArray<Gecode::Int::IntView> cost;
    
public:
    /** Initializing constructor */
    BestCostBrancher (Gecode::Home& home, Gecode::IntVarArray& process, Gecode::IntVarArray& cost);
    /** Copy constructor for Gecode search */
    BestCostBrancher (Gecode::Space& space, bool share, BestCostBrancher& b);
    
    /** Brancher copy method for Gecode search */
    virtual BestCostBrancher* copy (Gecode::Space& space, bool share);
    
    /** Static entry point for creating the brancher */
    static void post (Gecode::Home home, Gecode::IntVarArray& process, Gecode::IntVarArray& cost);
    /** Return if there is some work to do for this brancher */
    virtual bool status (const Gecode::Space& space) const;
    /** Select a process/machine for branching */
    virtual Gecode::Choice* choice (Gecode::Space& space);
    /** Reload a choice from the archive */
    virtual Gecode::Choice* choice (const Gecode::Space& space, Gecode::Archive& e);
    /** Apply a choice to the given search space */
    virtual Gecode::ExecStatus commit (Gecode::Space& space, const Gecode::Choice& c, unsigned int a);
    /** Brancher destruction for Gecode search */
    virtual size_t dispose (Gecode::Space& space);
};

#endif /* __ROADEF_BESTCOSTBRANCHER_H__ */
