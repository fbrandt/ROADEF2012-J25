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

#include "SchedulePlotter.h"
#include <stdio.h>

using namespace std;

string SchedulePlotter::color (int load, int sc, int capa)
{
    char s[16];
    
    if (load < sc)
    {
        int value = (int)((long long)load * 255 / sc);
        sprintf(s, "#%02xFF00", value);
    }
    else
    {
        int value = (int)((long long)255 - (((long long)load - sc) * 255 / (capa - sc)));
        sprintf(s, "#FF%02x00", value);
    }
    
    return string(s);
}

int SchedulePlotter::percent (int nom, int den)
{
    return (int)((long long)(nom) * 100 / den);
}

void SchedulePlotter::table (ostream& out, MachineStats& load, MachineStats& tload, const Instance& instance, int machine)
{
    int load_cost = 0;
    int balance_cost = 0;
    
    for (unsigned int r = 0; r < load.size(); ++r)
    {
        load_cost += std::max(0, load[r].load - load[r].safety_capacity) * instance.resource[r].weight_load_cost;
    }
    
    for (unsigned int b = 0; b < instance.balance.size(); ++b)
    {
        int r1 = instance.balance[b].resource1;
        int r2 = instance.balance[b].resource2;
        
        int a1 = load[r1].capacity - load[r1].load;
        int a2 = load[r2].capacity - load[r2].load;
        
        balance_cost += std::max((int)(0), (int)(instance.balance[b].balance * a1 - a2));
    }
    
    out << "<tr><td style=\"text-align: left;\"><a href=\"#mach" << machine << "\">Machine " << machine << "</a></td>" << endl <<
    "<td>" << load_cost << "</td>" <<
    "<td>" << balance_cost << "</td>";
    
    for (unsigned int r = 0; r < load.size(); ++r)
    {
        string s = SchedulePlotter::color(load[r].load, load[r].safety_capacity, load[r].capacity);
        out << "<td style=\"background: " << s.c_str() <<
        ";\">" << load[r].load << "(" << percent(load[r].load, load[r].capacity) << "%";
        if (instance.resource[r].is_transient) {
            out << " | " << percent(tload[r].load, load[r].capacity) << "%";
        }
        out << ")</td>" << endl;
    }
    
    out << "</tr>";
}

