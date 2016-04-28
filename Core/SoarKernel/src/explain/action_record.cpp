#include "action_record.h"

#include "explain.h"
#include "agent.h"
#include "condition.h"
#include "debug.h"
#include "instantiation.h"
#include "preference.h"
#include "print.h"
#include "production.h"
#include "rete.h"
#include "rhs.h"
#include "symbol.h"
#include "test.h"
#include "output_manager.h"
#include "visualize.h"
#include "working_memory.h"

action_record::action_record(agent* myAgent, preference* pPref, action* pAction, uint64_t pActionID)
{
    thisAgent               = myAgent;
    actionID                = pActionID;
    instantiated_pref       = shallow_copy_preference(thisAgent, pPref);
    original_pref           = pPref;
    if (pAction)
    {
        variablized_action = copy_action(thisAgent, pAction);
    } else {
        variablized_action = NULL;
    }
    identities_used = NULL;
    dprint(DT_EXPLAIN_CONDS, "   Created action record a%u for pref %p (%r ^%r %r), [act %a]", pActionID, pPref, pPref->rhs_funcs.id, pPref->rhs_funcs.attr, pPref->rhs_funcs.value, pAction);
}

action_record::~action_record()
{
    dprint(DT_EXPLAIN_CONDS, "   Deleting action record a%u for: %p\n", actionID, instantiated_pref);
    deallocate_preference(thisAgent, instantiated_pref);
    deallocate_action_list(thisAgent, variablized_action);
    if (identities_used)
    {
        delete identities_used;
    }
    dprint(DT_EXPLAIN_CONDS, "   Done deleting action record a%u\n", actionID);
}

void add_all_identities_in_rhs_value(agent* thisAgent, rhs_value rv, id_set* pIDSet)
{
    list* fl;
    cons* c;
    Symbol* sym;

    if (rhs_value_is_symbol(rv))
    {
        /* --- ordinary values (i.e., symbols) --- */
        sym = rhs_value_to_symbol(rv);
        if (sym->is_variable())
        {
            dprint(DT_EXPLAIN_PATHS, "Adding identity %u from rhs value to id set...\n", rhs_value_to_o_id(rv));
            pIDSet->insert(rhs_value_to_o_id(rv));
        }
    }
    else
    {
        /* --- function calls --- */
        fl = rhs_value_to_funcall_list(rv);
        for (c = fl->rest; c != NIL; c = c->rest)
        {
            add_all_identities_in_rhs_value(thisAgent, static_cast<char*>(c->first), pIDSet);
        }
    }
}

void add_all_identities_in_action(agent* thisAgent, action* a, id_set* pIDSet)
{
    Symbol* id;

    if (a->type == MAKE_ACTION)
    {
        /* --- ordinary make actions --- */
        id = rhs_value_to_symbol(a->id);
        if (id->is_variable())
        {
            dprint(DT_EXPLAIN_PATHS, "Adding identity %u from rhs action id to id set...\n", rhs_value_to_o_id(a->id));
            pIDSet->insert(rhs_value_to_o_id(a->id));
        }
        add_all_identities_in_rhs_value(thisAgent, a->attr, pIDSet);
        add_all_identities_in_rhs_value(thisAgent, a->value, pIDSet);
        if (preference_is_binary(a->preference_type))
        {
            add_all_identities_in_rhs_value(thisAgent, a->referent, pIDSet);
        }
    }
    else
    {
        /* --- function call actions --- */
        add_all_identities_in_rhs_value(thisAgent, a->value, pIDSet);
    }
}

id_set* action_record::get_identities()
{
    if (!identities_used)
    {
        identities_used = new id_set();
        add_all_identities_in_action(thisAgent, variablized_action, identities_used);
    }

    return identities_used;
}

void action_record::viz_rhs_value(const rhs_value pRHS_value, const rhs_value pRHS_variablized_value, uint64_t pID)
{
    std::string tempString;
    tempString = "";
    thisAgent->outputManager->set_print_test_format(true, false);
    thisAgent->outputManager->rhs_value_to_string(thisAgent, pRHS_value, tempString, NULL, NULL);
    thisAgent->visualizer->graphviz_output += tempString;
    if (pRHS_variablized_value)
    {
        tempString = "";
        thisAgent->outputManager->set_print_test_format(false, true);
        thisAgent->outputManager->rhs_value_to_string(thisAgent, pRHS_variablized_value, tempString, NULL, NULL);
        if (!tempString.empty())
        {
            thisAgent->visualizer->graphviz_output += tempString;
        }
    } else if (pID) {
        thisAgent->visualizer->graphviz_output += " (";
        thisAgent->visualizer->graphviz_output += std::to_string(pID);
        thisAgent->visualizer->graphviz_output += ')';
    }
}

