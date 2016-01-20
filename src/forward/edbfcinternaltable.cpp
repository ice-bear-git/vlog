/*
   Copyright (C) 2015 Jacopo Urbani.

   This file is part of Vlog.

   Vlog is free software: you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation, either version 2 of the License, or
   (at your option) any later version.

   Vlog is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with Vlog.  If not, see <http://www.gnu.org/licenses/>.
*/

#include <vlog/fcinttable.h>
#include <vlog/concepts.h>

EDBFCInternalTable::EDBFCInternalTable(const size_t iteration,
                                       const Literal &literal, EDBLayer *layer)
    : iteration(iteration),
      nfields(literal.getNVars()),
      query(QSQQuery(literal)),
      layer(layer) {
    uint8_t j = 0;
    defaultSorting.clear();
    for (uint8_t i = 0; i < literal.getTupleSize(); ++i) {
        Term t = literal.getTermAtPos(i);
        if (t.isVariable()) {
            defaultSorting.push_back(j);
            posFields[j++] = i;
        }
    }
}

size_t EDBFCInternalTable::getNRows() const {
    return layer->getCardinality(&query);
}

bool EDBFCInternalTable::isEmpty() const {
    return layer->isEmpty(&query, NULL, NULL);
}


uint8_t EDBFCInternalTable::getRowSize() const {
    return nfields;
}

FCInternalTableItr *EDBFCInternalTable::getIterator() const {
    EDBFCInternalTableItr *itr = new EDBFCInternalTableItr();
    EDBIterator *edbItr = layer->getSortedIterator(&query, defaultSorting);
    itr->init(iteration, nfields, posFields, edbItr, layer, query.getLiteral());
    return itr;
}

FCInternalTableItr *EDBFCInternalTable::getSortedIterator() const {
    return getIterator();
}

std::shared_ptr<const FCInternalTable> EDBFCInternalTable::merge(std::shared_ptr<const FCInternalTable> t) const {
    assert(false);
    throw 10;
}

bool EDBFCInternalTable::isSorted() const {
    return true;
}

size_t EDBFCInternalTable::estimateNRows(const uint8_t nconstantsToFilter,
        const uint8_t *posConstantsToFilter,
        const Term_t *valuesConstantsToFilter) const {
    if (nconstantsToFilter == 0)
        return layer->getCardinality(&query);

    //Create a new literal adding the constants
    Tuple t = query.getLiteral()->getTuple();
    for (uint8_t i = 0; i < nconstantsToFilter; ++i) {
        t.set(Term(0, valuesConstantsToFilter[i]), posConstantsToFilter[i]);
    }
    const Literal newLiteral(query.getLiteral()->getPredicate(), t);
    const QSQQuery query(newLiteral);
    return layer->estimateCardinality(&query);
}

std::shared_ptr<Column> EDBFCInternalTable::getColumn(
    const uint8_t columnIdx) const {
    //bool unq = query.getLiteral()->getNVars() == 2;
    std::vector<uint8_t> presortFields;
    for(uint8_t i = 0; i < columnIdx; ++i)
        presortFields.push_back(i);

    return std::shared_ptr<Column>(new EDBColumn(*layer,
                                         *query.getLiteral(),
                                         posFields[columnIdx],
                                         presortFields,
                                         //unq));
                                         false));
}

bool EDBFCInternalTable::isColumnConstant(const uint8_t columnid) const {
    return false;
}

Term_t EDBFCInternalTable::getValueConstantColumn(const uint8_t columnid) const {
    throw 10; //should never be called
}