void SchedulePlotter::chart (ostream& out, MachineStats& load, MachineStats& tload, const Instance& instance, int m, const Assignment& state, const Assignment& current)
{
    int load_cost = 0;
    int balance_cost = 0;
    
    out << "<table class=\"chart\"><thead><tr><th></th>" << endl;
    for (unsigned int r = 0; r < load.size(); ++r)
    {
        out << "<th>" << r+1 << "</th>" << endl;
        load_cost += std::max(0, load[r].load - load[r].safety_capacity) * instance.resource[r].weight_load_cost;
    }
    
    for (unsigned int b = 0; b < instance.balance.size(); ++b)
    {
        int r1 = instance.balance[b].resource1;
        int r2 = instance.balance[b].resource2;
        
        int a1 = load[r1].capacity - load[r1].load;
        int a2 = load[r2].capacity - load[r2].load;
        
        balance_cost += std::max((int)(0), (int)(instance.balance[b].balance * a1 - a2));
    }
    
    out << "</tr></thead><tbody><tr><td><h1>L " << load_cost << "</h1><h1>B " << balance_cost << "</h1></td>";
    
    for (unsigned int r = 0; r < load.size(); ++r)
    {
        out << "<td><div class=\"chart\"><div class=\"chart_container\">" << endl <<
        "<div class=\"chart_value\" style=\"height: " << (int)((long long)load[r].load * 100 / load[r].capacity) << "px;\"></div>" << endl <<
        "<div class=\"chart_value2\" style=\"height: " << (int)((long long)load[r].load2 * 100 / load[r].capacity) << "px;\"></div>" << endl <<
        "<div class=\"chart_value3\" style=\"height: " << (int)((long long)tload[r].load * 100 / load[r].capacity) << "px;\"></div>" << endl <<
        "<div class=\"chart_line\" style=\"bottom: " << (int)((long long)load[r].safety_capacity * 100 / load[r].capacity) << "px;\"></div>" << endl <<
        "</div></div></td>" << endl;
    }
    
    out << "</tr><tr><th>Load (before)</th>" << endl;
    for (unsigned int r = 0; r < load.size(); ++r)
    {
        out << "<td>" << load[r].load << "</td>" << endl;
    }
    
    out << "</tr><tr><th>Load (after)</th>" << endl;
    for (unsigned int r = 0; r < load.size(); ++r)
    {
        out << "<td>" << load[r].load2 << "</td>" << endl;
    }
    
    out << "</tr><tr><th>Safety Capacity</th>" << endl;
    for (unsigned int r = 0; r < load.size(); ++r)
    {
        out << "<td>" << load[r].safety_capacity << "</td>" << endl;
    }
    
    out << "</tr><tr><th>Capacity</th>" << endl;
    for (unsigned int r = 0; r < load.size(); ++r)
    {
        out << "<td>" << load[r].capacity << "</td>" << endl;
    }
    
    out << "<tr><th colspan=\"" << instance.num_resources + 1 << "\"><hr /></th></tr>" << endl;
    
    int j = 1;
    for (unsigned int p = 0; p < state.size(); ++p)
    {
        if (current[p] == m)
        {
            bool unmoved = state[p] == m;
            out << "<tr" << (unmoved ? " class=\"grey\"" : "") << "><th>" << j++ << ". Process " << p << "</th>" << endl;
            for (unsigned int r = 0; r < instance.process[p].requirement.size(); ++r)
            {
                out << "<td>" << instance.process[p].requirement[r] << "</td>" << endl;
            }
            out << "</tr>";
        }
    }
    
    out << "</tr></tbody></table>" << endl;
}

void SchedulePlotter::getLoadStats (const Instance& instance, const Assignment& state, const Assignment& current, InstanceStats& stats, InstanceStats& tload)
{
    stats.clear();
    stats.resize(instance.num_machines, MachineStats(instance.num_resources));
    tload.clear();
    tload.resize(instance.machine.size(), MachineStats(instance.resource.size()));
    
    for (unsigned int p = 0; p < instance.num_processes; ++p)
    {
        for (unsigned int r = 0; r < instance.num_resources; ++r)
        {
            stats[state[p]][r].load += instance.process[p].requirement[r];
            stats[current[p]][r].load2 += instance.process[p].requirement[r];
            
            if (instance.resource[r].is_transient) {
                tload[state[p]][r].load += instance.process[p].requirement[r];
                if (state[p] != current[p]) {
                    tload[current[p]][r].load += instance.process[p].requirement[r];
                }
            }
        }
    }
    
    for (unsigned int m = 0; m < instance.num_machines; ++m)
    {
        for (unsigned int r = 0; r < instance.num_resources; ++r)
        {
            stats[m][r].safety_capacity = instance.machine[m].safety_capacity[r];
            stats[m][r].capacity = instance.machine[m].capacity[r];
        }
    }
}

void SchedulePlotter::plot (ostream& out, const Instance& instance, const Assignment& state, const Assignment& current)
{
    InstanceStats load;
    InstanceStats tload;
    
    SchedulePlotter::getLoadStats(instance, state, current, load, tload);
    
    out << "<html><head>" << endl <<
    "<link rel=\"stylesheet\" type=\"text/css\" href=\"chart.css\"/>" << endl <<
    "</head><body>";
    
    out << "<table>" << endl;
    for (unsigned int m = 0; m < instance.num_machines; ++m)
    {
        table(out, load[m], tload[m], instance, m);      
    }
    out << "</table>" << endl;
    
    for (unsigned int m = 0; m < instance.num_machines; ++m)
    {
        out << "<h2><a name=\"mach" << m << "\"></a>Machine " << m << "</h2>" << endl;
        chart(out, load[m], tload[m], instance, m, state, current);
    }
    
    out << "</html>" << endl;
}