void action_record::viz_action_list(agent* thisAgent, action_record_list* pActionRecords, production* pOriginalRule, action* pRhs)
{
    if (pActionRecords->empty())
    {
        thisAgent->visualizer->viz_text_record("Empty RHS");
    }
    else
    {
        action_record* lAction;
        condition* top = NULL, *bottom = NULL;
        action* rhs;
        int lActionCount = 0;
        thisAgent->outputManager->set_print_indents("");
        thisAgent->outputManager->set_print_test_format(true, false);
        if (thisAgent->explanationLogger->print_explanation_trace)
        {
            /* We use pRhs to deallocate actions at end, and rhs to iterate through actions */
            if (pRhs)
            {
                rhs = pRhs;
            } else {
                if (!pOriginalRule || !pOriginalRule->p_node)
                {
                    thisAgent->visualizer->viz_text_record("No RETE rule");
                    return;
                } else {
                    p_node_to_conditions_and_rhs(thisAgent, pOriginalRule->p_node, NIL, NIL, &top, &bottom, &rhs);
                    pRhs = rhs;
                }
            }
        }
        size_t lNumRecords = pActionRecords->size();
        for (action_record_list::iterator it = pActionRecords->begin(); it != pActionRecords->end(); it++)
        {
            lAction = (*it);
            ++lActionCount;
            if (lActionCount <= lNumRecords)
            {
                thisAgent->visualizer->viz_endl();
            }
            if (!thisAgent->explanationLogger->print_explanation_trace)
            {
                lAction->viz_preference();
            } else {
                lAction->viz_action(rhs);
                rhs = rhs->next;
            }
        }
        thisAgent->visualizer->graphviz_output += "\n";
        if (thisAgent->explanationLogger->print_explanation_trace)
        {
            /* If top exists, we generated conditions here and must deallocate. */
            if (pRhs) deallocate_action_list(thisAgent, pRhs);
            if (top) deallocate_condition_list(thisAgent, top);
        }
        thisAgent->outputManager->clear_print_test_format();
    }
}
void action_record::viz_preference()
{
    thisAgent->visualizer->viz_record_start();
    thisAgent->visualizer->viz_table_element_start(actionID, 'a', true);
    thisAgent->outputManager->sprinta_sf(thisAgent, thisAgent->visualizer->graphviz_output, "%y", instantiated_pref->id);
    thisAgent->visualizer->viz_table_element_end();
    thisAgent->visualizer->viz_table_element_start();
    thisAgent->outputManager->sprinta_sf(thisAgent, thisAgent->visualizer->graphviz_output, "%y", instantiated_pref->attr);
    thisAgent->visualizer->viz_table_element_end();

    if (preference_is_binary(instantiated_pref->type))
    {
        thisAgent->visualizer->viz_table_element_start();
        thisAgent->outputManager->sprinta_sf(thisAgent, thisAgent->visualizer->graphviz_output, "%y", instantiated_pref->value);
        thisAgent->visualizer->viz_table_element_end();
        thisAgent->visualizer->viz_table_element_start(actionID, 'a', false);
        thisAgent->outputManager->sprinta_sf(thisAgent, thisAgent->visualizer->graphviz_output, " %c %y", preference_to_char(instantiated_pref->type), instantiated_pref->referent);
        thisAgent->visualizer->viz_table_element_end();
    } else {
        thisAgent->visualizer->viz_table_element_start(actionID, 'a', false);
        thisAgent->outputManager->sprinta_sf(thisAgent, thisAgent->visualizer->graphviz_output, " %y %c", instantiated_pref->value, preference_to_char(instantiated_pref->type));
        thisAgent->visualizer->viz_table_element_end();
    }
    thisAgent->visualizer->viz_record_end();
}

void action_record::viz_action(action* pAction)
{
    std::string tempString;

    if (pAction->type == FUNCALL_ACTION)
    {
        thisAgent->visualizer->viz_record_start();
        thisAgent->visualizer->viz_table_element_start(actionID, 'a', true);
        thisAgent->visualizer->graphviz_output += "RHS Funcall";
        thisAgent->visualizer->viz_table_element_end();
        thisAgent->visualizer->viz_table_element_start(actionID, 'a', false);
        tempString = "";
        thisAgent->outputManager->rhs_value_to_string(thisAgent, pAction->value, tempString, NULL, NULL);
        thisAgent->visualizer->graphviz_output += tempString;
        thisAgent->visualizer->viz_table_element_end();
        thisAgent->visualizer->viz_record_end();
    } else {
        thisAgent->visualizer->viz_record_start();
        thisAgent->visualizer->viz_table_element_start(actionID, 'a', true);
        viz_rhs_value(pAction->id, (variablized_action ? variablized_action->id : NULL), instantiated_pref->o_ids.id);
        thisAgent->visualizer->viz_table_element_end();
        thisAgent->visualizer->viz_table_element_start();
        viz_rhs_value(pAction->attr, (variablized_action ? variablized_action->attr : NULL), instantiated_pref->o_ids.attr);
        thisAgent->visualizer->viz_table_element_end();
        if (pAction->referent)
        {
            thisAgent->visualizer->viz_table_element_start();
            viz_rhs_value(pAction->value, (variablized_action ? variablized_action->value : NULL), instantiated_pref->o_ids.value);
            thisAgent->visualizer->viz_table_element_end();
            thisAgent->visualizer->viz_table_element_start(actionID, 'a', false);
            thisAgent->visualizer->graphviz_output += preference_to_char(pAction->preference_type);
            viz_rhs_value(pAction->referent, (variablized_action ? variablized_action->referent : NULL), instantiated_pref->o_ids.referent);
            thisAgent->visualizer->viz_table_element_end();
        } else {
            thisAgent->visualizer->viz_table_element_start(actionID, 'a', false);
            viz_rhs_value(pAction->value, (variablized_action ? variablized_action->value : NULL), instantiated_pref->o_ids.value);
            thisAgent->visualizer->graphviz_output += ' ';
            thisAgent->visualizer->graphviz_output += preference_to_char(pAction->preference_type);
            thisAgent->visualizer->viz_table_element_end();
        }
        thisAgent->visualizer->viz_record_end();
    }
    tempString.clear();
}
