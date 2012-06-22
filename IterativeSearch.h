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
#ifndef __ROADEF_ITERATIVESEARCH_H__
#define __ROADEF_ITERATIVESEARCH_H__

#include "BaseSearch.h"

/**
 * Base class for iterative search strategies.
 */
class IterativeSearch : public BaseSearch
{
protected:
    bool abort_on_nonimproving;
    int identifier;
    
public:
    /**
     * Setup a local iterative search process
     */
    IterativeSearch(int identifier, time_t start_time, bool abort_on_nonimproving = true);
    virtual ~IterativeSearch ();
    
    virtual ReAssignment* run(const ReAssignment* best_known, time_t time_limit);
    virtual ReAssignment* runOnce(const ReAssignment* current_state) = 0;
    
    void process_cost(const ReAssignment& state, std::vector<ProcessCost>& cost);
};

#endif /* __ROADEF_ITERATIVESEARCH_H__ */
