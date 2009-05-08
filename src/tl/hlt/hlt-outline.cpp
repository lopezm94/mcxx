#include "hlt-outline.hpp"
#include <algorithm>
#include <functional>

using namespace TL::HLT;

int Outline::_num_outlines = 0;

Outline::Outline(Statement stmt)
    : _packed_arguments(false), 
    _use_nonlocal_scope(true),
    _outline_num(_num_outlines++)
{
    _outline_statements.append(stmt);
}

Outline::Outline(ObjectList<Statement> stmt_list)
    : _packed_arguments(false), 
    _use_nonlocal_scope(true),
    _outline_statements(stmt_list),
    _outline_num(_num_outlines++)
{
}

TL::Source Outline::get_source()
{
    do_outline();
    return _outlined_source;
}

Outline& Outline::use_packed_arguments()
{
    _packed_arguments = true;
    return *this;
}

void Outline::do_outline()
{
    // We can start building the outline code
    Source template_headers,
           required_qualification,
           outline_parameters,
           outline_body;

    _outlined_source
        << template_headers
        << "void " << required_qualification << _outline_name << "(" << outline_parameters << ")"
        << outline_body
        ;

    // This gets some information about the enclosing function
    compute_outline_name(template_headers, required_qualification);

    // Now find out all the required symbols
    compute_referenced_entities(outline_parameters);

    compute_outlined_body(outline_body);
}

static std::string template_header_regeneration(TL::AST_t template_header)
{
    return "template <" + template_header.prettyprint() + " >";
}

void Outline::compute_outline_name(Source &template_headers, Source &required_qualification)
{
    // Note: We are assuming all statements come from the same function
    // definition
    FunctionDefinition funct_def = _outline_statements[0].get_enclosing_function();
    _enclosing_function = funct_def.get_function_symbol();

    _is_member = _enclosing_function.is_member();

    IdExpression id_expr = funct_def.get_function_name();

    // FIXME - This is a bit lame
    _is_inlined_member = (!id_expr.is_qualified() && _is_member);

    if (id_expr.is_qualified())
    {
        required_qualification
            << id_expr.get_qualified_part() << "::"
            ;
    }

    _is_templated = funct_def.is_templated();
    if (_is_templated)
    {
        _template_header = funct_def.get_template_header();
        template_headers <<
            concat_strings(_template_header.map(functor(template_header_regeneration)))
            ;
    }

    _outline_name
        << "_ol_" << _outline_num << "_" << _enclosing_function.get_name()
        ;
}

static void get_referenced_entities(TL::Statement stmt, TL::ObjectList<TL::Symbol>* entities)
{
    entities->insert(stmt.non_local_symbol_occurrences(TL::Statement::ONLY_VARIABLES).map(functor(&TL::IdExpression::get_symbol)));
}

static std::string c_argument_declaration(TL::Symbol sym)
{
    TL::Type type = sym.get_type();
    if (type.is_array())
    {
        type = type.array_element();
    }

    type = type.get_pointer_to();

    return type.get_declaration(sym.get_scope(), sym.get_name());
}

#if 0
static std::string cxx_argument_declaration(TL::Symbol sym)
{
    Type type = sym.get_type();

    type = type.get_reference_to();

    return type.get_declaration(sym.get_scope(), sym.get_name());
}
#endif

static void get_field_decls(TL::Symbol sym, TL::Source *src)
{
    (*src) << c_argument_declaration(sym) << ";"
        ;
}

void Outline::compute_referenced_entities(Source &arguments)
{
    ObjectList<Symbol> entities;
    std::for_each(_outline_statements.begin(), _outline_statements.end(), 
            std::bind2nd(ptr_fun(get_referenced_entities), &entities));

    if (_use_nonlocal_scope)
    {
        // Remove those that we know that are nonlocal to the function
        entities = entities.filter(negate(predicate(&Symbol::has_local_scope)));
    }

    _referenced_symbols = entities;


    if (_packed_arguments)
    {
        _packed_argument_typename 
            << "struct _arg_pack_" << _outline_num
            ;
        arguments
            << _packed_argument_typename << " _args"
            ;

        Source fields;
        _additional_decls_source
            << _packed_argument_typename
            << "{"
            << fields
            << "}"
            ;
        std::for_each(_referenced_symbols.begin(), _referenced_symbols.end(),
                std::bind2nd(std::ptr_fun(get_field_decls), &fields));
    }
    else
    {
        if (_enclosing_function.is_member() && !_enclosing_function.is_static())
        {
            Type class_type = _enclosing_function.get_class_type();
            arguments
                << class_type.get_declaration(_enclosing_function.get_scope(), "_this")
                ;
        }
        arguments.append_with_separator(
                concat_strings( entities.map(functor(c_argument_declaration)),
                    ","),
                ",");
    }
}

struct AuxiliarOutlineReplace
{
    TL::ReplaceSrcIdExpression *_replacements;
    TL::Symbol _enclosing_function;
    bool _packed_args;

    AuxiliarOutlineReplace(TL::ReplaceSrcIdExpression& replacements,
            TL::Symbol enclosing_function,
            bool packed_args)
        : _replacements(&replacements),
        _enclosing_function(enclosing_function),
        _packed_args(packed_args) { }

    void operator()(TL::Symbol sym)
    {
        if (/* !IS_CXX_LANGUAGE
                || */ !sym.is_member() 
                || !(_enclosing_function.is_member() && !_enclosing_function.is_static())
                || sym.get_class_type().is_same_type(_enclosing_function.get_class_type()))
        {
            if (_packed_args)
            {
                _replacements->add_replacement(sym, "(*_args->" + sym.get_name() + ")");
            }
            else
            {
                _replacements->add_replacement(sym, "(*" + sym.get_name() + ")");
            }
        }
        else
        {
            if (_packed_args)
            {
                _replacements->add_replacement(sym, "(_args->this->" + sym.get_name() + ")");
            }
            else
            {
                _replacements->add_replacement(sym, "(_this->" + sym.get_name() + ")");
            }
        }
    }
};

struct auxiliar_replace_t
{
    TL::Source *src;
    TL::ReplaceSrcIdExpression *replacements;
};

static void print_replaced_stmts(TL::Statement stmt, auxiliar_replace_t aux)
{
    (*aux.src) << aux.replacements->replace(stmt);
}

void Outline::compute_outlined_body(Source &outlined_body)
{
    // Warning, empty outlines are rendered invalid because of this
    ReplaceSrcIdExpression replacements(_outline_statements[0].get_scope_link());

    std::for_each(_referenced_symbols.begin(),
            _referenced_symbols.end(),
            AuxiliarOutlineReplace(replacements, _enclosing_function, _packed_arguments));

    auxiliar_replace_t aux = { &outlined_body, &replacements };

    std::for_each(_outline_statements.begin(),
            _outline_statements.end(),
            std::bind2nd(std::ptr_fun(print_replaced_stmts), aux));
}
