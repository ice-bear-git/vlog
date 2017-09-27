#include <vlog/qsqr.h>
#include <vlog/concepts.h>
#include <vlog/bindingstable.h>
#include <vlog/ruleexecutor.h>
#include <trident/model/table.h>
#include <trident/iterators/arrayitr.h>

#include <cstring>
#include <unordered_map>

BindingsTable *QSQR::getInputTable(const Predicate pred) {
    //raiseIfExpired();
    BindingsTable **table = inputs[pred.getId()];
    if (table == NULL) {
        const uint8_t maxAdornments = (uint8_t)pow(2, pred.getCardinality());
        table = new BindingsTable*[maxAdornments];
        memset(table, 0, sizeof(BindingsTable*)*maxAdornments);
        inputs[pred.getId()] = table;
        sizePreds[pred.getId()] = maxAdornments;
    }
    if (table[pred.getAdornment()] == NULL) {
        table[pred.getAdornment()] = new BindingsTable(pred.getCardinality(), pred.getAdornment());
    }
    return table[pred.getAdornment()];
}

BindingsTable *QSQR::getAnswerTable(const Predicate pred, uint8_t adornment) {
    //raiseIfExpired();
    BindingsTable **table = answers[pred.getId()];
    if (table == NULL) {
        const uint8_t maxAdornments = (uint8_t)pow(2, pred.getCardinality());
        table = new BindingsTable*[maxAdornments];
        memset(table, 0, sizeof(BindingsTable*)*maxAdornments);
        answers[pred.getId()] = table;
        sizePreds[pred.getId()] = maxAdornments;
    }
    if (table[adornment] == NULL) {
        table[adornment] = new BindingsTable(pred.getCardinality());
    }
    return table[adornment];
}

QSQR::~QSQR() {
    for (PredId_t i = 0; i < MAX_NPREDS; ++i) {
        if (inputs[i] != NULL) {
            for (uint32_t j = 0; j < sizePreds[i]; ++j)
                delete inputs[i][j];
            delete[] inputs[i];
        }
        if (answers[i] != NULL) {
            for (uint32_t j = 0; j < sizePreds[i]; ++j)
                delete answers[i][j];
            delete[] answers[i];
        }
        if (rules[i] != NULL) {
            for (int j = 0; j < sizePreds[i]; ++j) {
                if (rules[i][j] != NULL) {
                    for (int m = 0; m < program->getAllRulesByPredicate(i)->size(); ++m) {
                        delete rules[i][j][m];
                    }
                    delete[] rules[i][j];
                }
            }
            delete[] rules[i];
        }
    }
}

void QSQR::deallocateAllRules() {
    for (PredId_t i = 0; i < MAX_NPREDS; ++i) {
        if (rules[i] != NULL) {
            for (int j = 0; j < sizePreds[i]; ++j) {
                if (rules[i][j] != NULL) {
                    for (int m = 0; m < program->getAllRulesByPredicate(i)->size(); ++m) {
                        delete rules[i][j][m];
                    }
                    delete[] rules[i][j];
                }
            }
            delete[] rules[i];
            rules[i] = NULL;
        }
    }
}

size_t QSQR::calculateAllAnswers() {
    size_t total = 0;
    for (int i = 0; i < MAX_NPREDS; ++i) {
        if (answers[i] != NULL) {
            for (uint32_t j = 0; j < sizePreds[i]; ++j) {
                if (answers[i][j] != NULL) {
                    total += answers[i][j]->getNTuples();
                }
            }
        }
    }
    return total;
}

void QSQR::cleanAllInputs() {
    for (int i = 0; i < MAX_NPREDS; ++i) {
        if (inputs[i] != NULL) {
            for (uint32_t j = 0; j < sizePreds[i]; ++j) {
                if (inputs[i][j] != NULL) {
                    inputs[i][j]->clear();
                }
            }
        }
    }
}

