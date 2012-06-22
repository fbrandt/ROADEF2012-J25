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
#ifndef __ROADEF_SCHEDULEPLOTTER_H__
#define __ROADEF_SCHEDULEPLOTTER_H__

#include "Instance.h"
#include <stdio.h>

struct ResourceStats {
    int load;
    int load2;
    int capacity;
    int safety_capacity;
};

typedef std::vector<ResourceStats> MachineStats;
typedef std::vector<MachineStats> InstanceStats;

/**
 * Create HTML overview of an instance/solution.
 */
class SchedulePlotter
{
public:
    /** Send HTML representation of this instance/assignment to the given stream */
    static void plot (std::ostream& out, const Instance& instance, const Assignment& initial, const Assignment& current);
    static int percent (int nom, int den);
    
    /** Transform load/capacity ratio into an HTML color code */
    static std::string color (int load, int safety_capacity, int capacity);
    /** Create usage table for a given machine */
    static void table (std::ostream& out, MachineStats& load, MachineStats& tload, const Instance& instance, int machine);
    /** Create usage chart for a given machine */
    static void chart (std::ostream& out, MachineStats& load, MachineStats& tload, const Instance& instance, int machine, const Assignment& state, const Assignment& current);
    /** Create usage overview over all machines&resources */
    static void getLoadStats (const Instance& instance, const Assignment& state, const Assignment& current, InstanceStats& stats, InstanceStats& tload);
};

#endif /* __ROADEF_SCHEDULEPLOTTER_H__ */
