/*--------------------------------------------------------------------
  (C) Copyright 2006-2012 Barcelona Supercomputing Center
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

#include "tl-nodecl-visitor.hpp"
#include "tl-outline-info.hpp"

#include <set>

namespace TL { namespace Nanox {

class LoweringVisitor : public Nodecl::ExhaustiveVisitor<void>
{
    public:
        LoweringVisitor();
        virtual void visit(const Nodecl::OpenMP::Task& construct);
        virtual void visit(const Nodecl::OpenMP::TaskwaitShallow& construct);
        virtual void visit(const Nodecl::OpenMP::WaitOnDependences& construct);
        virtual void visit(const Nodecl::OpenMP::TaskCall& construct);
        virtual void visit(const Nodecl::OpenMP::Single& construct);
        virtual void visit(const Nodecl::OpenMP::BarrierFull& construct);
        virtual void visit(const Nodecl::OpenMP::Parallel& construct);
        virtual void visit(const Nodecl::OpenMP::For& construct);
        virtual void visit(const Nodecl::OpenMP::Critical& construct);
        virtual void visit(const Nodecl::OpenMP::FlushMemory& construct);
        virtual void visit(const Nodecl::OpenMP::Atomic& construct);

    private:
        TL::Symbol declare_argument_structure(OutlineInfo& outline_info, Nodecl::NodeclBase construct);
        bool c_type_needs_vla_handling(TL::Type t);
        bool c_requires_vla_handling(OutlineDataItem& outline_data_item);

        void emit_async_common(
                Nodecl::NodeclBase construct,
                TL::Symbol function_symbol, 
                Nodecl::NodeclBase statements,
                Nodecl::NodeclBase priority,
                bool is_untied,

                OutlineInfo& outline_info);

        void emit_outline(OutlineInfo& outline_info,
                Nodecl::NodeclBase construct,
                Source body_source,
                const std::string& outline_name,
                TL::Symbol structure_symbol);
#if 0
        void emit_outline(OutlineInfo& outline_info,
                Nodecl::NodeclBase body,
                const std::string& outline_name,
                TL::Symbol structure_symbol);
#endif

        TL::Type c_handle_vla_type_rec(
                OutlineDataItem& outline_data_item,
                TL::Type type, 
                TL::Scope class_scope, 
                TL::Symbol new_class_symbol,
                TL::Type new_class_type,
                TL::ObjectList<TL::Symbol>& new_symbols,
                const std::string& filename, 
                int line);
        void c_handle_vla_type(
                OutlineDataItem& outline_data_item,
                TL::Scope class_scope, 
                TL::Symbol new_class_symbol,
                TL::Type new_class_type,
                TL::ObjectList<TL::Symbol>& new_symbols,
                const std::string& filename, 
                int line);

        void fortran_handle_vla_type(
                OutlineDataItem& outline_data_item,
                TL::Type field_type,
                TL::Symbol field_symbol,
                TL::Scope class_scope, 
                TL::Symbol new_class_symbol,
                TL::Type new_class_type,
                TL::ObjectList<TL::Symbol>& new_symbols,
                const std::string& filename, 
                int line);

        void fill_arguments(
                Nodecl::NodeclBase ctr,
                OutlineInfo& outline_info,
                Source& fill_outline_arguments,
                Source& fill_immediate_arguments
                );

        int count_dependences(OutlineInfo& outline_info);
        int count_copies(OutlineInfo& outline_info);

        void fill_copies(
                Nodecl::NodeclBase ctr,
                OutlineInfo& outline_info, 
                // Source arguments_accessor,
                // out
                Source& copy_ol_decl,
                Source& copy_ol_arg,
                Source& copy_ol_setup,
                Source& copy_imm_arg,
                Source& copy_imm_setup);

        void fill_dependences(
                Nodecl::NodeclBase ctr,
                OutlineInfo& outline_info,
                Source arguments_accessor,
                // out
                Source& result_src
                );
        void fill_dependences_wait(
                Nodecl::NodeclBase ctr,
                OutlineInfo& outline_info,
                // out
                Source& result_src
                );

        void emit_wait_async(Nodecl::NodeclBase construct, OutlineInfo& outline_info);

        static void fill_dimensions(int n_dims, int actual_dim, int current_dep_num,
                Nodecl::NodeclBase * dim_sizes, 
                Type dep_type, 
                Source& dims_description, 
                Source& dependency_regions_code, 
                Scope sc);

        void emit_wait_async(Nodecl::NodeclBase construct, bool has_dependences, OutlineInfo& outline_info);

        std::string get_outline_name(TL::Symbol function_symbol);

        Source fill_const_wd_info(
                Source &struct_arg_type_name,
                const std::string& outline_name,
                bool is_untied,
                bool mandatory_creation);

        void allocate_immediate_structure(
                OutlineInfo& outline_info,
                Source &struct_arg_type_name,
                Source &struct_size,

                // out
                Source &immediate_decl,
                Source &dynamic_size);

        void parallel_spawn(
                OutlineInfo& outline_info,
                Nodecl::NodeclBase construct,
                Nodecl::NodeclBase num_replicas,
                const std::string& outline_name,
                TL::Symbol structure_symbol);

        void loop_spawn(
                OutlineInfo& outline_info,
                Nodecl::NodeclBase construct,
                Nodecl::List distribute_environment,
                Nodecl::List ranges,
                const std::string& outline_name,
                TL::Symbol structure_symbol,
                Source inline_iteration_source);

        Source full_barrier_source();

        void reduction_initialization_code(
                Source max_threads,
                OutlineInfo& outline_info,
                Nodecl::NodeclBase construct,
                // out
                Source &reduction_declaration,
                Source &register_code,
                Source &fill_outline_arguments,
                Source &fill_immediate_arguments);

        std::set<std::string> _lock_names;
};

} }
