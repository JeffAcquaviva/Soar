/*************************************************************************
 * PLEASE SEE THE FILE "license.txt" (INCLUDED WITH THIS SOFTWARE PACKAGE)
 * FOR LICENSE AND COPYRIGHT INFORMATION.
 *************************************************************************/
#include "ebc.h"

#include "agent.h"
#include "condition.h"
#include "dprint.h"
#include "instantiation.h"
#include "output_manager.h"
#include "preference.h"
#include "print.h"
#include "rete.h"
#include "symbol.h"
#include "symbol_manager.h"
#include "test.h"
#include "working_memory.h"

#include <assert.h>

uint64_t Explanation_Based_Chunker::get_or_create_identity(Symbol* orig_var)
{
    int64_t existing_o_id = 0;

    auto iter_sym = instantiation_identities->find(orig_var);
    if (iter_sym != instantiation_identities->end())
    {
        existing_o_id = iter_sym->second;
    }

    if (!existing_o_id)
    {
        increment_counter(ovar_id_counter);
        (*instantiation_identities)[orig_var] = ovar_id_counter;
//        instantiation_being_built->bt_identity_set_mappings->insert({ovar_id_counter, 0});

        return ovar_id_counter;
    }
    return existing_o_id;
}

void Explanation_Based_Chunker::add_identity_to_id_test(condition* cond,
                                       byte field_num,
                                       rete_node_level levels_up)
{
    test t = 0;

    t = var_test_bound_in_reconstructed_conds(thisAgent, cond, field_num, levels_up);
    cond->data.tests.id_test->identity = t->identity;
}

void Explanation_Based_Chunker::force_add_identity(Symbol* pSym, uint64_t pID)
{
    if (pSym->is_sti()) (*instantiation_identities)[pSym] = pID;
}

void Explanation_Based_Chunker::add_identity_to_test(test pTest)
{
    if (!pTest->identity) pTest->identity = get_or_create_identity(pTest->data.referent);
}

void Explanation_Based_Chunker::add_explanation_to_RL_condition(rete_node* node,
    condition* cond,
    node_varnames* nvn,
    uint64_t pI_id,
    AddAdditionalTestsMode additional_tests)
{
        rete_test* rt = node->b.posneg.other_tests;

        /* --- Store original referent information.  Note that sometimes the
         *     original referent equality will be stored in the beta nodes extra tests
         *     data structure rather than the alpha memory --- */
        alpha_mem* am;
        am = node->b.posneg.alpha_mem_;

        if (nvn)
        {
            add_varname_identity_to_test(thisAgent, nvn->data.fields.id_varnames, cond->data.tests.id_test, pI_id, true);
            add_varname_identity_to_test(thisAgent, nvn->data.fields.attr_varnames, cond->data.tests.attr_test, pI_id, true);
            add_varname_identity_to_test(thisAgent, nvn->data.fields.value_varnames, cond->data.tests.value_test, pI_id, true);
        }

        /* --- on hashed nodes, add equality test for the hash function --- */
        if ((node->node_type == MP_BNODE) || (node->node_type == NEGATIVE_BNODE))
        {
            add_identity_to_id_test(cond,
                node->left_hash_loc_field_num,
                node->left_hash_loc_levels_up);

        }
        else if (node->node_type == POSITIVE_BNODE)
        {
            add_identity_to_id_test(cond,
                node->parent->left_hash_loc_field_num,
                node->parent->left_hash_loc_levels_up);

        }

        /* -- Now process any additional relational test -- */
        test chunk_test = NULL;
        TestType test_type;
        bool has_referent;
        for (; rt != NIL; rt = rt->next)
        {
            chunk_test = NULL;
            has_referent = true;
            if (test_is_variable_relational_test(rt->type))
            {
                test_type = relational_test_type_to_test_type(kind_of_relational_test(rt->type));
                if ((test_type == EQUALITY_TEST) || (test_type == NOT_EQUAL_TEST))
                {
                    test ref_test = var_test_bound_in_reconstructed_conds(thisAgent, cond,
                        rt->data.variable_referent.field_num,
                        rt->data.variable_referent.levels_up);
                    if (ref_test->data.referent->is_sti())
                    {
                        chunk_test = make_test(thisAgent, ref_test->data.referent, test_type);
                        chunk_test->identity = ref_test->identity;
                    }
                }
            }
            if (chunk_test)
            {
                if (rt->right_field_num == 0)
                {
                    add_constraint_to_explanation(&(cond->data.tests.id_test), chunk_test, pI_id, has_referent);
                }
                else if (rt->right_field_num == 1)
                {
                    add_constraint_to_explanation(&(cond->data.tests.attr_test), chunk_test, pI_id, has_referent);
                }
                else
                {
                    add_constraint_to_explanation(&(cond->data.tests.value_test), chunk_test, pI_id, has_referent);
                }
            }
        }
}