void QSQR::createRules(Predicate &pred) {
    //check if the adorned rules are created. If not, then create them.
    if (rules[pred.getId()] == NULL) {
        const uint16_t maxAdornments = (uint16_t)pow(2, pred.getCardinality());
        rules[pred.getId()] = new RuleExecutor**[maxAdornments];
        memset(rules[pred.getId()], 0, sizeof(RuleExecutor**)*maxAdornments);
    }

    if (rules[pred.getId()][pred.getAdornment()] == NULL) {
        std::vector<Rule> *r = program->getAllRulesByPredicate(pred.getId());
        // BOOST_LOG_TRIVIAL(debug) << "createRules for predicate " << pred.getId() << ", adornment = " << pred.getAdornment() << ", r->size = " << r->size();
        rules[pred.getId()][pred.getAdornment()] =
            new RuleExecutor*[r->size()];
        int m = 0;
        for (std::vector<Rule>::iterator itr =
                    r->begin(); itr != r->end(); ++itr) {
            rules[pred.getId()][pred.getAdornment()][m] =
                new RuleExecutor(*itr, pred.getAdornment(), program, layer);
            m++;
        }
    }
}

size_t QSQR::estimate(int depth, Predicate &pred, BindingsTable *inputTable/*, size_t offsetInput*/, int &countRules, int &countIntQueries, std::vector<Rule> &execRules) {

    if (depth <= 0) {
	// Question is: what to return here. Assume no results? Assume 1 result?
	return 1;
    }
    countIntQueries++;
    createRules(pred);
    std::vector<size_t> outputs;
    size_t output = 0; 
    for (int i = 0; i < program->getAllRulesByPredicate(pred.getId())->size(); ++i) {
        RuleExecutor *exec = rules[pred.getId()][pred.getAdornment()][i];
        size_t r = exec->estimate(depth - 1, inputTable/*, offsetInput*/, this, layer,i, countRules, countIntQueries,execRules);
	//BOOST_LOG_TRIVIAL(info) << "Rule" << i << "\n" << "counter" << dCounter;
        if (r != 0) {
	    // if (depth > 0 || r <= 10) {
	        output += r;
	    /*
            } else {
                // Somewhat silly duplicate detection heuristic ...
                bool found = false;
                for (int i = 0; i < outputs.size(); i++) {
                    if (outputs[i] == r) {
                        BOOST_LOG_TRIVIAL(debug) << "Ignoring " << r << " results";
                        // Assume it is the same answer ...
                        found = true;
                        break;
                    }
                }
                if (! found) {
                    outputs.push_back(r);
                    output += r;
                }
            }
            */
	}
    }
    return output;
}

void QSQR::estimateQuery(Metrics &metrics, int depth, Literal &l, std::vector<Rule> &execRules) {

    BOOST_LOG_TRIVIAL(debug) << "Literal " << l.tostring(program, &layer) << ", depth = " << depth;

    if (depth <= 0) {
	metrics.estimate++;
	metrics.intermediateResults++;
	metrics.cost++;
        return;
    }
 
    Predicate pred = l.getPredicate();
    metrics.countIntermediateQueries++;

    if (pred.getType() == EDB) {
	size_t result = layer.estimateCardinality(l);
	BOOST_LOG_TRIVIAL(debug) << "EDB: estimate = " << result;
	metrics.estimate += result;
	metrics.intermediateResults += result;
	metrics.cost += result;
    }

    std::vector<Rule> *r = program->getAllRulesByPredicate(pred.getId());
    for (std::vector<Rule>::iterator itr =
	    r->begin(); itr != r->end(); ++itr) {
	Literal head = itr->getHead();
	Substitution substitutions[SIZETUPLE];
	int nSubs = Literal::subsumes(substitutions, head, l);
	if (nSubs < 0) {
	    continue;
	}

	Metrics m;
	memset(&m, 0, sizeof(Metrics));

	BOOST_LOG_TRIVIAL(debug) << "Matches with rule " << itr->tostring(program, &layer);

	// Remove replacements of variables with variables ...
	int filterednSubs = 0;
	Substitution filteredSubstitutions[SIZETUPLE];
	for (int i = 0; i < nSubs; i++) {
	    if (! substitutions[i].destination.isVariable()) {
		 filteredSubstitutions[filterednSubs++] = substitutions[i];
	    }
	}

        estimateRule(m, depth - 1, *itr, filteredSubstitutions, filterednSubs, execRules);
	metrics.estimate += m.estimate;
	metrics.intermediateResults += m.intermediateResults;
	metrics.countRules += m.countRules;
	metrics.countIntermediateQueries += m.countIntermediateQueries;
	metrics.cost += m.cost;
    }
}

