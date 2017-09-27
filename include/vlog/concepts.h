#ifndef CONCEPTS_H
#define CONCEPTS_H

#include <vlog/support.h>

#include <string>
#include <inttypes.h>
#include <stdlib.h>
#include <vector>
#include <unordered_map>

/*** PREDICATES ***/
#define EDB 0
#define IDB 1
#define MAX_NPREDS 32768
#define OUT_OF_PREDICATES   32768

typedef uint16_t PredId_t;

class EDBLayer;

using namespace std;

/*** TERMS ***/
class VTerm {
private:
    uint8_t id; //ID != 0 => variable. ID==0 => const value
    uint64_t value;
public:
    VTerm() : id(0), value(0) {}
    VTerm(const uint8_t id, const uint64_t value) : id(id), value(value) {}
    uint8_t getId() const {
        return id;
    }
    uint64_t getValue() const {
        return value;
    }
    void setId(const uint8_t i) {
        id = i;
    }
    void setValue(const uint64_t v) {
        value = v;
    }
    bool isVariable() const {
        return id > 0;
    }
    bool operator==(const VTerm& rhs) const {
        return id == rhs.getId() && value == rhs.getValue();
    }
    bool operator!=(const VTerm& rhs) const {
        return id != rhs.getId() || value != rhs.getValue();
    }
};

/*** TUPLES ***/
#define SIZETUPLE 3
class VTuple {
private:
    const uint8_t sizetuple;
    VTerm terms[SIZETUPLE];
public:
    VTuple(const uint8_t sizetuple) : sizetuple(sizetuple) {}
    size_t getSize() const {
        return sizetuple;
    }
    VTerm get(const int pos) const {
        return terms[pos];
    }
    void set(const VTerm term, const int pos) {
        terms[pos] = term;
    }

    std::vector<std::pair<uint8_t, uint8_t>> getRepeatedVars() const {
        std::vector<std::pair<uint8_t, uint8_t>> output;
        for (uint8_t i = 0; i < sizetuple; ++i) {
            VTerm t1 = get(i);
            if (t1.isVariable()) {
                for (uint8_t j = i + 1; j < sizetuple; ++j) {
                    VTerm t2 = get(j);
                    if (t2.getId() == t1.getId()) {
                        output.push_back(std::make_pair(i, j));
                    }
                }
            }
        }
        return output;
    }

    bool operator==(const VTuple &other) const {
	if (sizetuple == other.sizetuple) {
	    if (terms == other.terms) {
		return true;
	    }
	    for (int i = 0; i < sizetuple; i++) {
		if (terms[i].getId() != other.terms[i].getId()) {
		    return false;
		}
		if (terms[i].getId() == 0) {
		    if (terms[i].getValue() != other.terms[i].getValue()) {
			return false;
		    }
		}
	    }
	    return true;
	}
	return false;
    }
};

struct hash_VTuple {
    size_t operator()(const VTuple &v) const {
	size_t hash = 0;
	int sz = v.getSize();
	for (int i = 0; i < sz; i++) {
	    VTerm term = v.get(i);
	    if (term.isVariable()) {
		hash = (hash + (324723947 + term.getId())) ^93485734985;
	    } else {
		hash = (hash + (324723947 + term.getValue())) ^93485734985;
	    }
	}
	return hash;
    }
};

class Predicate {
private:
    const PredId_t id;
    const uint8_t type;
    const uint8_t adornment;
    const uint8_t card;

public:
    Predicate(const Predicate &pred) : id(pred.id), type(pred.type), adornment(pred.adornment), card(pred.card) {
    }

    Predicate(const Predicate p, const uint8_t adornment) : id(p.id),
        type(p.type), adornment(adornment), card(p.card) {
    }

    Predicate(const PredId_t id, const uint8_t adornment, const uint8_t type,
              const uint8_t card) : id(id), type(type), adornment(adornment),
        card(card) {
    }

    PredId_t getId() const {
        return id;
    }

    uint8_t getType() const {
        return type & 1;
    }

    bool isMagic() const {
        return (type >> 1) != 0;
    }

    uint8_t getAdornment() const {
        return adornment;
    }

    uint8_t getCardinality() const {
        return card;
    }

    static bool isEDB(std::string pred) {
        return pred.at(pred.size() - 1) == 'E';
    }

    static uint8_t calculateAdornment(VTuple &t) {
        uint8_t adornment = 0;
        for (size_t i = 0; i < t.getSize(); ++i) {
            if (!t.get(i).isVariable()) {
                adornment += 1 << i;
            }
        }
        return adornment;
    }

    static uint8_t changeVarToConstInAdornment(const uint8_t adornment, const uint8_t pos) {
        uint8_t shiftedValue = (uint8_t) (1 << pos);
        return adornment | shiftedValue;
    }

    static uint8_t getNFields(uint8_t adornment) {
        uint8_t n = 0;
        for (int i = 0; i < 8; ++i) {
            if (adornment & 1)
                n++;
            adornment >>= 1;
        }
        return n;
    }
};

/*** SUBSTITUTIONS ***/
struct Substitution {
    uint8_t origin;
    VTerm destination;
    Substitution() {}
    Substitution(uint8_t origin, VTerm destination) : origin(origin), destination(destination) {}
};

std::vector<Substitution> concat(std::vector<Substitution>&, std::vector<Substitution>&);
std::vector<Substitution> inverse_concat(std::vector<Substitution>&, std::vector<Substitution>&);
/*** LITERALS ***/
class Program;
class Literal {
private:
    const Predicate pred;
    const VTuple tuple;
public:
    Literal(const Predicate pred, const VTuple tuple) : pred(pred), tuple(tuple) {}

