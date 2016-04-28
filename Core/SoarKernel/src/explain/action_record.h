/*
 * action_record.h
 *
 *  Created on: Apr 22, 2016
 *      Author: mazzin
 */

#ifndef CORE_SOARKERNEL_SRC_EXPLAIN_ACTION_RECORD_H_
#define CORE_SOARKERNEL_SRC_EXPLAIN_ACTION_RECORD_H_

#include "kernel.h"
#include "stl_typedefs.h"

class action_record
{
        friend class Explanation_Logger;

    public:
        action_record(agent* myAgent, preference* pPref, action* pAction, uint64_t pActionID);
        ~action_record();

        uint64_t                get_actionID()   { return actionID; };
        id_set*                 get_identities();

        void                    viz_rhs_value(const rhs_value pRHS_value, const rhs_value pRHS_variablized_value, uint64_t pID);
        void                    viz_action(action* pAction);
        void                    viz_preference();
        /* Action lists are common to chunk records and instantiation records, but don't have a class to themselves */
        static void             viz_action_list(agent* thisAgent, action_record_list* pActionRecords, production* pOriginalRule, action* pRhs = NULL);

        preference*             original_pref;          // Only used when building explain records

    private:
        agent* thisAgent;
        preference*             instantiated_pref;
        action*                 variablized_action;
        id_set*                 identities_used;
        uint64_t                actionID;
};

#endif /* CORE_SOARKERNEL_SRC_EXPLAIN_ACTION_RECORD_H_ */