void QSQR::estimateRule(Metrics &metrics, int depth, Rule &rule, Substitution *subs, int nSubs, std::vector<Rule> &execRules) {
    bool exists = false;
    for (std::vector<Rule>::iterator itr = execRules.begin(); itr != execRules.end() && ! exists; itr++) {
	exists = itr->getHead() == rule.getHead() && itr->getBody() == rule.getBody();
    }
    if (!exists) {
	BOOST_LOG_TRIVIAL(debug) << "Adding rule " << rule.tostring(program, &layer);
	execRules.push_back(rule);
    }
    metrics.countRules++;
    std::vector<Literal> body = rule.getBody();
    Literal substitutedHead = rule.getHead().substitutes(subs, nSubs);
    std::vector<uint8_t> headVars = substitutedHead.getAllVars();
    std::vector<uint8_t> allVars;
    bool noAnswers = false;
    for (std::vector<Literal>::const_iterator itr = body.begin(); itr != body.end(); ++itr) {
	Metrics m;
	memset(&m, 0, sizeof(Metrics));
	Literal substituted = itr->substitutes(subs, nSubs);
	BOOST_LOG_TRIVIAL(debug) << "Substituted literal = " << substituted.tostring(program, &layer);
	estimateQuery(m, depth, substituted, execRules);
	metrics.countRules += m.countRules;
	metrics.countIntermediateQueries += m.countIntermediateQueries;
	metrics.cost += m.cost;
	metrics.cost += m.intermediateResults * metrics.intermediateResults;

	if (m.estimate == 0) {
	    // Should only be the case when we are sure...
	    noAnswers = true;
	    break;
	}

	// Check for filtering join ...
	std::vector<uint8_t> newAllVars = substituted.getNewVars(allVars);
	bool contribution = newAllVars.size() > 0;
	for (int i = 0; i < newAllVars.size(); i++) {
	    allVars.push_back(newAllVars[i]);
	}
	if (contribution) {
	    metrics.intermediateResults += m.intermediateResults;
	    // check if the literal has variables in common with the LHS. If not, no contribution to estimate?
	    std::vector<uint8_t> shared = substituted.getSharedVars(headVars);
	    if (shared.size() == 0) {
		contribution = false;
	    }
	}
	if (contribution) {
	    metrics.estimate += m.estimate;
	}
    }
    if (noAnswers) {
	metrics.estimate = 0;
    } else if (metrics.estimate == 0) {
	metrics.estimate = 1;
    }
}


void QSQR::evaluate(Predicate &pred, BindingsTable *inputTable,
                    size_t offsetInput, bool repeat) {

    // BOOST_LOG_TRIVIAL(debug) << "QSQR:evaluate predicate = " << (int) pred.getId()  << ", nTUples = " << inputTable->getNTuples() << ", offsetInput = " << offsetInput;

#ifdef RECURSIVE_QSQR
    size_t totalAnswers;
    bool shouldRepeat = false;

    //Calculate all answers produced so far.
    totalAnswers = calculateAllAnswers();
    do {
        //Create rules
        createRules(pred);
        for (int i = 0; i < program->getAllRulesByPredicate(pred.getId())->size(); ++i) {
            RuleExecutor *exec = rules[pred.getId()][pred.getAdornment()][i];
            exec->evaluate(inputTable, offsetInput, this, layer);
        }

        //Repeat the execution if new answers were being produced
        //Clean up all the inputs relations to ensure completeness.
        size_t newTotalAnswers = calculateAllAnswers();

        shouldRepeat = newTotalAnswers > totalAnswers;
        totalAnswers = newTotalAnswers;
    } while (repeat && shouldRepeat);
    // BOOST_LOG_TRIVIAL(debug) << "QSQR: finished execution of query";
#else
    createRules(pred);
    size_t sz = program->getAllRulesByPredicate(pred.getId())->size();
    if (sz > 0) {
	QSQR_Task task(QSQR_TaskType::QUERY, pred);
	task.currentRuleIndex = 1;
	task.inputTable = inputTable;
	task.offsetInput = offsetInput;
	task.repeat = repeat;
	task.totalAnswers = calculateAllAnswers();
	pushTask(task);
        RuleExecutor *exec = rules[pred.getId()][pred.getAdornment()][0];
        exec->evaluate(inputTable, offsetInput, this, layer);
    }
#endif
}

