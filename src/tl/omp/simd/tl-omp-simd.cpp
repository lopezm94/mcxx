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

#include "tl-omp-simd.hpp"
#include "tl-nodecl-utils.hpp"

using namespace TL::Vectorization;

namespace TL { 
    namespace OpenMP {

        Simd::Simd()
            : PragmaCustomCompilerPhase("omp-simd"),  
            _simd_enabled(false), _svml_enabled(false), _ffast_math_enabled(false), _mic_enabled(false)
        {
            set_phase_name("Vectorize OpenMP SIMD parallel IR");
            set_phase_description("This phase vectorize the OpenMP SIMD parallel IR");

            register_parameter("simd_enabled",
                    "If set to '1' enables simd constructs, otherwise it is disabled",
                    _simd_enabled_str,
                    "0").connect(functor(&Simd::set_simd, *this));


            register_parameter("svml_enabled",
                    "If set to '1' enables svml math library, otherwise it is disabled",
                    _svml_enabled_str,
                    "0").connect(functor(&Simd::set_svml, *this));

            register_parameter("ffast_math_enabled",
                    "If set to '1' enables ffast_math operations, otherwise it is disabled",
                    _ffast_math_enabled_str,
                    "0").connect(functor(&Simd::set_ffast_math, *this));

            register_parameter("mic_enabled",
                    "If set to '1' enables compilation for MIC architecture, otherwise it is disabled",
                    _mic_enabled_str,
                    "0").connect(functor(&Simd::set_mic, *this));
        }

        void Simd::set_simd(const std::string simd_enabled_str)
        {
            if (simd_enabled_str == "1")
            {
                _simd_enabled = true;
            }
        }

        void Simd::set_svml(const std::string svml_enabled_str)
        {
            if (svml_enabled_str == "1")
            {
                _svml_enabled = true;
            }
        }

        void Simd::set_ffast_math(const std::string ffast_math_enabled_str)
        {
            if (ffast_math_enabled_str == "1")
            {
                _ffast_math_enabled = true;
            }
        }

        void Simd::set_mic(const std::string mic_enabled_str)
        {
            if (mic_enabled_str == "1")
            {
                _mic_enabled = true;
            }
        }

        void Simd::pre_run(TL::DTO& dto)
        {
            this->PragmaCustomCompilerPhase::pre_run(dto);
        }

        void Simd::run(TL::DTO& dto)
        {
            this->PragmaCustomCompilerPhase::run(dto);

            //RefPtr<FunctionTaskSet> function_task_set = RefPtr<FunctionTaskSet>::cast_static(dto["openmp_task_info"]);

            Nodecl::NodeclBase translation_unit = dto["nodecl"];

            if (_simd_enabled)
            {
                SimdVisitor simd_visitor(_ffast_math_enabled, _svml_enabled, _mic_enabled);
                simd_visitor.walk(translation_unit);
            }
        }

        SimdVisitor::SimdVisitor(bool ffast_math_enabled, bool svml_enabled, bool mic_enabled)
            : _vectorizer(TL::Vectorization::Vectorizer::get_vectorizer())
        {
            if (ffast_math_enabled)
                _vectorizer.enable_ffast_math();

            if (mic_enabled)
            {
                _vector_length = 64;
                _device_name = "knc";
                _support_masking = true;
                _mask_size = 16;

                if (svml_enabled)
                    _vectorizer.enable_svml_knc();
            }
            else
            {
                _vector_length = 16;
                _device_name = "smp";
                _support_masking = false;
                _mask_size = 0;

                if (svml_enabled)
                    _vectorizer.enable_svml_sse();
            }
        }

