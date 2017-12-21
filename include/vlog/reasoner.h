#ifndef REASONER_H
#define REASONER_H

#include <vlog/concepts.h>
#include <vlog/edb.h>
#include <vlog/fctable.h>
#include <vlog/seminaiver.h>

#include <trident/kb/kb.h>
#include <trident/kb/querier.h>

#include <trident/sparql/query.h>

#define QUERY_MAT 0
#define QUERY_ONDEM 1

typedef enum {TOPDOWN, MAGIC} ReasoningMode;
typedef enum {BOUNDED = 1, FREE, RESTRICTED} QueryType;

struct Metrics {
    size_t estimate;
    size_t intermediateResults;
    int countIntermediateQueries;
    double cost;
    int countRules;
    int countUniqueRules;
    int countIDBPredicates;
    uint8_t boundedness;
};

class Reasoner {
private:

    const uint64_t threshold;

    void cleanBindings(std::vector<Term_t> &bindings, std::vector<uint8_t> * posJoins,
                       TupleTable *input);

    /*TupleTable *getVerifiedBindings(QSQQuery &query,
                                    std::vector<uint8_t> * posJoins,
                                    std::vector<Term_t> *possibleValuesJoins,
                                    EDBLayer &layer, Program &program, DictMgmt *dict,
                                    bool returnOnlyVars);*/

    FCBlock getBlockFromQuery(Literal constantsQuery, Literal &boundQuery,
                                     std::vector<uint8_t> *posJoins,
                                     std::vector<Term_t> *possibleValuesJoins);


    TupleIterator *getIncrReasoningIterator(Literal &query,
            std::vector<uint8_t> * posJoins,
            std::vector<Term_t> *possibleValuesJoins,
            EDBLayer &layer, Program &program,
            bool returnOnlyVars,
            std::vector<uint8_t> *sortByFields);

public:

    Reasoner(const uint64_t threshold) : threshold(threshold) {}

    size_t estimate(Literal &query, std::vector<uint8_t> *posBindings,
                    std::vector<Term_t> *valueBindings, EDBLayer &layer,
                    Program &program, int *countRules, int *countIntQueries, int *countUniqRules, int maxDepth);

    void getMetrics(Literal &query,
	            std::vector<uint8_t> *posBindings,
		    std::vector<Term_t> *valueBindings,
		    EDBLayer &layer,
		    Program &program,
		    Metrics &metrics,
		    int maxDepth);

    ReasoningMode chooseMostEfficientAlgo(Literal &query,
                                          EDBLayer &layer, Program &program,
                                          std::vector<uint8_t> *posBindings,
                                          std::vector<Term_t> *valueBindings, int maxDepth);


    TupleIterator *getIterator(Literal &query,
                                           std::vector<uint8_t> * posJoins,
                                           std::vector<Term_t> *possibleValuesJoins,
                                           EDBLayer &layer, Program &program,
                                           bool returnOnlyVars,
                                           std::vector<uint8_t> *sortByFields);

    TupleIterator *getTopDownIterator(Literal &query,
            std::vector<uint8_t> * posJoins,
            std::vector<Term_t> *possibleValuesJoins,
            EDBLayer &layer, Program &program,
            bool returnOnlyVars,
            std::vector<uint8_t> *sortByFields);

    TupleIterator *getMaterializationIterator(Literal &query,
            std::vector<uint8_t> * posJoins,
            std::vector<Term_t> *possibleValuesJoins,
            EDBLayer &layer, Program &program,
            bool returnOnlyVars,
            std::vector<uint8_t> *sortByFields);

    TupleIterator *getEDBIterator(Literal &query,
            std::vector<uint8_t> * posJoins,
            std::vector<Term_t> *possibleValuesJoins,
            EDBLayer &layer,
            bool returnOnlyVars,
            std::vector<uint8_t> *sortByFields);
    std::vector<std::vector<uint64_t>> getRandomEDBTuples(Literal &query, EDBLayer& edb, uint64_t maxTuples);

    TupleIterator *getMagicIterator(Literal &query,
                                           std::vector<uint8_t> * posJoins,
                                           std::vector<Term_t> *possibleValuesJoins,
                                           EDBLayer &layer, Program &program,
                                           bool returnOnlyVars,
                                           std::vector<uint8_t> *sortByFields);

    static std::shared_ptr<SemiNaiver> fullMaterialization(EDBLayer &layer,
            Program *p, bool opt_intersect, bool opt_filtering, bool opt_threaded,
            int nthreads, int interRuleThreads, bool shuffleRules);

    static std::shared_ptr<SemiNaiver> getSemiNaiver(EDBLayer &layer,
            Program *p, bool opt_intersect, bool opt_filtering, bool opt_threaded,
            int nthreads, int interRuleThreads, bool shuffleRules);

    int getNumberOfIDBPredicates(Literal&, Program&);

    //static int materializationOrOnDemand(const uint64_t matThreshold, std::vector<std::shared_ptr<SPARQLOperator>> &patterns);

    ~Reasoner() {
    }
};
#endif