void Explanation_Based_Chunker::add_explanation_to_condition(rete_node* node,
                                        condition* cond,
                                        node_varnames* nvn,
                                        uint64_t pI_id,
                                        AddAdditionalTestsMode additional_tests)
{
    if (additional_tests == JUST_INEQUALITIES)
    {
        add_explanation_to_RL_condition(node, cond, nvn, pI_id, additional_tests);
        return;
    }

    if (!ebc_settings[SETTING_EBC_LEARNING_ON]) return;

    rete_test* rt = node->b.posneg.other_tests;

    /* --- Store original referent information.  Note that sometimes the
     *     original referent equality will be stored in the beta nodes extra tests
     *     data structure rather than the alpha memory --- */
    alpha_mem* am;
    am = node->b.posneg.alpha_mem_;

    dprint(DT_ADD_EXPLANATION_TRACE, "add_constraints_and_identities called for %s.\n%l\n",  thisAgent->newly_created_instantiations->prod_name->sc->name, cond);

    if (nvn)
    {
        add_varname_identity_to_test(thisAgent, nvn->data.fields.id_varnames, cond->data.tests.id_test, pI_id);
        add_varname_identity_to_test(thisAgent, nvn->data.fields.attr_varnames, cond->data.tests.attr_test, pI_id);
        add_varname_identity_to_test(thisAgent, nvn->data.fields.value_varnames, cond->data.tests.value_test, pI_id);
    }

    /* --- on hashed nodes, add equality test for the hash function --- */
    if ((node->node_type == MP_BNODE) || (node->node_type == NEGATIVE_BNODE))
    {
        add_identity_to_id_test(cond,
            node->left_hash_loc_field_num,
            node->left_hash_loc_levels_up);
    }
    else if (node->node_type == POSITIVE_BNODE)
    {
        add_identity_to_id_test(cond,
            node->parent->left_hash_loc_field_num,
            node->parent->left_hash_loc_levels_up);
    }

    /* -- Now process any additional relational test -- */
    test chunk_test = NULL;
    TestType test_type;
    bool has_referent;
    for (; rt != NIL; rt = rt->next)
    {
        chunk_test = NULL;
        has_referent = true;
        if (rt->type ==DISJUNCTION_RETE_TEST)
        {
            chunk_test = make_test(thisAgent, NIL, DISJUNCTION_TEST);
            chunk_test->data.disjunction_list = thisAgent->symbolManager->copy_symbol_list_adding_references(rt->data.disjunction_list);
            has_referent = false;
        } else {
            if (test_is_constant_relational_test(rt->type))
            {
                test_type = relational_test_type_to_test_type(kind_of_relational_test(rt->type));
                    chunk_test = make_test(thisAgent, rt->data.constant_referent, test_type);
            }
            else if (test_is_variable_relational_test(rt->type))
            {
                test_type = relational_test_type_to_test_type(kind_of_relational_test(rt->type));
                test ref_test = var_test_bound_in_reconstructed_conds(thisAgent, cond,
                    rt->data.variable_referent.field_num,
                    rt->data.variable_referent.levels_up);
                chunk_test = make_test(thisAgent, ref_test->data.referent, test_type);
                chunk_test->identity = ref_test->identity;
            }
        }
        if (chunk_test)
        {
            if (rt->right_field_num == 0)
            {
                add_constraint_to_explanation(&(cond->data.tests.id_test), chunk_test, pI_id, has_referent);
            }
            else if (rt->right_field_num == 1)
            {
                add_constraint_to_explanation(&(cond->data.tests.attr_test), chunk_test, pI_id, has_referent);
            }
            else
            {
                add_constraint_to_explanation(&(cond->data.tests.value_test), chunk_test, pI_id, has_referent);
            }
        }
    }
    dprint(DT_ADD_EXPLANATION_TRACE, "Final test after add_constraints_and_identities: %l\n", cond);
}

