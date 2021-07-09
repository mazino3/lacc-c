#if !AMALGAMATION
# define INTERNAL
# define EXTERNAL extern
#endif
#include "builtin.h"
#include "declaration.h"
#include "eval.h"
#include "expression.h"
#include "symtab.h"
#include "typetree.h"
#include <lacc/context.h>
#include <lacc/token.h>

/*
 * Parse call to builtin symbol __builtin_va_start, which is the result
 * of calling va_start(arg, s). Return type depends on second input
 * argument.
 */
static struct block *parse__builtin_va_start(
    struct definition *def,
    struct block *block)
{
    Type type;
    String name;
    const struct member *mb;
    const struct symbol *sym;

    consume('(');
    block = assignment_expression(def, block);
    consume(',');
    consume(IDENTIFIER);

    name = access_token(0)->d.string;
    sym = sym_lookup(&ns_ident, name);
    if (!sym) {
        error("Undefined symbol '%s'.", str_raw(name));
        exit(1);
    }

    type = def->symbol->type;
    if (!is_vararg(type)) {
        error("Function must be vararg to use va_start.");
        exit(1);
    }

    mb = get_member(type, nmembers(type) - 1);
    if (!str_eq(mb->name, sym->name) || sym->depth != 1) {
        error("Expected last function argument %s as va_start argument.",
            str_raw(mb->name));
        exit(1);
    }

    consume(')');
    eval__builtin_va_start(block, block->expr);
    return block;
}

/*
 * Parse call to builtin symbol __builtin_va_arg, which is the result of
 * calling va_arg(arg, T). Return type depends on second input argument.
 */
static struct block *parse__builtin_va_arg(
    struct definition *def,
    struct block *block)
{
    struct var value;
    Type type;

    consume('(');
    block = assignment_expression(def, block);
    value = eval(def, block, block->expr);
    consume(',');
    type = declaration_specifiers(NULL);
    if (peek() != ')') {
        block = declarator(def, block, type, &type, NULL);
    }

    consume(')');
    block->expr = eval_va_arg(def, block, value, type);
    return block;
}

/*
 * Implement alloca as a normal VLA.
 *
 *   void *ptr = alloca(n + 1);
 *
 * is translated to
 *
 *   size_t len = n + 1;
 *   char sym[len];
 *   void *ptr = (void *) sym;
 *
 */
static struct block *parse__builtin_alloca(
    struct definition *def,
    struct block *block)
{
    struct var t1;
    struct symbol *sym;

    consume('(');
    block = assignment_expression(def, block);
    consume(')');

    t1 = create_var(def, basic_type__unsigned_long);
    eval_assign(def, block, t1, block->expr);

    sym = sym_create_temporary(type_create_vla(basic_type__char, t1.value.symbol));
    array_push_back(&def->locals, sym);

    block = declare_vla(def, block, sym);
    block->expr = eval_cast(def, block, var_direct(sym),
        type_create_pointer(basic_type__void));
    return block;
}

INTERNAL void register_builtins(void)
{
    sym_create_builtin(str_c("__builtin_alloca"), parse__builtin_alloca);
    sym_create_builtin(str_c("__builtin_va_start"), parse__builtin_va_start);
    sym_create_builtin(str_c("__builtin_va_arg"), parse__builtin_va_arg);
}