        void SimdVisitor::visit(const Nodecl::OpenMP::Simd& simd_node)
        {
            Nodecl::ForStatement for_statement = simd_node.get_statement().as<Nodecl::ForStatement>();
            Nodecl::List simd_environment = simd_node.get_environment().as<Nodecl::List>();

            Nodecl::List suitable_expressions;

            Nodecl::OpenMP::VectorSuitable omp_suitable = simd_environment.find_first<Nodecl::OpenMP::VectorSuitable>();
            if(!omp_suitable.is_null())
            {
                suitable_expressions = omp_suitable.get_suitable_expressions().as<Nodecl::List>();
            }

            // Add epilog before vectorization
            Nodecl::ForStatement epilog = Nodecl::Utils::deep_copy(
                    for_statement, for_statement).as<Nodecl::ForStatement>();

            simd_node.append_sibling(epilog);

            // Vectorize for
            VectorizerEnvironment for_environment(
                    _device_name,
                    _vector_length, 
                    _support_masking,
                    _mask_size,
                    NULL,
                    for_statement.get_statement().as<Nodecl::List>().front().retrieve_context(),
                    suitable_expressions);

            bool needs_epilog = 
                _vectorizer.vectorize(for_statement, for_environment); 

            // Process epilog
            if (needs_epilog)
            {
                VectorizerEnvironment epilog_environment(
                        _device_name,
                        _vector_length, 
                        _support_masking,
                        _mask_size,
                        NULL,
                        epilog.get_statement().as<Nodecl::List>().front().retrieve_context(),
                        suitable_expressions);

                _vectorizer.process_epilog(epilog, epilog_environment);
            }
            else // Remove epilog
            {
                Nodecl::Utils::remove_from_enclosing_list(epilog);
            }

            // Remove Simd node
            simd_node.replace(for_statement);
        }

        void SimdVisitor::visit(const Nodecl::OpenMP::SimdFor& simd_node)
        {
            Nodecl::OpenMP::For omp_for = simd_node.get_openmp_for().as<Nodecl::OpenMP::For>();
            Nodecl::List omp_simd_for_environment = simd_node.get_environment().as<Nodecl::List>();
            Nodecl::List omp_for_environment = omp_for.get_environment().as<Nodecl::List>();

            // Skipping AST_LIST_NODE
            Nodecl::NodeclBase loop = omp_for.get_loop();
            ERROR_CONDITION(!loop.is<Nodecl::ForStatement>(), 
                    "Unexpected node %s. Expecting a ForStatement after '#pragma omp simd for'", 
                    ast_print_node_type(loop.get_kind()));

            Nodecl::ForStatement for_statement = loop.as<Nodecl::ForStatement>();

            Nodecl::List suitable_expressions;

            Nodecl::OpenMP::VectorSuitable omp_suitable = omp_simd_for_environment.find_first<Nodecl::OpenMP::VectorSuitable>();
            if(!omp_suitable.is_null())
            {
                suitable_expressions = omp_suitable.get_suitable_expressions().as<Nodecl::List>();
            }

            // Add epilog with single before vectorization
            Nodecl::ForStatement epilog = Nodecl::Utils::deep_copy(
                    for_statement, for_statement).as<Nodecl::ForStatement>();

            Nodecl::List single_environment;

            Nodecl::NodeclBase barrier = omp_for_environment.find_first<Nodecl::OpenMP::BarrierAtEnd>();
            Nodecl::NodeclBase flush = omp_for_environment.find_first<Nodecl::OpenMP::FlushAtExit>();

            if (!barrier.is_null())
            {
                // Move barrier from omp for to single
                single_environment.append(barrier.shallow_copy());
                Nodecl::Utils::remove_from_enclosing_list(barrier);
            }
            if (!flush.is_null())
            {
                // Move flush from omp for to single
                single_environment.append(flush.shallow_copy());
                Nodecl::Utils::remove_from_enclosing_list(flush);
            }

            // Mark the induction variable as a private entity in the Single construct
            Nodecl::OpenMP::Private ind_var_priv = 
                Nodecl::OpenMP::Private::make(Nodecl::List::make(
                            TL::ForStatement(for_statement).get_induction_variable().make_nodecl()));
            single_environment.append(ind_var_priv);

            Nodecl::OpenMP::Single single_epilog =
                Nodecl::OpenMP::Single::make(single_environment,
                        Nodecl::List::make(epilog), epilog.get_locus());

            simd_node.append_sibling(single_epilog);

            // Vectorize for
            VectorizerEnvironment for_environment(
                    _device_name,
                    _vector_length,
                    _support_masking, 
                    _mask_size,
                    NULL,
                    for_statement.get_statement().as<Nodecl::List>().front().retrieve_context(),
                    suitable_expressions);

            bool needs_epilog = 
                _vectorizer.vectorize(for_statement, for_environment); 

            // Process epilog
            if (needs_epilog)
            {
                VectorizerEnvironment epilog_environment(
                        _device_name,
                        _vector_length, 
                        _support_masking,
                        _mask_size,
                        NULL,
                        epilog.get_statement().as<Nodecl::List>().front().retrieve_context(),
                        suitable_expressions);

                _vectorizer.process_epilog(epilog, epilog_environment);
            }
            else // Remove epilog
            {
                Nodecl::Utils::remove_from_enclosing_list(single_epilog);
            }

            // Remove Simd node
            simd_node.replace(omp_for);
        }