    Predicate getPredicate() const {
        return pred;
    }

    bool isMagic() const {
        return pred.isMagic();
    }

    VTerm getTermAtPos(const int pos) const {
        return tuple.get(pos);
    }

    size_t getTupleSize() const {
        return tuple.getSize();
    }

    VTuple getTuple() const {
        return tuple;
    }

    size_t getNBoundVariables() const {
        return pred.getNFields(pred.getAdornment());
    }

    static int mgu(Substitution *substitutions, const Literal &l, const Literal &m);

    static int subsumes(Substitution *substitutions, const Literal &from, const Literal &to);

    static int getSubstitutionsA2B(
        Substitution *substitutions, const Literal &a, const Literal &b);

    Literal substitutes(Substitution *substitions, const int nsubs) const;

    bool sameVarSequenceAs(const Literal &l) const;

    uint8_t getNVars() const;

    uint8_t getNUniqueVars() const;

    bool hasRepeatedVars() const;

    std::vector<uint8_t> getPosVars() const;

    std::vector<std::pair<uint8_t, uint8_t>> getRepeatedVars() const;

    std::vector<uint8_t> getSharedVars(const std::vector<uint8_t> &vars) const;

    std::vector<uint8_t> getNewVars(std::vector<uint8_t> &vars) const;

    std::vector<uint8_t> getAllVars() const;

    std::string tostring(Program *program, EDBLayer *db) const;

    std::string toprettystring(Program *program, EDBLayer *db) const;

    std::string tostring() const;

    /*Literal operator=(const Literal &other) {
        return Literal(other.pred,other.tuple);
    }*/

    bool operator ==(const Literal &other) const;
};

class Rule {
private:
    const Literal head;
    const std::vector<Literal> body;
    const bool _isRecursive;

    static bool checkRecursion(const Literal &head, const std::vector<Literal> &body);

public:
    bool doesVarAppearsInFollowingPatterns(int startingPattern, uint8_t value);

    Rule(const Literal head, std::vector<Literal> body) : head(head),
        body(body),
        _isRecursive(checkRecursion(head, body)) {
	    checkRule();
    }

    Rule createAdornment(uint8_t headAdornment);

    bool isRecursive() const {
        return this->_isRecursive;
    }

    const Literal &getHead() const {
        return head;
    }

    const std::vector<Literal> &getBody() const {
        return body;
    }

    uint8_t getNIDBPredicates() const {
        uint8_t i = 0;
        for (std::vector<Literal>::const_iterator itr = body.begin(); itr != body.end();
                ++itr) {
            if (itr->getPredicate().getType() == IDB) {
                i++;
            }
        }
        return i;
    }

    uint8_t getNIDBNotMagicPredicates() const {
        uint8_t i = 0;
        for (std::vector<Literal>::const_iterator itr = body.begin(); itr != body.end();
                ++itr) {
            if (itr->getPredicate().getType() == IDB && !itr->getPredicate().isMagic()) {
                i++;
            }
        }
        return i;
    }

    uint8_t getNEDBPredicates() const {
        uint8_t i = 0;
        for (std::vector<Literal>::const_iterator itr = body.begin(); itr != body.end();
                ++itr) {
            if (itr->getPredicate().getType() == EDB) {
                i++;
            }
        }
        return i;
    }
    
    void checkRule() const;

    std::string tostring(Program *program, EDBLayer *db) const;

    std::string toprettystring(Program * program, EDBLayer *db) const;

    std::string tostring() const;

    Rule normalizeVars() const;

    ~Rule() {
    }

    /* How can you have an assignment operator when the fields are const? --Ceriel */
    Rule operator=(const Rule &other) {
        return Rule(other.head, other.body);
    }
};

class Program {
private:
    const uint64_t assignedIds;
    EDBLayer *kb;
    std::vector<Rule> rules[MAX_NPREDS];
    Dictionary dictVariables;

    Dictionary dictPredicates;
    std::unordered_map<PredId_t, uint8_t> cardPredicates;

    Dictionary additionalConstants;

    void parseRule(std::string rule);

    std::string rewriteRDFOWLConstants(std::string input);

public:
    Program(const uint64_t assignedIds,
            EDBLayer *kb);

    Literal parseLiteral(std::string literal);

    void readFromFile(std::string pathFile);

    void readFromString(std::string rules);

    Term_t getIDVar(std::string var);

    PredId_t getPredicateID(std::string &p, const uint8_t card);

    std::string getPredicateName(const PredId_t id);

    Predicate getPredicate(std::string &p);

    Predicate getPredicate(std::string &p, uint8_t adornment);

    Predicate getPredicate(const PredId_t id);

    std::vector<Rule> *getAllRulesByPredicate(PredId_t predid);

    std::string getFromAdditional(Term_t val) {
        return additionalConstants.getRawValue(val);
    }

    void sortRulesByIDBPredicates();

    std::vector<Rule> getAllRules();

    int getNRules() const;

    Program clone() const;

    std::shared_ptr<Program> cloneNew() const;

    void cleanAllRules();

    void addAllRules(std::vector<Rule> &rules);

    bool isPredicateIDB(const PredId_t id);

    std::string getAllPredicates();

    std::vector<std::string> getAllPredicateStrings();

    std::vector<uint8_t> getAllPredicateIds();

    int getNEDBPredicates();

    int getNIDBPredicates();

    std::string tostring();

    static std::string compressRDFOWLConstants(std::string input);

    ~Program() {
    }
};
#endif