std::shared_ptr<const FCInternalTable> EDBFCInternalTable::filter(
    const uint8_t nPosToCopy, const uint8_t *posVarsToCopy,
    const uint8_t nPosToFilter, const uint8_t *posConstantsToFilter,
    const Term_t *valuesConstantsToFilter, const uint8_t nRepeatedVars,
    const std::pair<uint8_t, uint8_t> *repeatedVars) const {

    //Create a new literal adding the constants
    Tuple t = query.getLiteral()->getTuple();
    for (uint8_t i = 0; i < nPosToFilter; ++i) {
        t.set(Term(0, valuesConstantsToFilter[i]), posConstantsToFilter[i]);
    }
    for (uint8_t i = 0; i < nRepeatedVars; ++i) {
        t.set(t.get(repeatedVars[i].first), repeatedVars[i].second);
    }
    const Literal newLiteral(query.getLiteral()->getPredicate(), t);

    // BOOST_LOG_TRIVIAL(debug) << "EDBFCInternalTable";

    FCInternalTable *filteredTable = new EDBFCInternalTable(iteration,
            newLiteral, layer);
    if (filteredTable->isEmpty()) {
        delete filteredTable;
        return NULL;
    } else {
        assert(filteredTable->getRowSize() == nPosToCopy);
        return std::shared_ptr<const FCInternalTable>(filteredTable);
    }
}

FCInternalTableItr *EDBFCInternalTable::sortBy(const std::vector<uint8_t> &fields) const {
    EDBFCInternalTableItr *itr = new EDBFCInternalTableItr();
    EDBIterator *edbItr = layer->getSortedIterator(&query, fields);
    itr->init(iteration, nfields, posFields, edbItr, layer, query.getLiteral());
    return itr;
}

void EDBFCInternalTable::releaseIterator(FCInternalTableItr *itr) const {
    EDBFCInternalTableItr *castedItr = (EDBFCInternalTableItr*)itr;
    layer->releaseIterator(castedItr->getEDBIterator());
    delete castedItr;
}

EDBFCInternalTable::~EDBFCInternalTable() {
}

void EDBFCInternalTableItr::init(const size_t iteration,
                                 const uint8_t nfields,
                                 uint8_t const *posFields,
                                 EDBIterator *itr,
                                 EDBLayer *layer,
                                 const Literal *query) {
    this->iteration = iteration;
    this->edbItr = itr;
    this->nfields = nfields;
    this->layer = layer;
    this->query = query;
    // BOOST_LOG_TRIVIAL(debug) << "EDB iter: nfields = " << (int) this->nfields;
    for (int i = 0; i < nfields; ++i) {
        // BOOST_LOG_TRIVIAL(debug) << "EDB iter: posfields[" << i << "] = " << (int) posFields[i];
        this->posFields[i] = posFields[i];
    }
    compiled = false;
}

EDBIterator *EDBFCInternalTableItr::getEDBIterator() {
    return edbItr;
}

inline bool EDBFCInternalTableItr::hasNext() {
    bool response = edbItr->hasNext();
    return response;
}

inline void EDBFCInternalTableItr::next() {
    edbItr->next();
    compiled = false;
}

uint8_t EDBFCInternalTableItr::getNColumns() const {
    return nfields;
}

inline Term_t EDBFCInternalTableItr::getCurrentValue(const uint8_t pos) {
    return edbItr->getElementAt(posFields[pos]);
}

std::vector<std::shared_ptr<Column>> EDBFCInternalTableItr::getColumn(
const uint8_t ncolumns, const uint8_t *columns) {
    std::vector<std::shared_ptr<Column>> output;
    //bool unq = query->getNVars() == 1;

    std::vector<uint8_t> presortFields;
    for (uint8_t i = 0; i < ncolumns; ++i) {
        output.push_back(std::shared_ptr<Column>(
                             // new EDBColumn(*layer, *query, columns[i], i, unq)));
                             new EDBColumn(*layer, *query, columns[i], presortFields, false)));
        presortFields.push_back(i);
    }
    return output;
}

std::vector<std::shared_ptr<Column>> EDBFCInternalTableItr::getAllColumns() {
    std::vector<uint8_t> idxs;
    for (uint8_t i = 0; i < nfields; ++i) {
        idxs.push_back(i);
    }
    return getColumn(nfields, &(idxs[0]));
}