/* -- This function adds a constraint test from the rete's other tests lists.  This function
 *    is odd because, although in most cases the rete only uses the other-tests list for
 *    tests that place additional constraints on the value, sometimes, for as of yet not
 *    understood reason, it can also store an equality test that contains the original rule
 *    symbol, which is normally stored in the varlist for the node.  This seems to happen for
 *    certain conditions that are linked more than one or two levels away from the state.  We
 *    need that symbol to assign an identity, so we look for that case here.
 * -- */

void Explanation_Based_Chunker::add_constraint_to_explanation(test* dest_test_address, test new_test, uint64_t pI_id, bool has_referent)
{
    if (has_referent)
    {
        // Handle case where relational test is equality test
        if ((*dest_test_address) && new_test && (new_test->type == EQUALITY_TEST))
        {
            test destination = *dest_test_address;
            if (destination->type == EQUALITY_TEST)
            {
                if (destination->data.referent == new_test->data.referent)
                {
                    if (!destination->identity && new_test->identity)
                    {
                        /* This is the special case */
                        destination->identity = new_test->identity;
                        deallocate_test(thisAgent, new_test);
                        return;
                    }
                    else
                    {
                        /* Identical referents and possibly identical originals.  Ignore. */
                        return;
                    }
                } // else different referents and should be added as new test
            }
            else if (destination->type == CONJUNCTIVE_TEST)
            {
                if (destination->eq_test)
                {
                    if (destination->eq_test->data.referent == new_test->data.referent)
                    {
                        if (!destination->eq_test->identity && new_test->identity)
                        {
                            /* This is the special case */
                            destination->eq_test->identity = new_test->identity;
                            deallocate_test(thisAgent, new_test);
                            return;
                        }
                    }
                }
            }
        }
    }
    add_test(thisAgent, dest_test_address, new_test);
}

void Explanation_Based_Chunker::update_remaining_identity_sets_in_test(test t, instantiation* pInst)
{
    cons* c;
    switch (t->type)
        {
            case GOAL_ID_TEST:
            case IMPASSE_ID_TEST:
            case SMEM_LINK_UNARY_TEST:
            case SMEM_LINK_UNARY_NOT_TEST:
            case DISJUNCTION_TEST:
                break;
            case CONJUNCTIVE_TEST:
                for (c = t->data.conjunct_list; c != NIL; c = c->rest)
                {
                    update_remaining_identity_sets_in_test(static_cast<test>(c->first), pInst);
                }
                break;
            default:
                if (!t->identity_set) t->identity_set = t->identity ?  get_identity(t->identity) : t->identity;
                break;
        }
}

void Explanation_Based_Chunker::update_remaining_identity_sets_in_condlist(condition* pCondTop, instantiation* pInst)
{
    condition* pCond;

    for (pCond = pCondTop; pCond != NIL; pCond = pCond->next)
    {
        if (pCond->type != CONJUNCTIVE_NEGATION_CONDITION)
        {
            update_remaining_identity_sets_in_test(pCond->data.tests.id_test, pInst);
            update_remaining_identity_sets_in_test(pCond->data.tests.attr_test, pInst);
            update_remaining_identity_sets_in_test(pCond->data.tests.value_test, pInst);
        } else {
            update_remaining_identity_sets_in_condlist(pCond->data.ncc.top, pInst);
        }
    }
}
