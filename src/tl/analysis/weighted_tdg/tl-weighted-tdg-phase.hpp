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
--------------------------------------------------------------------*/



#ifndef TL_WEIGHTED_TDG_PHASE_HPP
#define TL_WEIGHTED_TDG_PHASE_HPP

#include "tl-compilerphase.hpp"
#include "tl-nodecl-visitor.hpp"
#include "tl-task-dependency-graph.hpp"

namespace TL {
namespace Analysis {

    class LIBTL_CLASS Task2FPGA : public Nodecl::ExhaustiveVisitor<void>
    {
    private:

    public:
        Ret unhandled_node(const Nodecl::NodeclBase& n);

        Ret visit(const Nodecl::OpenMP::Task& n);
    };

    //! Phase that creates an ETDG and enriches it with information about communication cost, task execution cost, etc.
    class LIBTL_CLASS WeightedTdgPhase : public CompilerPhase
    {
    private:
        std::string _functions_str;
        void set_functions(const std::string& functions_str);
        
        std::string _call_graph_str;
        bool _call_graph_enabled;
        void set_call_graph(const std::string& call_graph_enabled_str);

        void traverse_subetdg_rec(ETDGNode* n);
        void traverse_subetdg(SubETDG* etdg);

    public:
        //! Constructor of this phase
        WeightedTdgPhase();

        //!Entry point of the phase
        virtual void run(TL::DTO& dto);

        virtual ~WeightedTdgPhase() {};
    };
}
}

#endif  // TL_WEIGHTED_TDG_PHASE_HPP