#ifndef RECURSIVE_QSQR
void QSQR::processTask(QSQR_Task &task) {
    switch (task.type) {
    case QUERY: {
	size_t sz = program->getAllRulesByPredicate(task.pred.getId())->size();
	if (task.currentRuleIndex < sz) {
            //Execute the next rule
	    QSQR_Task newTask(QSQR_TaskType::QUERY, task.pred);
	    newTask.currentRuleIndex = task.currentRuleIndex + 1;
	    newTask.inputTable = task.inputTable;
	    newTask.offsetInput = task.offsetInput;
	    newTask.repeat = task.repeat;
	    newTask.totalAnswers = task.totalAnswers;
	    pushTask(newTask);
            // BOOST_LOG_TRIVIAL(debug) << "pushed new task QUERY, totalAnswers = " << newTask.totalAnswers;
            RuleExecutor *exec = rules[task.pred.getId()]
                                 [task.pred.getAdornment()][task.currentRuleIndex];
            exec->evaluate(task.inputTable, task.offsetInput, this, layer);
        } else {
            size_t newAnswers = calculateAllAnswers();
            if (task.repeat && newAnswers > task.totalAnswers) {
                createRules(task.pred);
		QSQR_Task newTask(QSQR_TaskType::QUERY, task.pred);
		newTask.currentRuleIndex = 1;
		newTask.inputTable = task.inputTable;
		newTask.offsetInput = task.offsetInput;
		newTask.repeat = task.repeat;
		//newTask.shouldRepeat = false;
		newTask.totalAnswers = newAnswers;
		pushTask(newTask);
                // BOOST_LOG_TRIVIAL(debug) << "pushed new task QUERY(0), totalAnswers = " << newTask.totalAnswers;
                RuleExecutor *exec = rules[task.pred.getId()]
                                     [task.pred.getAdornment()][0];
                exec->evaluate(task.inputTable, task.offsetInput, this, layer);
            }
        }
        break;
    }
    case RULE:
    case RULE_QUERY:
        RuleExecutor *exec = task.executor;
        exec->processTask(&task);
        break;
    }
}
#endif

