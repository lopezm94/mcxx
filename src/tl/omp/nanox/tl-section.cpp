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




#include "tl-nanos.hpp"
#include "tl-omp-nanox.hpp"
#include "tl-data-env.hpp"
#include "tl-counters.hpp"
#include "tl-devices.hpp"

using namespace TL;
using namespace TL::Nanox;

void OMPTransform::section_preorder(PragmaCustomConstruct ctr) 
{
    // Add a new section
    SectionInfoList& section_list(_section_info.back());
    section_list.push_back(SectionInfo());
}

void OMPTransform::section_postorder(PragmaCustomConstruct ctr)
{
    SectionInfoList& _section_list(_section_info.back());
    SectionInfo& current_section_info(_section_list.back());

    Statement statement = ctr.get_statement();

    // Get the enclosing sections or parallel sections since 'section' does not
    // have a data sharing by itself
    AST_t enclosing_sections = ctr.get_ast();
    while (enclosing_sections.is_valid()
            && !is_pragma_custom_construct("omp", "sections", enclosing_sections, ctr.get_scope_link())
            && !is_pragma_custom_construct("omp", "parallel|sections", enclosing_sections, ctr.get_scope_link()))
    {
        enclosing_sections = enclosing_sections.get_parent();
    }

    ERROR_CONDITION(!enclosing_sections.is_valid(), "Invalid section tree", 0);
    OpenMP::DataSharingEnvironment& data_sharing = openmp_info->get_data_sharing(enclosing_sections);

    DataEnvironInfo data_environ_info;
    compute_data_environment(
            data_sharing,
            ctr,
            data_environ_info,
            _converted_vlas);


    std::string struct_arg_type_name;

    define_arguments_structure(ctr,
            struct_arg_type_name,
            data_environ_info,
            ObjectList<OpenMP::DependencyItem>(),
            Source());
    
    FunctionDefinition funct_def = ctr.get_enclosing_function();
    Symbol function_symbol = funct_def.get_function_symbol();

    int outline_num = TL::CounterManager::get_counter(NANOX_OUTLINE_COUNTER);

    std::stringstream ss;
    ss << "_ol_" << function_symbol.get_name() << "_" << outline_num;

    std::string outline_name = ss.str();

    Source final_barrier;

    if (!ctr.get_clause("nowait").is_defined())
    {
        final_barrier << get_barrier_code(ctr.get_ast())
            ;
    }

    Source device_descriptor,
           device_description,
           device_description_line,
           ancillary_device_description;

    device_descriptor << outline_name << "_devices";
    device_description
        << "nanos_device_t " << device_descriptor << "[] ="
        << "{"
        << device_description_line
        << "};"
        ;

    OutlineFlags outline_flags;

    DeviceHandler &device_handler = DeviceHandler::get_device_handler();
    ObjectList<std::string> current_targets;
    data_sharing.get_all_devices(current_targets);
    for (ObjectList<std::string>::iterator it = current_targets.begin();
            it != current_targets.end();
            it++)
    {
        DeviceProvider* device_provider = device_handler.get_device(*it);

        if (device_provider == NULL)
        {
            internal_error("invalid device '%s' at '%s'\n",
                    it->c_str(), ctr.get_ast().get_locus().c_str());
        }

        Source initial_setup, replaced_body;

        device_provider->do_replacements(data_environ_info,
                statement.get_ast(),
                ctr.get_scope_link(),
                initial_setup,
                replaced_body);

        Source outline_body;
        outline_body
            << replaced_body
            ;

        device_provider->create_outline(outline_name,
                struct_arg_type_name,
                data_environ_info,
                outline_flags,
                ctr.get_statement().get_ast(),
                ctr.get_scope_link(),
                initial_setup,
                outline_body);

        device_provider->get_device_descriptor(outline_name, 
                data_environ_info, 
                outline_flags,
                ctr.get_statement().get_ast(),
                ctr.get_scope_link(),
                ancillary_device_description, 
                device_description_line);
    }

    // FIXME - Move this to a tl-workshare.cpp
    Source spawn_source;

    Source fill_outline_arguments;
    fill_data_args(
            "section_data",
            data_environ_info, 
            ObjectList<OpenMP::DependencyItem>(), // empty
            /* is_pointer */ true,
            fill_outline_arguments);

    Source alignment;
    if (Nanos::Version::interface_is_at_least("master", 5004))
    {
        alignment <<  "__alignof__(" << struct_arg_type_name << ")"
            ;
    }

    Source num_devices, struct_size,
           num_copies, copy_data, data, nanos_create_wd;

    num_devices << 1;
    struct_size << "sizeof(" << struct_arg_type_name << ")";
    data << "(void**)&section_data";

    // FIXME: No copies at the moment
    num_copies << 0;
    copy_data << "(nanos_copy_data_t**)0";

    Source properties_opt, device_description_opt, decl_dyn_props_opt,
           constant_code_opt, constant_struct_definition, constant_variable_declaration;
    if (Nanos::Version::interface_is_at_least("master", 5012))
    {
        nanos_create_wd = OMPTransform::get_nanos_create_wd_compact_code(struct_size, data, copy_data);

        decl_dyn_props_opt << "nanos_wd_dyn_props_t dyn_props = {0};";
        
        constant_code_opt
            << constant_struct_definition
            <<  constant_variable_declaration
            ;

        constant_struct_definition
            << "struct nanos_const_wd_definition_local_t"
            << "{"
            <<      "nanos_const_wd_definition_t base;"
            <<      "nanos_device_t devices[" << num_devices << "];"
            << "};"
            ;

        constant_variable_declaration
            << "static struct nanos_const_wd_definition_local_t _const_def ="
            << "{"
            <<      "{"
            <<          "{"
            <<              "1, " /* mandatory_creation */
            <<              "0, " /* tied */
            <<              "0, " /* reserved0 */
            <<              "0, " /* reserved1 */
            <<              "0, " /* reserved2 */
            <<              "0, " /* reserved3 */
            <<              "0, " /* reserved4 */
            <<              "0, " /* reserved5 */
            <<              "0, " /* priority */
            <<          "}, "
            <<          alignment   << ", "
            <<          num_copies  << ", "
            <<          num_devices << ", "
            <<      "},"
            <<      "{"
            <<          device_description_line
            <<      "}"
            << "};"
            ;
    }
    else
    {
        nanos_create_wd = OMPTransform::get_nanos_create_wd_code(num_devices,
                device_descriptor, struct_size, alignment, data, num_copies, copy_data);

        properties_opt
            << "nanos_wd_props_t props;"
            << "__builtin_memset(&props, 0, sizeof(props));"
            << "props.mandatory_creation = 1;"
            ;
        device_description_opt << device_description;
    }

    spawn_source
        << "{"
        <<    "nanos_err_t err;"
        <<    "nanos_wd_t  wd = (nanos_wd_t)0;"
        <<    ancillary_device_description
        <<    device_description_opt
        <<    struct_arg_type_name << "*section_data = ("<< struct_arg_type_name << "*)0;"
        <<    constant_code_opt
        <<    properties_opt
        <<    decl_dyn_props_opt
        <<    "err = " << nanos_create_wd
        <<    "if (err != NANOS_OK) nanos_handle_error(err);"
        <<    fill_outline_arguments
        // Do not submit, it will be done by the slicer
        <<    statement_placeholder(current_section_info.placeholder)
        << "}"
        ;

    AST_t spawn_tree = spawn_source.parse_statement(ctr.get_ast(), ctr.get_scope_link());
    ctr.get_ast().replace(spawn_tree);
    TL::CounterManager::get_counter(NANOX_OUTLINE_COUNTER)++;
}
