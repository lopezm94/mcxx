/*--------------------------------------------------------------------
  (C) Copyright 2006-2013 Barcelona Supercomputing Center
                          Centro Nacional de Supercomputacion

  This file is part of Mercurium C/C++ source-to-source compiler.

  See AUTHORS file in the top level directory for information
  regarding developers and contributors.

  This library is free software; you can redistribute it and/or
  modify it under the terms of the GNU Lesser General Public
  License as published by the Free Software Foundation; either
  version 3 of the License, or (at your option) any later version.

  Mercurium C/C++ source-to-source compiler is distributed in the hope
  that it will be useful, but WITHOUT ANY WARRANTY; without even the
  implied warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR
  PURPOSE.  See the GNU Lesser General Public License for more
  details.

  You should have received a copy of the GNU Lesser General Public
  License along with Mercurium C/C++ source-to-source compiler; if
  not, write to the Free Software Foundation, Inc., 675 Mass Ave,
  Cambridge, MA 02139, USA.
// --------------------------------------------------------------------*/

#include <sys/stat.h>

#include "cxx-cexpr.h"
#include "tl-counters.hpp"
#include "tl-weighted-tdg-phase.hpp"
#include "tl-analysis-base.hpp"
#include "tl-analysis-utils.hpp"
#include "tl-symbol-utils.hpp"

namespace TL {
namespace Analysis {

    // ****************************************************************************** //
    // ************************ Visitor to enrich Task nodes ************************ //

    void Task2FPGA::unhandled_node(const Nodecl::NodeclBase& n)
    {
        WARNING_MESSAGE("Unhandled node of type '%s' while Weighted-ETDG transformations.\n",
                        ast_print_node_type(n.get_kind()));
    }
    
    void Task2FPGA::visit(const Nodecl::OpenMP::Task& n)
    {
        // 1.- Create the node with the value of the onto clause
        //     FIXME The value of onto_value_nodecl will be the one in id2Onto[xx]
        unsigned onto_value = 0;
        Nodecl::IntegerLiteral onto_value_nodecl = Nodecl::IntegerLiteral::make(
                Type::get_unsigned_int_type(),
                const_value_get_integer(onto_value, /*num_bytes*/4, /*sign*/0));
        Nodecl::List onto_exprs = Nodecl::List::make(onto_value_nodecl);

        // 2.- Create the onto clause
        Nodecl::OmpSs::Onto onto_clause = Nodecl::OmpSs::Onto::make(onto_exprs);

        // 3.- Add the onto clause to the task
        n.get_environment().as<Nodecl::List>().append(onto_clause);
    }

    // ********************** END Visitor to enrich Task nodes ********************** //
    // ****************************************************************************** //


    // ****************************************************************************** //
    // ***************************** Weighted TDG Phase ***************************** //

namespace {
    void tokenizer(std::string str, std::set<std::string>& result)
    {
        std::string temporary("");
        for (std::string::const_iterator it = str.begin();
             it != str.end(); ++it)
        {
            const char & c(*it);
            if (c == ',' || c == ' ')
            {
                if (temporary != "")
                {
                    std::cerr << "   -> " << temporary << std::endl;
                    result.insert(temporary);
                    temporary = "";
                }
            }
            else
            {
                temporary += c;
            }
        }
        if (temporary != "")
        {
            result.insert(temporary);
        }
    }
}

    WeightedTdgPhase::WeightedTdgPhase()
            : _functions_str(""),
              _call_graph_str(""), _call_graph_enabled(true)
              
    {
        set_phase_name("Phase for generating a weighted TDG");

        register_parameter("functions",
                           "Points out the function that has to be analyzed",
                           _functions_str,
                           "").connect(std::bind(&WeightedTdgPhase::set_functions, this, std::placeholders::_1));

        register_parameter("call_graph",
                           "If set to '1' enbles analyzing the call graph of all functions specified in parameter 'functions'",
                           _call_graph_str,
                           "1").connect(std::bind(&WeightedTdgPhase::set_call_graph, this, std::placeholders::_1));
    }

    void WeightedTdgPhase::run(TL::DTO& dto)
    {
        AnalysisBase analysis(/*_ompss_mode_enabled*/ true);
        Nodecl::NodeclBase ast = *std::static_pointer_cast<Nodecl::NodeclBase>(dto["nodecl"]);

        // 1.- _functions_str is a comma-separated list of function names
        //     Transform it into a set of strings
        std::set<std::string> functions;
        tokenizer(_functions_str, functions);

        // 2.- Static generation of the Task Dependency Graph
        ObjectList<TaskDependencyGraph*> tdgs = analysis.task_dependency_graph(
                    ast, functions, _call_graph_enabled,
                    /*taskparts_enabled*/ false, /*_etdg_enabled*/ true);

        // 3.- Print tdg if asked to do so
        if (debug_options.print_tdg)
        {
            for (ObjectList<TaskDependencyGraph*>::iterator it = tdgs.begin(); it != tdgs.end(); ++it)
                analysis.print_tdg((*it)->get_name());
        }

        // 4.- Traverse each TDG (one per nesting level, if aplicable)
        for (ObjectList<TaskDependencyGraph*>::iterator it = tdgs.begin(); it != tdgs.end(); ++it)
        {
            ExpandedTaskDependencyGraph* etdg = (*it)->get_etdg();
             std::vector<SubETDG*> sub_tdgs = etdg->get_etdgs();
             for (ObjectList<SubETDG*>::iterator itt = sub_tdgs.begin(); itt != sub_tdgs.end(); ++itt)
             {
                 traverse_subetdg(*itt);
             }
        }

        // 5.- Add information to the tdg
        Task2FPGA task2fpga;
        task2fpga.walk(ast);
    }

    void WeightedTdgPhase::traverse_subetdg_rec(ETDGNode* n)
    {
        // Make sure you only traverse a node once
        if (n->is_visited())
            return;
        n->set_visited(true);
        // std::cerr << "Traversing node " << n->get_id() << std::endl;

        // TODO Do your work on the node here
        

        // Keep iterating over the node's childre
        std::set<ETDGNode*> outputs = n->get_outputs();
        for (std::set<ETDGNode*>::iterator it = outputs.begin(); it != outputs.end(); ++it)
        {
            traverse_subetdg_rec(*it);
        }
    }

    void WeightedTdgPhase::traverse_subetdg(SubETDG* etdg)
    {
        const ObjectList<ETDGNode*>& roots = etdg->get_roots();
        for (ObjectList<ETDGNode*>::const_iterator it = roots.begin(); it != roots.end(); ++it)
        {
            traverse_subetdg_rec(*it);
        }

        etdg->clear_visits();
    }

    void WeightedTdgPhase::set_functions(const std::string& functions_str)
    {
        if (functions_str != "")
            _functions_str = functions_str;
    }

    void WeightedTdgPhase::set_call_graph(const std::string& call_graph_enabled_str)
    {
        if (call_graph_enabled_str == "0")
            _call_graph_enabled = false;
    }

    // *************************** END Weighted TDG Phase *************************** //
    // ****************************************************************************** //
}
}

EXPORT_PHASE(TL::Analysis::WeightedTdgPhase);