TupleTable *QSQR::evaluateQuery(int evaluateOrEstimate, QSQQuery *query,
                                std::vector<uint8_t> *posJoins,
                                std::vector<Term_t> *possibleValuesJoins,
                                bool returnOnlyVars,/*,
                                const Timeout * const timeout*/int *cR, int *cIQ, int *cUR) {

    Predicate pred = query->getLiteral()->getPredicate();
    int countRules =0;
    int countIntQueries =0;
    std::vector<Rule> execRules; 
    if (pred.getType() == EDB) {
        if (evaluateOrEstimate == QSQR_EVAL) {
            uint8_t nvars = query->getLiteral()->getNVars();
            TupleTable *outputTable = new TupleTable(nvars);
            layer.query(query, outputTable, posJoins, possibleValuesJoins);
            return outputTable;
        } else {
            TupleTable *output = new TupleTable(1);
            uint64_t card = layer.estimateCardinality(*query->getLiteral());
            output->addRow(&card);
            return output;
        }
    } else {
        cleanAllInputs();
        size_t totalAnswers, newTotalAnswers;
        bool shouldRepeat = false;
        uint8_t adornment = pred.getAdornment();
        do {
            BindingsTable *inputTable;
            if (posJoins != NULL) {
                //Modify the adornment of the pred. Set constant values that were
                //set as variables
                for (uint8_t i = 0; i < posJoins->size(); ++i) {
                    adornment = Predicate::changeVarToConstInAdornment(adornment,
                                posJoins->at(i));
                }
                Predicate pred2 = Predicate(pred.getId(), adornment, pred.getType(),
                                            pred.getCardinality());
                inputTable = getInputTable(pred2);

                assert(possibleValuesJoins != NULL);
                assert(query->getLiteral()->getTupleSize() <= 3);
                Term_t tuple[3];
                //Fill the tuple with the content of the query
                VTuple t = query->getLiteral()->getTuple();
                for (int i = 0; i < t.getSize(); ++i) {
                    tuple[i] = t.get(i).getValue();
                }

                //Add all possible literals
                std::vector<Term_t>::iterator itr = possibleValuesJoins->begin();
                while (itr != possibleValuesJoins->end()) {
                    //raiseIfExpired();
                    for (uint8_t j = 0; j < posJoins->size(); ++j) {
                        tuple[posJoins->at(j)] = *itr;
                        itr++;
                    }
                    inputTable->addTuple(tuple);
                }

                //raiseIfExpired();
                totalAnswers = calculateAllAnswers();

                if (evaluateOrEstimate == QSQR_EVAL) {
                    evaluate(pred2, inputTable, 0, false);
#ifndef RECURSIVE_QSQR
                    //evaluate in this case is not recursive. Process the tasks
                    //until the queue is empty
                    while (tasks.size() > 0) {
                        // BOOST_LOG_TRIVIAL(debug) << "Task size=" << tasks.size();
                        QSQR_Task task = tasks.back();
                        tasks.pop_back();
                        processTask(task);
                    }
#endif

                } else {
		    TupleTable *output = new TupleTable(1);
                    uint64_t est = estimate(4, pred2, inputTable/*, 0*/,countRules,countIntQueries,execRules);
		    // Incorporate size of possible join values?
		    // Useless, I think, because in the planning phase, we don't actually have more than
		    // one possiblevaluesjoin. --Ceriel
		    est = est + (est * (possibleValuesJoins->size() / posJoins->size() - 1)) / 10; 
                    output->addRow(&est);
		    if (cR != NULL) {
			*cR = countRules;
		    }
		    if (cIQ != NULL) {
			*cIQ = countIntQueries;
		    }
		    if (cUR != NULL) {
			*cUR = execRules.size();
		    }
                    return output;
                }
            } else {
                inputTable = getInputTable(pred);
                inputTable->addTuple(query->getLiteral());
                if (evaluateOrEstimate == QSQR_EVAL) {
                    totalAnswers = calculateAllAnswers();
                    evaluate(pred, inputTable, 0, false);
#ifndef RECURSIVE_QSQR
                    //evaluate in this case is not recursive. Process the tasks
                    //until the queue is empty
                    while (tasks.size() > 0) {
                        //BOOST_LOG_TRIVIAL(debug) << "Task size=" << tasks.size();
                        QSQR_Task task = tasks.back();
                        tasks.pop_back();
                        processTask(task);
                    }
#endif
		
                } else { //ESTIMATE
                    TupleTable *output = new TupleTable(1);
		    uint64_t est = estimate(4, pred, inputTable/*, 0*/,countRules, countIntQueries,execRules);
		    BOOST_LOG_TRIVIAL(debug) << "No of Rules Executed(depth of 3) : " << countRules;
		    BOOST_LOG_TRIVIAL(debug) << "No of Intermediate Queries" << countIntQueries;	
		    BOOST_LOG_TRIVIAL(debug) << "No of Unique Rules" << execRules.size();
		    output->addRow(&est);
		    if (cR != NULL) {
			*cR = countRules;
		    }
		    if (cIQ != NULL) {
			*cIQ = countIntQueries;
		    }
		    if (cUR != NULL) {
			*cUR = execRules.size();
		    }
                    return output;
                }
            }

            newTotalAnswers = calculateAllAnswers();
            shouldRepeat = newTotalAnswers > totalAnswers;
            if (shouldRepeat) {
                cleanAllInputs();
            }
        } while (shouldRepeat);

        const Literal *l = query->getLiteral();
        BindingsTable *answer = getAnswerTable(l->getPredicate(), adornment);

	if (cR != NULL) {
	    *cR = countRules;
	}
	if (cIQ != NULL) {
	    *cIQ = countIntQueries;
	}
	if (cUR != NULL) {
	    *cUR = execRules.size();
	}
        if (returnOnlyVars) {
            //raiseIfExpired();
            return answer->projectAndFilter(*l, posJoins, possibleValuesJoins);
        } else {
            //raiseIfExpired();
            return answer->filter(*l, posJoins, possibleValuesJoins);
        }
    }
}