        void SimdVisitor::visit(const Nodecl::OpenMP::SimdFunction& simd_node)
        {
            Nodecl::FunctionCode function_code = simd_node.get_statement()
                .as<Nodecl::FunctionCode>();

            Nodecl::List omp_environment = simd_node.get_environment().as<Nodecl::List>();

            // Remove SimdFunction node
            simd_node.replace(function_code);


            Nodecl::List suitable_expressions;

            Nodecl::OpenMP::VectorSuitable omp_suitable = omp_environment.find_first<Nodecl::OpenMP::VectorSuitable>();
            if(!omp_suitable.is_null())
            {
                suitable_expressions = omp_suitable.get_suitable_expressions().as<Nodecl::List>();
            }

            Nodecl::OpenMP::VectorMask omp_mask = omp_environment.find_first<Nodecl::OpenMP::VectorMask>();
            Nodecl::OpenMP::VectorNoMask omp_nomask = omp_environment.find_first<Nodecl::OpenMP::VectorNoMask>();

            if((!omp_mask.is_null()) && (!omp_nomask.is_null()))
            {
                running_error("SIMD: 'mask' and 'nomask' clauses are now allowed at the same time\n");
            } 

            if((!omp_mask.is_null()) && (!_support_masking))
            {
                running_error("SIMD: 'mask' clause detected. Masking is not supported by the underlying architecture\n");
            } 

            // Mask Version
            if (_support_masking && omp_nomask.is_null())
            {
                printf("MASK\n");
                Nodecl::FunctionCode mask_func =
                    common_simd_function(function_code, suitable_expressions, true);

                // Append vectorized function code to scalar function
                simd_node.append_sibling(mask_func);
            }
            // Nomask Version
            if (omp_mask.is_null())
            {
                printf("NO MASK\n");
                Nodecl::FunctionCode no_mask_func =
                    common_simd_function(function_code, suitable_expressions, false);
                
                // Append vectorized function code to scalar function
                simd_node.append_sibling(no_mask_func);
            }
        }

        Nodecl::FunctionCode SimdVisitor::common_simd_function(const Nodecl::FunctionCode& function_code,
                const Nodecl::List& suitable_expressions,
                const bool masked_version)
        {
            TL::Symbol func_sym = function_code.get_symbol();
            std::string orig_func_name = func_sym.get_name();

            // Set new symbol
            std::stringstream vector_func_name; 

            vector_func_name <<"__" 
                << orig_func_name
                << "_" 
                << _device_name 
                << "_" 
                << _vector_length
                ;

            if (masked_version)
            {
                vector_func_name << "_mask";
            }

            TL::Symbol new_func_sym = func_sym.get_scope().
                new_symbol(vector_func_name.str());

            Nodecl::Utils::SimpleSymbolMap func_sym_map;
            func_sym_map.add_map(func_sym, new_func_sym);

            Nodecl::FunctionCode vector_func_code = 
                Nodecl::Utils::deep_copy(function_code, 
                        function_code,
                        func_sym_map).as<Nodecl::FunctionCode>();

            // Vectorize function
            VectorizerEnvironment _environment(
                    _device_name,
                    _vector_length, 
                    _support_masking,
                    _mask_size,
                    NULL,
                    vector_func_code.get_statements().retrieve_context(),
                    suitable_expressions);

            _vectorizer.vectorize(vector_func_code, _environment, masked_version); 

            // Add SIMD version to vector function versioning
            _vectorizer.add_vector_function_version(orig_func_name, vector_func_code, 
                    _device_name, _vector_length, NULL, masked_version, 
                    TL::Vectorization::SIMD_FUNC_PRIORITY, false);

            return vector_func_code;
       }
    } 
}

EXPORT_PHASE(TL::OpenMP::Simd)